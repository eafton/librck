#include "rck.h"

#ifndef RCK_PRIV_H
#define RCK_PRIV_H

void irck_xpm_init();
void irck_xpm_deinit();
void irck_fallback_text_deinit();
PRIntn PR_CALLBACK irck_compare_uint32(const void *v1, const void *v2);

#endif
