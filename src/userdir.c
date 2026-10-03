#include <stdlib.h>
#include <string.h>
#include <nspr.h>
#include <plstr.h>
#include <plhash.h>
#include <prenv.h>
#include <ini_configobj.h>
#include <ini_valueobj.h>
#include "rck.h"

struct _RCKXDGUDCache {
	RCKXDGBDCache *bdcache;
	PRBool bdcache_destroy;
	char *dirs[RCK_XDG_UD_DIRECTORY_COUNT];
};

static void strip_quotes(char *str) {
	size_t len;
	
    if (!str) {
		return;
	}
	
    len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
        memmove(str, str + 1, len - 1);
        str[len - 2] = '\0';
    }
}

static void map_value(RCKXDGUDCache *ud, struct ini_cfgobj *ini, RCKXDGUDDirectory d, char *k) {
	struct value_obj *vobj;
	int rc;

	vobj = NULL;
	rc = ini_get_config_valueobj(NULL, k, ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		char *vstr;
		
		vstr = ini_get_string_config_value(vobj, &rc);
		
		if (!rc) {
			if (ud->dirs[d]) {
				rck_expandenv_free(ud->dirs[d]);
			}
			
			strip_quotes(vstr);
			ud->dirs[d] = rck_expandenv(vstr);
		}
		
		if (vstr) {
			free(vstr);
		}
    } 
}

static void load_ini_values(RCKXDGUDCache *ud, char *path) {
    struct ini_cfgobj *ini;
    struct ini_cfgfile *file;
    
	ini_config_create(&ini);
	ini_config_file_open(path, 0, &file);
	ini_config_parse(file, INI_STOP_ON_NONE, 0, 0, ini);

	map_value(ud, ini, RCK_XDG_UD_DESKTOP, "XDG_DESKTOP_DIR");
	map_value(ud, ini, RCK_XDG_UD_DOCUMENTS, "XDG_DOCUMENTS_DIR");
	map_value(ud, ini, RCK_XDG_UD_DOWNLOADS, "XDG_DOWNLOAD_DIR");
	map_value(ud, ini, RCK_XDG_UD_MUSIC, "XDG_MUSIC_DIR");
	map_value(ud, ini, RCK_XDG_UD_PICTURES, "XDG_PICTURES_DIR");
	map_value(ud, ini, RCK_XDG_UD_PROJECTS, "XDG_PROJECTS_DIR");
	map_value(ud, ini, RCK_XDG_UD_PUBLICSHARE, "XDG_PUBLICSHARE_DIR");
	map_value(ud, ini, RCK_XDG_UD_TEMPLATES, "XDG_TEMPLATES_DIR");
	map_value(ud, ini, RCK_XDG_UD_VIDEOS, "XDG_VIDEOS_DIR");

	ini_config_file_destroy(file); 
	ini_config_destroy(ini);	
}

RCKXDGUDCache *rck_xdg_ud_cache_new(RCKXDGBDCache *bdcache) {
	RCKXDGUDCache *ud;
	const char **dirs;
	char *ucfg;
	unsigned int i, c;
	
	ud = PR_NEW(RCKXDGUDCache);
	if (!ud) {
		return NULL;
	}
	
	if (bdcache) {
		ud->bdcache = bdcache;
		ud->bdcache_destroy = PR_FALSE;
	} else {
		ud->bdcache = rck_xdg_bd_cache_new();
		ud->bdcache_destroy = PR_TRUE;
	}

	for (i = 0; i < RCK_XDG_UD_DIRECTORY_COUNT; i++) {
		ud->dirs[i] = NULL;
	}
	
	for (i = 0; i < RCK_XDG_UD_DIRECTORY_COUNT; i++) {
		ud->dirs[i] = NULL;
	}
	
	dirs = rck_xdg_bd_cache_get_directory_list(ud->bdcache, RCK_XDG_BD_CONFIG_DIRS, RCK_XDG_BD_FLAGS_NONE, &c);
	for (i = 0; i < c; i++) {
		ucfg = PR_smprintf("%s/user-dirs.defaults", dirs[i]);
		if (PR_Access(ucfg, PR_ACCESS_EXISTS) == PR_SUCCESS) {
			load_ini_values(ud, ucfg);
		}
		PR_smprintf_free(ucfg);
	}
	
	ucfg = PR_smprintf("%s/user-dirs.dirs", rck_xdg_bd_cache_get_directory(ud->bdcache, RCK_XDG_BD_CONFIG_HOME));
	if (PR_Access(ucfg, PR_ACCESS_EXISTS) == PR_SUCCESS) {
		load_ini_values(ud, ucfg);
	}
	PR_smprintf_free(ucfg);

	return ud;
}

const char *rck_xdg_ud_cache_get_path(RCKXDGUDCache *cache, RCKXDGUDDirectory dir) {
	if (!cache) {
		return NULL;
	}
	
	return cache->dirs[dir];
}

RCKXDGUDDirectory rck_xdg_ud_cache_get_directory_of_path(RCKXDGUDCache *cache, const char *path) {
	unsigned int i;
	
	if (!cache || !path) {
		return RCK_XDG_UD_INVALID;
	}
	
	for (i = 0; i < RCK_XDG_UD_DIRECTORY_COUNT; i++) {
		if (!strcmp(path, cache->dirs[i])) {
			return i;
		}
	}

	return RCK_XDG_UD_INVALID;
}

void rck_xdg_ud_cache_destroy(RCKXDGUDCache *cache) {
	unsigned int i;
	
	if (!cache) {
		return;
	}

	for (i = 0; i < RCK_XDG_UD_DIRECTORY_COUNT; i++) {
		rck_expandenv_free(cache->dirs[i]);
	}

	if (cache->bdcache_destroy) {
		rck_xdg_bd_cache_destroy(cache->bdcache);
	}
	
	PR_Free(cache);
}
