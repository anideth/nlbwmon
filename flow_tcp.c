#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include "flow_tcp.h"

#define FLOW_TCP_MAX_RECORD 2048
#define FLOW_TCP_MAX_QUEUE 65536

struct flow_tcp_item {
    struct flow_tcp_item *next;
    size_t length;
    size_t offset;
    char data[];
};

struct flow_tcp {
    struct sockaddr_storage address;
    socklen_t address_len;
    int fd;
    bool connecting;
    unsigned int reconnect_interval;
    time_t retry_at;
    size_t queue_limit;
    size_t queued;
    uint64_t dropped;
    struct flow_tcp_item *head;
    struct flow_tcp_item *tail;
};

static time_t monotonic_seconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now))
        return 0;
    return now.tv_sec;
}

static void disconnect_tcp(struct flow_tcp *tcp)
{
    if (tcp->fd >= 0)
        close(tcp->fd);
    tcp->fd = -1;
    tcp->connecting = false;
    tcp->retry_at = monotonic_seconds() + tcp->reconnect_interval;
    /* The receiver may have accepted a partial record. Retry whole JSONL line. */
    if (tcp->head)
        tcp->head->offset = 0;
}

struct flow_tcp *flow_tcp_create(const struct flow_tcp_config *config)
{
    struct flow_tcp *tcp;
    struct addrinfo hints = { 0 }, *addresses = NULL, *a;
    char port[8];

    if (!config || !config->host || !*config->host || !config->port ||
        !config->queue_size || config->queue_size > FLOW_TCP_MAX_QUEUE ||
        !config->reconnect_interval || config->reconnect_interval > 3600)
        return NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    snprintf(port, sizeof(port), "%u", (unsigned)config->port);
    if (getaddrinfo(config->host, port, &hints, &addresses))
        return NULL;

    tcp = calloc(1, sizeof(*tcp));
    if (!tcp) {
        freeaddrinfo(addresses);
        return NULL;
    }

    tcp->fd = -1;
    tcp->queue_limit = config->queue_size;
    tcp->reconnect_interval = config->reconnect_interval;
    for (a = addresses; a; a = a->ai_next) {
        if (a->ai_addrlen <= sizeof(tcp->address)) {
            memcpy(&tcp->address, a->ai_addr, a->ai_addrlen);
            tcp->address_len = a->ai_addrlen;
            break;
        }
    }
    freeaddrinfo(addresses);
    if (!tcp->address_len) {
        free(tcp);
        return NULL;
    }
    return tcp;
}

void flow_tcp_destroy(struct flow_tcp *tcp)
{
    struct flow_tcp_item *item, *next;
    if (!tcp)
        return;
    if (tcp->fd >= 0)
        close(tcp->fd);
    for (item = tcp->head; item; item = next) {
        next = item->next;
        free(item);
    }
    free(tcp);
}

int flow_tcp_enqueue(struct flow_tcp *tcp, const char *json, size_t length)
{
    struct flow_tcp_item *item;

    if (!tcp || !json || !length || length > FLOW_TCP_MAX_RECORD ||
        json[length - 1] != '\n')
        return -EINVAL;

    if (tcp->queued >= tcp->queue_limit) {
        tcp->dropped++;
        return -ENOSPC;
    }

    item = malloc(sizeof(*item) + length);
    if (!item) {
        tcp->dropped++;
        return -ENOMEM;
    }
    item->next = NULL;
    item->length = length;
    item->offset = 0;
    memcpy(item->data, json, length);
    if (tcp->tail)
        tcp->tail->next = item;
    else
        tcp->head = item;
    tcp->tail = item;
    tcp->queued++;
    return 0;
}

size_t flow_tcp_queued(const struct flow_tcp *tcp)
{
    return tcp ? tcp->queued : 0;
}

uint64_t flow_tcp_dropped(const struct flow_tcp *tcp)
{
    return tcp ? tcp->dropped : 0;
}

void flow_tcp_step(struct flow_tcp *tcp)
{
    int error, fd, flags;
    socklen_t len = sizeof(error);
    ssize_t n;
    unsigned int budget = 32;

    if (!tcp)
        return;

    if (tcp->fd < 0) {
        if (tcp->retry_at > monotonic_seconds())
            return;
        fd = socket(tcp->address.ss_family, SOCK_STREAM, IPPROTO_TCP);
        if (fd < 0) {
            tcp->retry_at = monotonic_seconds() + tcp->reconnect_interval;
            return;
        }
        flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(fd);
            tcp->retry_at = monotonic_seconds() + tcp->reconnect_interval;
            return;
        }
        tcp->fd = fd;
        if (connect(fd, (struct sockaddr *)&tcp->address, tcp->address_len) < 0) {
            if (errno != EINPROGRESS) {
                disconnect_tcp(tcp);
                return;
            }
            tcp->connecting = true;
        }
    }

    if (tcp->connecting) {
        /* SO_ERROR is meaningful only after the socket is writable. */
        fd_set writefds;
        struct timeval timeout = { 0, 0 };
        FD_ZERO(&writefds);
        FD_SET(tcp->fd, &writefds);
        n = select(tcp->fd + 1, NULL, &writefds, NULL, &timeout);
        if (n == 0 || (n < 0 && errno == EINTR))
            return;
        if (n < 0 || getsockopt(tcp->fd, SOL_SOCKET, SO_ERROR, &error, &len) ||
            error) {
            disconnect_tcp(tcp);
            return;
        }
        tcp->connecting = false;
    }

    while (tcp->head && budget--) {
        struct flow_tcp_item *item = tcp->head;
        n = send(tcp->fd, item->data + item->offset,
                 item->length - item->offset, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        if (n <= 0) {
            disconnect_tcp(tcp);
            return;
        }
        item->offset += (size_t)n;
        if (item->offset == item->length) {
            tcp->head = item->next;
            if (!tcp->head)
                tcp->tail = NULL;
            tcp->queued--;
            free(item);
        }
    }
}
