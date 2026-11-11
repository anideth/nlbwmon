#ifndef FLOW_JSON_H
#define FLOW_JSON_H

#include <stddef.h>
#include "flow_export.h"

/* Returns bytes written (excluding NUL), or a negative errno value. */
int flow_json_format(const struct flow_record *record, char *buffer, size_t size);

#endif
