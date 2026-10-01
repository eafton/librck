#include <stdlib.h>
#include <string.h>
#include <nspr.h>
#include <plstr.h>
#include <plhash.h>
#include <prenv.h>
#include "rck.h"

struct _RCKXDGBDCache {
	char *dirs[RCK_XDG_BD_DIRECTORY_COUNT];
	
	char **dir_lists[RCK_XDG_BD_DIRECTORY_LIST_COUNT];
	unsigned int dir_lists_sz[RCK_XDG_BD_DIRECTORY_LIST_COUNT];
	
	char **dir_lists_searchable[RCK_XDG_BD_DIRECTORY_LIST_COUNT];
	unsigned int dir_lists_searchable_sz[RCK_XDG_BD_DIRECTORY_LIST_COUNT];
	
	PLHashTable *free_table;
};

static void bdstrfree(RCKXDGBDCache *cache, const char *in) {
	void (*free_func)(void *);
	
	free_func = PL_HashTableLookup(cache->free_table, in);
	if (!free_func) {
		PL_HashTableRemove(cache->free_table, in);
		free_func = free;
	}
	
	free_func((void*)in);
}

static char *bdstrdup(RCKXDGBDCache *cache, const char *in) {
	char *value;
	
	value = PL_strdup(in);
	if (value) {
		PL_HashTableAdd(cache->free_table, value, PL_strfree);
		return value;
	} else {
		return NULL;
	}
}

static void destroy(RCKXDGBDCache *cache) {
	unsigned int i;
	unsigned int j;

	for (i = 0; i < RCK_XDG_BD_DIRECTORY_COUNT; i++) {
		if (cache->dirs[i]) {
			bdstrfree(cache, cache->dirs[i]);
		}
	}
	
	for (i = 0; i < RCK_XDG_BD_DIRECTORY_LIST_COUNT; i++) {
		if (cache->dir_lists[i]) {
			for (j = 0; j < cache->dir_lists_sz[i]; j++) {
				if (cache->dir_lists[i][j]) {
					bdstrfree(cache, cache->dir_lists[i][j]);
				}
			}

			free(cache->dir_lists[i]);
		}
	}
	
	for (i = 0; i < RCK_XDG_BD_DIRECTORY_LIST_COUNT; i++) {
		if (cache->dir_lists_searchable[i]) {
			for (j = 0; j < cache->dir_lists_searchable_sz[i]; j++) {
				if (cache->dir_lists_searchable[i][j]) {
					bdstrfree(cache, cache->dir_lists_searchable[i][j]);
				}
			}

			free(cache->dir_lists_searchable[i]);
		}
	}
}

