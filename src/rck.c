#include <stdlib.h>
#include <string.h>
#include "rck.h"
#include "rckpriv.h"

static PRBool ok = PR_FALSE;

PRBool rck_init(void) {
	if (ok) {
		return PR_TRUE;
	} else {
		PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
		irck_xpm_init();
		return PR_TRUE;
	}
	
	return PR_FALSE;
}

void rck_deinit(void) {
	if (ok) {
		irck_xpm_deinit();
		PR_Cleanup();
	}
}

PRBool rck_strprefix(const char *string, const char *prefix) {
	if (!strncmp(prefix, string, strlen(prefix))) {
		return PR_TRUE;
	} 
	
	return PR_FALSE;
}

void rck_free(void *ptr) {
	free(ptr);
}

unsigned int rck_version(unsigned int *minor) {
	if (minor) {
		*minor = RCK_MINOR_VERSION;
	}

	return RCK_MAJOR_VERSION;
}

PRBool rck_strsuffix(const char *string, const char *suffix) {
    size_t string_len, suffix_len;
    
    string_len = strlen(string);
    suffix_len = strlen(suffix);
    
    if (suffix_len > string_len) {
        return PR_FALSE;
    }
    
	if (!strcmp(string + (string_len - suffix_len), suffix)) {
		return PR_TRUE;
	}
	
	return PR_FALSE;
}

