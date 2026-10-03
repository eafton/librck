#include <stdlib.h>
#include <string.h>
#include "rck.h"

struct _RCKFSManager {
	RCKXDGBDCache *bdcache;
	PRBool bdcache_destroy;
	
	RCKXDGUDCache *udcache;
	PRBool udcache_destroy;
};

RCKFSManager *rck_fs_manager_new(RCKXDGBDCache *bdcache, RCKXDGUDCache* udcache) {
	RCKFSManager *fso;
	
	fso = PR_NEW(RCKFSManager);
	if (!fso) {
		return NULL;
	}
	
	if (bdcache) {
		fso->bdcache = bdcache;
		fso->bdcache_destroy = PR_FALSE;
	} else {
		fso->bdcache = rck_xdg_bd_cache_new();
		fso->bdcache_destroy = PR_TRUE;
	}
	
	if (udcache) {
		fso->udcache = udcache;
		fso->udcache_destroy = PR_FALSE;
	} else {
		fso->udcache = rck_xdg_ud_cache_new(fso->bdcache);
		fso->udcache_destroy = PR_TRUE;
	}
	
	return fso;
}

void rck_fs_manager_destroy(RCKFSManager *fso) {
	if (fso) {
		if (fso->udcache_destroy) {
			rck_xdg_ud_cache_destroy(fso->udcache);
		}
		
		if (fso->bdcache_destroy) {
			rck_xdg_bd_cache_destroy(fso->bdcache);
		}

		PR_Free(fso);
	}
}