static void populate(RCKXDGBDCache *cache) {
	const char *envvar;
	
	envvar = PR_GetEnv("XDG_DATA_HOME");
	if (envvar && strcmp(envvar, "")) {
		cache->dirs[RCK_XDG_BD_DATA_HOME] = bdstrdup(cache, envvar);
	} else {
		envvar = PR_GetEnv("HOME");
		if (envvar) {
			#define LOCAL_SHARE "/.local/share"
			
			cache->dirs[RCK_XDG_BD_DATA_HOME] = malloc(strlen(envvar) + strlen(LOCAL_SHARE) + 1);
			if (cache->dirs[RCK_XDG_BD_DATA_HOME]) {
				strcpy(cache->dirs[RCK_XDG_BD_DATA_HOME], envvar);
				strcat(cache->dirs[RCK_XDG_BD_DATA_HOME], LOCAL_SHARE);
			}
		} else {
			cache->dirs[RCK_XDG_BD_DATA_HOME] = NULL;
		}
	}

	envvar = PR_GetEnv("XDG_CONFIG_HOME");
	if (envvar && strcmp(envvar, "")) {
		cache->dirs[RCK_XDG_BD_CONFIG_HOME] = bdstrdup(cache, envvar);
	} else {
		envvar = PR_GetEnv("HOME");
		if (envvar) {
			#define CONFIG "/.config"
			
			cache->dirs[RCK_XDG_BD_CONFIG_HOME] = malloc(strlen(envvar) + strlen(CONFIG) + 1);
			if (cache->dirs[RCK_XDG_BD_CONFIG_HOME]) {
				strcpy(cache->dirs[RCK_XDG_BD_CONFIG_HOME], envvar);
				strcat(cache->dirs[RCK_XDG_BD_CONFIG_HOME], CONFIG);
			}
		} else {
			cache->dirs[RCK_XDG_BD_CONFIG_HOME] = NULL;
		}
	}
	
	envvar = PR_GetEnv("XDG_STATE_HOME");
	if (envvar && strcmp(envvar, "")) {
		cache->dirs[RCK_XDG_BD_STATE_HOME] = bdstrdup(cache, envvar);
	} else {
		envvar = PR_GetEnv("HOME");
		if (envvar) {
			#define LOCAL_STATE "/.local/state"
			
			cache->dirs[RCK_XDG_BD_STATE_HOME] = malloc(strlen(envvar) + strlen(LOCAL_STATE) + 1);
			if (cache->dirs[RCK_XDG_BD_STATE_HOME]) {
				strcpy(cache->dirs[RCK_XDG_BD_STATE_HOME], envvar);
				strcat(cache->dirs[RCK_XDG_BD_STATE_HOME], LOCAL_STATE);
			}
		} else {
			cache->dirs[RCK_XDG_BD_STATE_HOME] = NULL;
		}
	}

	envvar = PR_GetEnv("XDG_CACHE_HOME");
	if (envvar && strcmp(envvar, "")) {
		cache->dirs[RCK_XDG_BD_CACHE_HOME] = bdstrdup(cache, envvar);
	} else {
		envvar = PR_GetEnv("HOME");
		if (envvar) {
			#define CACHE "/.cache"
			
			cache->dirs[RCK_XDG_BD_CACHE_HOME] = malloc(strlen(envvar) + strlen(CACHE) + 1);
			if (cache->dirs[RCK_XDG_BD_CACHE_HOME]) {
				strcpy(cache->dirs[RCK_XDG_BD_CACHE_HOME], envvar);
				strcat(cache->dirs[RCK_XDG_BD_CACHE_HOME], CACHE);
			}
		} else {
			cache->dirs[RCK_XDG_BD_CACHE_HOME] = NULL;
		}
	}

	envvar = PR_GetEnv("XDG_RUNTIME_DIR");
	if (envvar && strcmp(envvar, "")) {
		cache->dirs[RCK_XDG_BD_RUNTIME_DIR] = bdstrdup(cache, envvar);
	} else {
		cache->dirs[RCK_XDG_BD_RUNTIME_DIR] = NULL;
	}
	
	envvar = PR_GetEnv("XDG_DATA_DIRS");
	if (envvar && strcmp(envvar, "")) {
		char *input;
		char *kstate;
		char *token;
		unsigned int i;
		
		cache->dir_lists_sz[RCK_XDG_BD_DATA_DIRS] = 0;
		input = bdstrdup(cache, envvar);
		kstate = input;
		while ((token = PL_strtok_r(kstate, ":", &kstate))) {
			cache->dir_lists_sz[RCK_XDG_BD_DATA_DIRS]++;

			if (cache->dir_lists_sz[RCK_XDG_BD_DATA_DIRS] == 1) {
				cache->dir_lists[RCK_XDG_BD_DATA_DIRS] = malloc(sizeof(char *));
			} else {
				cache->dir_lists[RCK_XDG_BD_DATA_DIRS] = realloc(cache->dir_lists[RCK_XDG_BD_DATA_DIRS], cache->dir_lists_sz[RCK_XDG_BD_DATA_DIRS] * sizeof(char *));
			}
			
			cache->dir_lists[RCK_XDG_BD_DATA_DIRS][cache->dir_lists_sz[RCK_XDG_BD_DATA_DIRS] - 1] = bdstrdup(cache, token);
		}
		bdstrfree(cache, input);
		
		cache->dir_lists_searchable_sz[RCK_XDG_BD_DATA_DIRS] = cache->dir_lists_sz[RCK_XDG_BD_DATA_DIRS] + 1;
		cache->dir_lists_searchable[RCK_XDG_BD_DATA_DIRS] = calloc(cache->dir_lists_searchable_sz[RCK_XDG_BD_DATA_DIRS], sizeof(char *));
		cache->dir_lists_searchable[RCK_XDG_BD_DATA_DIRS][0] = bdstrdup(cache, cache->dirs[RCK_XDG_BD_DATA_HOME]);
		for (i = 1; i < cache->dir_lists_sz[RCK_XDG_BD_DATA_DIRS] + 1; i++) {
			cache->dir_lists_searchable[RCK_XDG_BD_DATA_DIRS][i] = bdstrdup(cache, cache->dir_lists[RCK_XDG_BD_DATA_DIRS][i - 1]);
		}
	} else {
		cache->dir_lists_sz[RCK_XDG_BD_DATA_DIRS] = 2;
		cache->dir_lists[RCK_XDG_BD_DATA_DIRS] = calloc(cache->dir_lists_sz[RCK_XDG_BD_DATA_DIRS], sizeof(char *));
		cache->dir_lists[RCK_XDG_BD_DATA_DIRS][0] = bdstrdup(cache, "/usr/local/share/");
		cache->dir_lists[RCK_XDG_BD_DATA_DIRS][1] = bdstrdup(cache, "/usr/share/");
		
		cache->dir_lists_searchable_sz[RCK_XDG_BD_DATA_DIRS] = 3;
		cache->dir_lists_searchable[RCK_XDG_BD_DATA_DIRS] = calloc(cache->dir_lists_searchable_sz[RCK_XDG_BD_DATA_DIRS], sizeof(char *));
		cache->dir_lists_searchable[RCK_XDG_BD_DATA_DIRS][0] = bdstrdup(cache, cache->dirs[RCK_XDG_BD_DATA_HOME]);
		cache->dir_lists_searchable[RCK_XDG_BD_DATA_DIRS][1] = bdstrdup(cache, "/usr/local/share/");
		cache->dir_lists_searchable[RCK_XDG_BD_DATA_DIRS][2] = bdstrdup(cache, "/usr/share/");
	}

	envvar = PR_GetEnv("XDG_CONFIG_DIRS");
	if (envvar && strcmp(envvar, "")) {
		char *input;
		char *kstate;
		char *token;
		unsigned int i;
		
		cache->dir_lists_sz[RCK_XDG_BD_CONFIG_DIRS] = 0;
		input = bdstrdup(cache, envvar);
		kstate = input;
		while ((token = PL_strtok_r(kstate, ":", &kstate))) {
			cache->dir_lists_sz[RCK_XDG_BD_CONFIG_DIRS]++;

			if (cache->dir_lists_sz[RCK_XDG_BD_CONFIG_DIRS] == 1) {
				cache->dir_lists[RCK_XDG_BD_CONFIG_DIRS] = malloc(sizeof(char *));
			} else {
				cache->dir_lists[RCK_XDG_BD_CONFIG_DIRS] = realloc(cache->dir_lists[RCK_XDG_BD_CONFIG_DIRS], cache->dir_lists_sz[RCK_XDG_BD_CONFIG_DIRS] * sizeof(char *));
			}
			
			cache->dir_lists[RCK_XDG_BD_CONFIG_DIRS][cache->dir_lists_sz[RCK_XDG_BD_CONFIG_DIRS] - 1] = bdstrdup(cache, token);
		}
		bdstrfree(cache, input);
		
		cache->dir_lists_searchable_sz[RCK_XDG_BD_CONFIG_DIRS] = cache->dir_lists_sz[RCK_XDG_BD_CONFIG_DIRS] + 1;
		cache->dir_lists_searchable[RCK_XDG_BD_CONFIG_DIRS] = calloc(cache->dir_lists_searchable_sz[RCK_XDG_BD_CONFIG_DIRS], sizeof(char *));
		cache->dir_lists_searchable[RCK_XDG_BD_CONFIG_DIRS][0] = bdstrdup(cache, cache->dirs[RCK_XDG_BD_CONFIG_HOME]);
		for (i = 1; i < cache->dir_lists_sz[RCK_XDG_BD_CONFIG_DIRS] + 1; i++) {
			cache->dir_lists_searchable[RCK_XDG_BD_CONFIG_DIRS][i] = bdstrdup(cache, cache->dir_lists[RCK_XDG_BD_CONFIG_DIRS][i - 1]);
		}
		
		for (i = 0; i < cache->dir_lists_searchable_sz[RCK_XDG_BD_CONFIG_DIRS]; i++) {
			puts(cache->dir_lists_searchable[RCK_XDG_BD_CONFIG_DIRS][i]);
		}
	} else {
		cache->dir_lists_sz[RCK_XDG_BD_CONFIG_DIRS] = 1;
		cache->dir_lists[RCK_XDG_BD_CONFIG_DIRS] = calloc(cache->dir_lists_sz[RCK_XDG_BD_CONFIG_DIRS], sizeof(char *));
		cache->dir_lists[RCK_XDG_BD_CONFIG_DIRS][0] = bdstrdup(cache, "/etc/xdg");
		
		cache->dir_lists_searchable_sz[RCK_XDG_BD_CONFIG_DIRS] = 2;
		cache->dir_lists_searchable[RCK_XDG_BD_CONFIG_DIRS] = calloc(cache->dir_lists_searchable_sz[RCK_XDG_BD_CONFIG_DIRS], sizeof(char *));
		cache->dir_lists_searchable[RCK_XDG_BD_CONFIG_DIRS][0] = bdstrdup(cache, cache->dirs[RCK_XDG_BD_CONFIG_HOME]);
		cache->dir_lists_searchable[RCK_XDG_BD_CONFIG_DIRS][1] = bdstrdup(cache, "/etc/xdg");
	}
}

