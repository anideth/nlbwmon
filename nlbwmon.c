/*
  ISC License

  Copyright (c) 2016-2017, Jo-Philipp Wich <jo@mein.io>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
  REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
  LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
  OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
  PERFORMANCE OF THIS SOFTWARE.
*/


#include <libubox/avl.h>
#include <libubox/list.h>
#include <libubox/uloop.h>
#include <libubox/usock.h>
#include <libubox/utils.h>

#include <endian.h>
#include <arpa/inet.h>
#include <netinet/ether.h>

#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>

#include <endian.h>

#include "nlbwmon.h"
#include "neigh.h"
#include "database.h"
#include "protocol.h"
#include "nfnetlink.h"
#include "flow_export.h"
#include "flow_json.h"
#include "flow_tcp.h"
#include "subnets.h"
#include "socket.h"
#include "client.h"
#include "utils.h"

#ifdef HAVE_ULOOP_INTERVAL
# define uloop_timer_type uloop_interval
# define uloop_timer_set(tm, ms) uloop_interval_set(tm, ms)
# define uloop_timer_reset(tm, ms)
#else
# define uloop_timer_type uloop_timeout
# define uloop_timer_set(tm, ms) uloop_timeout_set(tm, ms)
# define uloop_timer_reset(tm, ms) uloop_timeout_set(tm, ms)
#endif

static struct uloop_timer_type commit_tm = { };
static struct uloop_timer_type refresh_tm = { };
static struct uloop_timeout flow_tcp_tm = { };
static struct flow_tcp *flow_transport;
static uint64_t flow_tcp_last_dropped;
static time_t flow_tcp_last_warning;

static void flow_completed(const struct flow_record *record, void *ctx)
{
	char line[1024];
	int length = flow_json_format(record, line, sizeof(line));
	if (length > 0)
		flow_tcp_enqueue(ctx, line, (size_t)length);
}

static void flow_tcp_tick(struct uloop_timeout *tm)
{
	uint64_t dropped;
	time_t now;

	flow_tcp_step(flow_transport);
	dropped = flow_tcp_dropped(flow_transport);
	now = time(NULL);
	if (dropped != flow_tcp_last_dropped &&
	    (flow_tcp_last_warning == 0 || now - flow_tcp_last_warning >= 60)) {
		fprintf(stderr, "Flow TCP queue: dropped=%" PRIu64 ", queued=%zu\\n",
		        dropped, flow_tcp_queued(flow_transport));
		flow_tcp_last_dropped = dropped;
		flow_tcp_last_warning = now;
	}
	uloop_timeout_set(tm, 100);
}


struct options opt = {
	.commit_interval = 86400,
	.refresh_interval = 30,

	.netlink_buffer_size = 524288,

	.tempdir = "/tmp",
	.socket = "/var/run/nlbwmon.sock",
	.protocol_db = "/usr/share/nlbwmon/protocols",

	.db = {
		.directory = "/usr/share/nlbwmon/db"
	}
};


static void save_persistent(uint32_t timestamp)
{
	int err;

	err = database_save(gdbh, opt.db.directory, timestamp, opt.db.compress);

	if (err == -EEXIST) {
		fprintf(stderr, "Existing database found, merging values\n");

		err = database_load(gdbh, opt.db.directory, timestamp);

		if (err) {
			fprintf(stderr, "Unable to load existing database: %s\n",
			        strerror(-err));
		}
	}

	err = database_save(gdbh, opt.db.directory, timestamp, opt.db.compress);

	if (err) {
		fprintf(stderr, "Unable to save database: %s\n",
		        strerror(-err));
	}
}

static void handle_shutdown(int sig)
{
	char path[256];
	uint32_t timestamp = interval_timestamp(&opt.archive_interval, 0);

	save_persistent(timestamp);

	if (sig == SIGTERM) {
		snprintf(path, sizeof(path), "%s/0.db", opt.tempdir);
		unlink(path);
	}
	else {
		database_save(gdbh, opt.tempdir, 0, false);
	}

	uloop_done();
	exit(0);
}

static void
handle_commit(struct uloop_timer_type *tm)
{
	uint32_t timestamp = interval_timestamp(&opt.archive_interval, 0);

	uloop_timer_reset(tm, opt.commit_interval * 1000);
	save_persistent(timestamp);
}

