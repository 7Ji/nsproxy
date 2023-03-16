#pragma once
#include "common.h"
#include "loop.h"

struct sk_ops *tcpdns_create(struct loopctx *loop,
                             void (*userev)(void *userp, unsigned int event),
                             void *userp);
