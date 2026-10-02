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

PRBool rck_strcaseprefix(const char *string, const char *prefix) {
	if (!PL_strncasecmp(prefix, string, strlen(prefix))) {
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

PRBool rck_strcasesuffix(const char *string, const char *suffix) {
    size_t string_len, suffix_len;
    
    string_len = strlen(string);
    suffix_len = strlen(suffix);
    
    if (suffix_len > string_len) {
        return PR_FALSE;
    }
    
	if (!PL_strcasecmp(string + (string_len - suffix_len), suffix)) {
		return PR_TRUE;
	}
	
	return PR_FALSE;
}

char *rck_expandenv(const char *src) {
	const char *p;
	char *dst;
    size_t cap;
    size_t len;

	cap = 256;
	len = 0;
    if (!src) {
		return NULL;
	}

    dst = PR_Malloc(cap);
    if (!dst) {
		return NULL;
	}
	
    p = src;
    while (*p) {
        if (*p == '$') {
            const char *start;
            size_t name_len;
            
            p++;
            start = p;
            
            while (*p && (rck_isalnum_ascii((unsigned char)*p) || *p == '_')) {
                p++;
            }
            
            name_len = p - start;
            if (name_len > 0) {
                char var_name[256];
                
                if (name_len < sizeof(var_name)) {
					const char *val;
					
                    memcpy(var_name, start, name_len);
                    var_name[name_len] = '\0';

                    val = PR_GetEnv(var_name);
                    if (val) {
                        size_t val_len;
                        
						val_len = strlen(val);
                        while (len + val_len >= cap) {
                            cap *= 2;
                            dst = PR_Realloc(dst, cap);
                            if (!dst) {
								 return NULL;
							}
                        }
                        memcpy(dst + len, val, val_len);
                        len += val_len;
                    }
                }
                continue; 
            } else {
                p--; 
            }
        }
        if (len + 1 >= cap) {
            cap *= 2;
			dst = PR_Realloc(dst, cap);
			if (!dst) {
				return NULL;
			}
        }
        dst[len++] = *p++;
    }

    dst[len] = '\0';
    return dst;
}