static void
handle_refresh(struct uloop_timer_type *tm)
{
	int err;

	uloop_timer_reset(tm, opt.refresh_interval * 1000);

	err = database_archive(gdbh);

	/* database successfully wrapped around and triggered a ct dump */
	if (err == -ESTALE)
		return;

	/* fatal error during database archiving */
	if (err != 0) {
		fprintf(stderr, "Unable to archive database: %s\n",
				strerror(-err));
		return;
	}

	err = nfnetlink_dump(false);

	if (err) {
		fprintf(stderr, "Unable to dump conntrack: %s\n",
		        strerror(-err));
		return;
	}

	database_save(gdbh, opt.tempdir, 0, false);
}

static int
parse_timearg(const char *val, time_t *dest)
{
	char *e;

	*dest = strtoul(val, &e, 10);

	if (e == val)
		return -EINVAL;

	switch (*e)
	{
	case 'w':
		*dest *= 604800;
		break;

	case 'd':
		*dest *= 86400;
		break;

	case 'h':
		*dest *= 3600;
		break;

	case 'm':
		*dest *= 60;
		break;

	case 's':
	case 0:
		break;

	default:
		return -EINVAL;
	}

	if (*dest > 0x7fffffff / 1000)
		return -ERANGE;

	return 0;
}