static PLHashNumber PR_CALLBACK hash_ptr(const void *key) {
    PRUword val = 
    
    val = (PRUword)key;
#if PR_BYTES_PER_WORD == 8
    return (PLHashNumber)(((val >> 3) * 0x9E3779B97F4A7C15ULL) >> 32);
#elif PR_BYTES_PER_WORD == 4
    return (PLHashNumber)((val >> 2) * 0x9E3779B9U);
#else
    val >>= 2;
    if (sizeof(val) > 4) {
        return (PLHashNumber)((PRUint32)val ^ (PRUint32)(val >> 32));
    }
    return (PLHashNumber)((PRUint32)val);
#endif
}

RCKXDGBDCache *rck_xdg_bd_cache_new(void) {
	RCKXDGBDCache *cache;
	int i;
	
	cache = malloc(sizeof(RCKXDGBDCache));
	if (!cache) {
		return NULL;
	}
	
	cache->free_table = PL_NewHashTable(0, hash_ptr, PL_CompareValues, PL_CompareValues, NULL, NULL);
	populate(cache);
	
	return cache;
}

void rck_xdg_bd_cache_update(RCKXDGBDCache *cache) {
	if (cache) {
		destroy(cache);
		populate(cache);
	}
}

const char *rck_xdg_bd_cache_get_directory(RCKXDGBDCache *cache, RCKXDGBDDirectory dir) {
	if (cache) {
		return cache->dirs[dir];
	} else {
		return NULL;
	}
}

const char **rck_xdg_bd_cache_get_directory_list(RCKXDGBDCache *cache, RCKXDGBDDirectoryList list, RCKXDGBDLookupFlags flags, unsigned int *count) {
	if (cache) {
		if (flags & RCK_XDG_BD_FLAG_SEARCHABLE) {
			*count = cache->dir_lists_searchable_sz[list];
			return (const char **)cache->dir_lists_searchable[list];
		} else {
			*count = cache->dir_lists_sz[list];
			return (const char **)cache->dir_lists[list];
		}
	} else {
		*count = 0;
		return NULL;
	}
}

void rck_xdg_bd_cache_destroy(RCKXDGBDCache *cache) {
	if (cache) {
		destroy(cache);
		PL_HashTableDestroy(cache->free_table);
		free(cache);
	}
}