static int
server_main(int argc, char **argv)
{
	struct sigaction sa = { .sa_handler = handle_shutdown };
	uint32_t timestamp;
	int optchr, err;
	char *e;
	struct flow_config flow_cfg = { .enabled = false, .mark = 0x40000000U, .mark_mask = 0x40000000U, .max_flows = 16384 };
	struct flow_tcp_config tcp_cfg = { .host = NULL, .port = 0, .queue_size = 1024, .reconnect_interval = 5 };

	while ((optchr = getopt(argc, argv, "b:i:r:s:o:p:G:I:L:PZE:M:K:F:H:T:Q:R:")) > -1) {
		switch (optchr) {
		case 'b':
			opt.netlink_buffer_size = (int)strtol(optarg, &e, 0);
			if (e == optarg || *e || opt.netlink_buffer_size < 32768) {
				fprintf(stderr, "Invalid netlink buffer size '%s'\n",
				        optarg);
				return 1;
			}
			break;

		case 'i':
			err = parse_timearg(optarg, &opt.commit_interval);
			if (err) {
				fprintf(stderr, "Invalid commit interval '%s': %s\n",
				        optarg, strerror(-err));
				return 1;
			}
			break;

		case 'r':
			err = parse_timearg(optarg, &opt.refresh_interval);
			if (err) {
				fprintf(stderr, "Invalid refresh interval '%s': %s\n",
				        optarg, strerror(-err));
				return 1;
			}
			break;

		case 's':
			err = add_subnet(optarg);
			if (err) {
				fprintf(stderr, "Invalid subnet '%s': %s\n",
				        optarg, strerror(-err));
				return 1;
			}
			break;

		case 'o':
			opt.db.directory = optarg;
			break;

		case 'p':
			opt.protocol_db = optarg;
			break;

		case 'G':
			opt.db.generations = strtoul(optarg, &e, 10);
			if (e == optarg || *e != 0) {
				fprintf(stderr, "Invalid generations argument: %s\n", optarg);
				return 1;
			}
			break;

		case 'I':
			err = interval_pton(optarg, &opt.archive_interval);
			if (err < 0) {
				fprintf(stderr, "Invalid interval '%s': %s\n",
				        optarg, strerror(-err));
				exit(1);
			}
			break;

		case 'P':
			opt.db.prealloc = true;
			break;

		case 'L':
			opt.db.limit = strtoul(optarg, &e, 10);
			if (e == optarg || *e != 0) {
				fprintf(stderr, "Invalid limit argument: %s\n", optarg);
				return 1;
			}
			break;

		case 'E':
			if (strcmp(optarg, "0") && strcmp(optarg, "1")) {
				fprintf(stderr, "Invalid flow enabled flag '%s'\n", optarg);
				return 1;
			}
			flow_cfg.enabled = (*optarg == '1');
			break;

		case 'M':
		case 'K': {
			unsigned long value;
			errno = 0;
			value = strtoul(optarg, &e, 0);
			if (errno || e == optarg || *e || value > UINT32_MAX) {
				fprintf(stderr, "Invalid flow mark/mask '%s'\n", optarg);
				return 1;
			}
			if (optchr == 'M')
				flow_cfg.mark = (uint32_t)value;
			else
				flow_cfg.mark_mask = (uint32_t)value;
			break;
		}

		case 'F': {
			unsigned long value;
			errno = 0;
			value = strtoul(optarg, &e, 10);
			if (errno || e == optarg || *e || !value || value > 1048576) {
				fprintf(stderr, "Invalid max flows '%s'\n", optarg);
				return 1;
			}
			flow_cfg.max_flows = (size_t)value;
			break;
		}

		case 'H':
			tcp_cfg.host = optarg;
			break;
		case 'T':
		case 'Q':
		case 'R': {
			unsigned long value;
			errno = 0;
			value = strtoul(optarg, &e, 10);
			if (errno || e == optarg || *e || !value ||
			    (optchr == 'T' && value > 65535) ||
			    (optchr == 'Q' && value > 65536) ||
			    (optchr == 'R' && value > 3600)) {
				fprintf(stderr, "Invalid flow TCP option '%s'\n", optarg);
				return 1;
			}
			if (optchr == 'T') tcp_cfg.port = (uint16_t)value;
			else if (optchr == 'Q') tcp_cfg.queue_size = (size_t)value;
			else tcp_cfg.reconnect_interval = (unsigned int)value;
			break;
		}
		case 'Z':
			opt.db.compress = true;
			break;
		}
	}

	if (!opt.archive_interval.type) {
		fprintf(stderr, "No interval specified; assuming 1st of month\n");
		interval_pton("1", &opt.archive_interval);
	}

	err = flow_configure(&flow_cfg);
	if (err) {
		fprintf(stderr, "Invalid flow configuration: %s\n", strerror(-err));
		return 1;
	}

	if (flow_cfg.enabled && tcp_cfg.host && tcp_cfg.port) {
		flow_transport = flow_tcp_create(&tcp_cfg);
		if (!flow_transport) {
			fprintf(stderr, "Unable to configure flow TCP transport\n");
			return 1;
		}
		flow_set_complete_callback(flow_completed, flow_transport);
	}

	uloop_init();

	err = rmkdir(opt.db.directory);

	if (err) {
		fprintf(stderr, "Unable to create database directory: %s\n",
		        strerror(-err));
		return 1;
	}

	database_cleanup();

	gdbh = database_init(&opt.archive_interval, opt.db.prealloc, opt.db.limit);

	if (!gdbh) {
		fprintf(stderr, "Unable to allocate memory database: %s\n",
		        strerror(ENOMEM));
		return 1;
	}

	err = database_load(gdbh, opt.tempdir, 0);

	if (err == -ENOENT) {
		timestamp = interval_timestamp(&opt.archive_interval, 0);
		err = database_load(gdbh, opt.db.directory, timestamp);
	}

	if (err != 0 && err != -ENOENT) {
		fprintf(stderr, "Unable to restore database: %s\n",
		        strerror(-err));
		exit(1);
	}

	err = init_protocols(opt.protocol_db);

	if (err) {
		fprintf(stderr, "Unable to read protocol list %s: %s\n",
		        opt.protocol_db, strerror(-err));
		exit(1);
	}

	err = nfnetlink_connect(opt.netlink_buffer_size);

	if (err) {
		fprintf(stderr, "Unable to connect nfnetlink: %s\n",
		        strerror(-err));
		exit(1);
	}

	err = socket_init(opt.socket);

	if (err) {
		fprintf(stderr, "Unable to create control socket: %s\n",
		        strerror(-err));
		exit(1);
	}

	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);

	commit_tm.cb = handle_commit;
	uloop_timer_set(&commit_tm, opt.commit_interval * 1000);

	refresh_tm.cb = handle_refresh;
	uloop_timer_set(&refresh_tm, opt.refresh_interval * 1000);

	if (flow_transport) {
		flow_tcp_tm.cb = flow_tcp_tick;
		uloop_timeout_set(&flow_tcp_tm, 100);
	}

	uloop_run();
	flow_set_complete_callback(NULL, NULL);
	flow_tcp_destroy(flow_transport);
	flow_transport = NULL;

	return 0;
}

int
main(int argc, char **argv)
{
	const char *name = basename(argv[0]);

	if (!strcmp(name, "nlbw"))
		return client_main(argc, argv);

	return server_main(argc, argv);
}
