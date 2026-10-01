#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <nspr.h>
#include <plhash.h>
#include <plstr.h>
#include <prclist.h>
#include <ini_configobj.h>
#include <ini_valueobj.h>
#include <magic.h>
#include "rck.h"
#include "missing.h"

struct _RCKXDGIconThemes {
	RCKXDGBDCache *cache;
	PRCList themes;
};

typedef enum {
	IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_UNKNOWN,
	IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_FIXED,
	IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_SCALABLE,
	IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_THRESHOLD
} IRCKXDGIconThemeDirectoryType;

typedef enum {
	IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_UNKNOWN,
	IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_ACTIONS,
	IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_DEVICES,
	IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_FILESYSTEMS,
	IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_MIMETYPES
} IRCKXDGIconThemeDirectoryContext;

typedef struct {
	char *name;
	int size;
	int scale;
	IRCKXDGIconThemeDirectoryContext context;
	IRCKXDGIconThemeDirectoryType type;
	int max_size, min_size;
	int threshold;
} IRCKXDGIconThemeDirectory;

struct _RCKXDGIconTheme {
	PRCList link;
	
	RCKXDGIconThemes *parent;
	RCKXDGIconThemeLocation location;
	
	char *internal_name;
	char *location_path;
	
	char *name_non_localized;
	PLHashTable *name_localizations;

	char *comment_non_localized;
	PLHashTable *comment_localizations;
	
	char **inherits;
	char **directory_names;
	char **scaled_directory_names;
	PLHashTable *directories;
	
	PRBool hide;
	
	char *example;
};

static void fill_theme_header(RCKXDGIconTheme *theme, struct ini_cfgobj *ini) {
	char **key_list;
	struct value_obj *vobj;
	uint32_t i, key_count;
	int rc;

	vobj = NULL;
	key_list = NULL;
	rc = ini_get_config_valueobj("Icon Theme", "Name", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		char *vstr;
		
		vstr = ini_get_string_config_value(vobj, &rc);
		if (!rc) {
			theme->name_non_localized = vstr;
		}
    } 
    
	vobj = NULL;
	rc = ini_get_config_valueobj("Icon Theme", "Comment", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		char *vstr;
		
		vstr = ini_get_string_config_value(vobj, &rc);
		if (!rc) {
			theme->comment_non_localized = vstr;
		}
    } 
    
    rc = 0;
	key_count = 0;
	key_list = ini_get_attribute_list(ini, "Icon Theme", &key_count, &rc);
    if (!rc && key_list) {
		for (i = 0; i < key_count; i++) {
			char *kl;
			
			kl = strchr(key_list[i], '[');
			if ((rck_strprefix(key_list[i], "Name") || rck_strprefix(key_list[i], "Comment")) && kl && kl[strlen(kl) - 1] == ']') {
				vobj = NULL;
				rc = ini_get_config_valueobj("Icon Theme", key_list[i], ini, INI_GET_LAST_VALUE, &vobj);
				if (!rc && vobj) {
					char *vstr;
					
					vstr = ini_get_string_config_value(vobj, &rc);
					if (!rc) {
						char *table_key;
						size_t l;
						
						kl++;
						l = strlen(kl);
						table_key = malloc(l);
						strncpy(table_key, kl, l - 1);
						table_key[l - 1] = '\0';
												
						if (rck_strprefix(key_list[i], "Name")) {
							if (!theme->name_localizations) {
								theme->name_localizations = PL_NewHashTable(0, PL_HashString, PL_CompareStrings, PL_CompareStrings, NULL, NULL);
							}

							PL_HashTableAdd(theme->name_localizations, table_key, vstr);
						} else {
							if (!theme->comment_localizations) {
								theme->comment_localizations = PL_NewHashTable(0, PL_HashString, PL_CompareStrings, PL_CompareStrings, NULL, NULL);
							}

							PL_HashTableAdd(theme->comment_localizations, table_key, vstr);
						}
					}
				} 
			}
		}
    }
	ini_free_attribute_list(key_list);


	vobj = NULL;
	rc = ini_get_config_valueobj("Icon Theme", "Inherits", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		char *value;
		
		value = ini_get_string_config_value(vobj, &rc);
		if (!rc) {
			if (strchr(value, ',')) {
				char *kstate, *k, *o;
				size_t i;
				
				i = 1;
				o = PL_strdup(value);
				kstate = o;
				while ((k = PL_strtok_r(kstate, ",", &kstate))) {
					i++;
				}	
				PL_strfree(o);
			
				theme->inherits = calloc(i, sizeof(char *));
				i = 0;
				o = PL_strdup(value);
				kstate = o;
				while ((k = PL_strtok_r(kstate, ",", &kstate))) {
					theme->inherits[i] = PL_strdup(k);
					i++;
				}	
				PL_strfree(o);
				theme->inherits[i] = NULL;
			} else {
				theme->inherits = calloc(2, sizeof(char *));
				theme->inherits[0] = PL_strdup(value);
				theme->inherits[1] = NULL;
			}
		}
		
		if (value) {
			free(value);
		}
    } 

	vobj = NULL;
	rc = ini_get_config_valueobj("Icon Theme", "Directories", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		char *value;
		
		value = ini_get_string_config_value(vobj, &rc);
		if (!rc) {
			char *kstate, *k, *o;
			size_t i;
				
			i = 1;
			o = PL_strdup(value);
			kstate = o;
			while ((k = PL_strtok_r(kstate, ",", &kstate))) {
				i++;
			}	
			PL_strfree(o);
			
			theme->directory_names = calloc(i, sizeof(char *));
			i = 0;
			o = PL_strdup(value);
			kstate = o;
			while ((k = PL_strtok_r(kstate, ",", &kstate))) {
				theme->directory_names[i] = PL_strdup(k);
				i++;
			}	
			PL_strfree(o);
			theme->directory_names[i] = NULL;
		}
		
		if (value) {
			free(value);
		}
    } 

	vobj = NULL;
	rc = ini_get_config_valueobj("Icon Theme", "ScaledDirectories", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		char *value;
		
		value = ini_get_string_config_value(vobj, &rc);
		if (!rc) {
			char *kstate, *k, *o;
			size_t i;
			
			i = 1;
			o = PL_strdup(value);
			kstate = o;
			while ((k = PL_strtok_r(kstate, ",", &kstate))) {
				i++;
			}	
			PL_strfree(o);
			
			theme->scaled_directory_names = calloc(i, sizeof(char *));
			i = 0;
			o = PL_strdup(value);
			kstate = o;
			while ((k = PL_strtok_r(kstate, ",", &kstate))) {
				theme->scaled_directory_names[i] = PL_strdup(k);
				i++;
			}	
			PL_strfree(o);
			theme->scaled_directory_names[i] = NULL;
		}
		
		if (value) {
			free(value);
		}
    } 

	vobj = NULL;
	rc = ini_get_config_valueobj("Icon Theme", "Example", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		char *vstr;
		
		vstr = ini_get_string_config_value(vobj, &rc);
		if (!rc) {
			theme->example = vstr;
		}
    } 
    
	vobj = NULL;
	rc = ini_get_config_valueobj("Icon Theme", "Hidden", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		char *vstr;
		
		vstr = ini_get_string_config_value(vobj, &rc);
		if (!rc) {
			if (!PL_strcasecmp(vstr, "true") || !strcmp(vstr, "1")) {
				theme->hide = PR_TRUE;
			}
		}
		
		if (vstr) {
			free(vstr);
		}
    } 
}

static void fill_theme_directory(IRCKXDGIconThemeDirectory *dir, struct ini_cfgobj *ini) {
	struct value_obj *vobj;
	int rc;

	vobj = NULL;
	rc = ini_get_config_valueobj(dir->name, "Size", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		int vint;
		
		vint = ini_get_int_config_value(vobj, 0, 0, &rc);
		if (!rc) {
			dir->size = vint;
		} 
    }  

	vobj = NULL;
	rc = ini_get_config_valueobj(dir->name, "Scale", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		int vint;
		
		vint = ini_get_int_config_value(vobj, 0, 1, &rc);
		if (!rc) {
			dir->scale = vint;
		} 
    }  

	vobj = NULL;
	rc = ini_get_config_valueobj(dir->name, "Context", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		char *vstr;
		
		vstr = ini_get_string_config_value(vobj, &rc);
		if (!rc) {
			if (!PL_strcasecmp(vstr, "Actions")) {
				dir->context = IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_ACTIONS;
			} else if (!PL_strcasecmp(vstr, "Devices")) {
				dir->context = IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_DEVICES;
			} else if (!PL_strcasecmp(vstr, "FileSystems")) {
				dir->context = IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_FILESYSTEMS;
			} else if (!PL_strcasecmp(vstr, "MimeTypes")) {
				dir->context = IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_MIMETYPES;
			} else {
				dir->context = IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_UNKNOWN;
			}
		}
		
		if (vstr) {
			free(vstr);
		}
    }  
	
	vobj = NULL;
	rc = ini_get_config_valueobj(dir->name, "Type", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		char *vstr;
		
		vstr = ini_get_string_config_value(vobj, &rc);
		if (!rc) {
			if (!PL_strcasecmp(vstr, "Fixed")) {
				dir->type = IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_FIXED;
			} else if (!PL_strcasecmp(vstr, "Scalable")) {
				dir->type = IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_SCALABLE;
			} else if (!PL_strcasecmp(vstr, "Threshold")) {
				dir->type = IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_THRESHOLD;
			} else {
				dir->type = IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_UNKNOWN;
			}
		}
		
		if (vstr) {
			free(vstr);
		}
    }  

	dir->max_size = dir->size;
	rc = ini_get_config_valueobj(dir->name, "MaxSize", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		int vint;
		
		vint = ini_get_int_config_value(vobj, 0, dir->size, &rc);
		if (!rc) {
			dir->max_size = vint;
		} 
    }  

	dir->min_size = dir->size;
	rc = ini_get_config_valueobj(dir->name, "MinSize", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		int vint;
		
		vint = ini_get_int_config_value(vobj, 0, dir->size, &rc);
		if (!rc) {
			dir->min_size = vint;
		} 
    }  
    
	rc = ini_get_config_valueobj(dir->name, "Threshold", ini, INI_GET_LAST_VALUE, &vobj);
    if (!rc && vobj) {
		int vint;
		
		vint = ini_get_int_config_value(vobj, 0, 2, &rc);
		if (!rc) {
			dir->threshold = vint;
		} 
    }  
}

static void create_theme_directories(RCKXDGIconTheme *theme, struct ini_cfgobj *ini) {
	IRCKXDGIconThemeDirectory *dir;
	char *cursor;
	size_t sz, i;

	sz = 0;
	if (theme->directory_names) {
		i = 0;
		cursor = theme->directory_names[i];
		while(cursor) {
			sz++;
			i++;
			cursor = theme->directory_names[i];
		}
	}
	
	if (theme->scaled_directory_names) {
		i = 0;
		cursor = theme->scaled_directory_names[i];
		while(cursor) {
			sz++;
			i++;
			cursor = theme->scaled_directory_names[i];
		}
	}
	
	theme->directories = PL_NewHashTable(sz, PL_HashString, PL_CompareStrings, PL_CompareValues, NULL, NULL);

	if (theme->directory_names) {
		i = 0;
		cursor = theme->directory_names[i];
		while(cursor) {
			dir = PR_NEW(IRCKXDGIconThemeDirectory);

			dir->name = PL_strdup(cursor);
			dir->size = -1;
			dir->scale = 1;
			dir->context = IRCK_XDG_ICON_THEME_DIRECTORY_CONTEXT_UNKNOWN;
			dir->type = IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_THRESHOLD;
			dir->max_size = -1;
			dir->min_size = -1;
			dir->threshold = 2;

			fill_theme_directory(dir, ini);
			
			PL_HashTableAdd(theme->directories, dir->name, dir);

			i++;
			cursor = theme->directory_names[i];
		}
	}
	
	if (theme->scaled_directory_names) {
		i = 0;
		cursor = theme->scaled_directory_names[i];
		while(cursor) {
			dir = PR_NEW(IRCKXDGIconThemeDirectory);

			dir->name = PL_strdup(cursor);
			dir->size = -1;
			dir->scale = 1;
			dir->context = IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_UNKNOWN;
			dir->type = IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_THRESHOLD;
			dir->max_size = -1;
			dir->min_size = -1;
			dir->threshold = 2;
			
			fill_theme_directory(dir, ini);
			
			PL_HashTableAdd(theme->directories, dir->name, dir);

			i++;
			cursor = theme->scaled_directory_names[i];
		}
	}
}

void add_theme(RCKXDGIconThemes *themes, char *themedir, RCKXDGIconThemeLocation location) {
	RCKXDGIconTheme *theme;
    struct ini_cfgobj *ini;
    struct ini_cfgfile *file;
   	char *idex;
	int rc;
	
	theme = PR_NEW(RCKXDGIconTheme);
	if (!theme) {
		return;
	}

	ini = NULL;
	file = NULL;
	theme->location_path = PL_strdup(themedir);
	theme->internal_name = strrchr(theme->location_path, '/') + 1;
	theme->parent = themes;
	theme->location = location;
	theme->hide = PR_FALSE;
	theme->name_non_localized = NULL;
	theme->name_localizations = NULL;
	theme->comment_non_localized = NULL;
	theme->comment_localizations = NULL;
	theme->inherits = NULL;
	theme->directory_names = NULL;
	theme->scaled_directory_names = NULL;
    theme->directories = NULL;
	theme->example = NULL;
	
	idex = PR_smprintf("%s/index.theme", themedir);
	if (PR_Access(idex, PR_ACCESS_EXISTS) != PR_SUCCESS) {
		PL_strfree(theme->location_path);
		PR_Free(theme);
		PR_smprintf_free(idex);	
		return;
	}
	ini_config_create(&ini);
	ini_config_file_open(idex, 0, &file);
	PR_smprintf_free(idex);	
	ini_config_parse(file, INI_STOP_ON_NONE, 0, 0, ini);
	fill_theme_header(theme, ini);
	create_theme_directories(theme, ini);
	ini_config_file_destroy(file); 
	ini_config_destroy(ini);
	PR_INIT_CLIST(&theme->link);
	PR_APPEND_LINK(&theme->link, &themes->themes);
}

RCKXDGIconThemes *rck_xdg_icon_themes_new(void) {
	RCKXDGIconThemes *themes;
	PRDir *dir;
	PRDirEntry *de;
	const char **list;
	const char *home;
	char *smstring;
	unsigned int count, i;
	
	themes = PR_NEW(RCKXDGIconThemes);
	if (!themes) {
		return NULL;
	}
    PR_INIT_CLIST(&themes->themes);

	themes->cache = rck_xdg_bd_cache_new();
	if (!themes->cache) {
		PR_Free(themes);
		return NULL;
	}
	
	home = PR_GetEnv("HOME");
	if (!home) {
		goto SYSTEM_ICONS;
	}
	
	smstring = PR_smprintf("%s/.icons", home);
	if (!smstring) {
		goto SYSTEM_ICONS;
	}
	
	dir = PR_OpenDir(smstring);
	if (!dir) {
		goto SYSTEM_ICONS;
		PR_smprintf_free(smstring);
	}
	
	de = PR_ReadDir(dir, PR_SKIP_BOTH);
	while (de) {
		char *icd;
	
		icd = PR_smprintf("%s/%s", smstring, de->name);
		if (icd) {
			add_theme(themes, icd, RCK_XDG_ICON_THEME_LOCATION_USER);
			PR_smprintf_free(icd);
		}
		de = PR_ReadDir(dir, PR_SKIP_BOTH);
	}
	PR_CloseDir(dir);
	PR_smprintf_free(smstring);
	
SYSTEM_ICONS:
	list = rck_xdg_bd_cache_get_directory_list(themes->cache, RCK_XDG_BD_DATA_DIRS, RCK_XDG_BD_FLAG_SEARCHABLE, &count);
	for (i = 0; i < count; i++) {
		smstring = PR_smprintf("%s/icons", list[i]);
		if (!smstring) {
			continue;
		}
		
		dir = PR_OpenDir(smstring);
		if (!dir) {
			
			PR_smprintf_free(smstring);
			continue;
		}
		
		de = PR_ReadDir(dir, PR_SKIP_BOTH);
		while (de) {
			char *icd;
	
			icd = PR_smprintf("%s/%s", smstring, de->name);
			if (icd) {
				add_theme(themes, icd, RCK_XDG_ICON_THEME_LOCATION_USER);
				PR_smprintf_free(icd);
			}
			de = PR_ReadDir(dir, PR_SKIP_BOTH);
		}
		
		PR_CloseDir(dir);
		PR_smprintf_free(smstring);
	}
	
	dir = PR_OpenDir("/usr/share/pixmaps");
	if (dir) {
		de = PR_ReadDir(dir, PR_SKIP_BOTH);
		while (de) {
			char *icd;
		
			icd = PR_smprintf("/usr/share/pixmaps/%s", de->name);
			if (icd) {
				add_theme(themes, icd, RCK_XDG_ICON_THEME_LOCATION_LEGACY);
				PR_smprintf_free(icd);
			}
			de = PR_ReadDir(dir, PR_SKIP_BOTH);
		}
		PR_CloseDir(dir);
	}
	
	return themes;
}

PRCList *rck_xdg_icon_themes_get_themes(RCKXDGIconThemes *themes) {
	if (!themes) {
		return NULL;
	}
	
	return &themes->themes;
}

const char *rck_xdg_icon_theme_get_localized_string(RCKXDGIconTheme *theme, RCKXDGIconThemeLocalizedString str, const char *locale) {
	if (!theme) {
		return NULL;
	}
	
	switch (str) {
		case RCK_XDG_ICON_THEME_LOCALIZED_STRING_NAME:
			if (!locale) {
				return theme->name_non_localized;
			} else {
				const char *ret;
				
				ret = PL_HashTableLookup(theme->name_localizations, locale);
				if (!ret) {
					ret = theme->name_non_localized;
				}
				
				return ret;
			}
			break;
		case RCK_XDG_ICON_THEME_LOCALIZED_STRING_COMMENT:
			if (!locale) {
				return theme->comment_non_localized;
			} else {
				const char *ret;
				
				ret = PL_HashTableLookup(theme->comment_localizations, locale);
				if (!ret) {
					ret = theme->comment_non_localized;
				}
				
				return ret;
			}
			break;
		default:
			break;
	}
	
	return NULL;
}

static PRIntn PR_CALLBACK count_languages(PLHashEntry *he, PRIntn index, void *arg) {
	(*((size_t*)arg))++;
	return HT_ENUMERATE_NEXT;
}

static PRIntn PR_CALLBACK populate(PLHashEntry *he, PRIntn index, void *arg) {
	const char **param;
	
	param = (const char **)arg;
	param[index] = (const char *)he->key;
	return HT_ENUMERATE_NEXT;
}

const char **rck_xdg_icon_theme_get_string_localizations(RCKXDGIconTheme *theme, RCKXDGIconThemeLocalizedString str) {
	const char **ret;
	size_t sz;
	
	if (!theme) {
		return NULL;
	}
	
	switch (str) {
		case RCK_XDG_ICON_THEME_LOCALIZED_STRING_NAME:
			sz = 0;
			if (theme->name_localizations) {
				PL_HashTableEnumerateEntries(theme->name_localizations, count_languages, &sz);
			}
						
			if (theme->name_localizations && sz) {
				sz++;
				ret = calloc(sz, sizeof(char *));
				PL_HashTableEnumerateEntries(theme->name_localizations, populate, ret);
				ret[sz - 1] = NULL;
				return ret;
			}
			break;
		case RCK_XDG_ICON_THEME_LOCALIZED_STRING_COMMENT:
			sz = 0;
			if (theme->comment_localizations) {
				PL_HashTableEnumerateEntries(theme->comment_localizations, count_languages, &sz);
			}
			
			if (theme->name_localizations && sz) {
				sz++;
				ret = calloc(sz, sizeof(char *));
				PL_HashTableEnumerateEntries(theme->comment_localizations, populate, ret);
				ret[sz - 1] = NULL;
				return ret;
			}
			break;
		default:
			break;
	}
	
	return NULL;
}

const char *rck_xdg_icon_theme_get_string(RCKXDGIconTheme *theme, RCKXDGIconThemeString str) {
	if (!theme) {
		return NULL;
	}
	
	switch (str) {
		case RCK_XDG_ICON_THEME_STRING_INTERNAL_NAME:
			return theme->internal_name;
			break;
		case RCK_XDG_ICON_THEME_STRING_EXAMPLE:
			return theme->example;
			break;
		case RCK_XDG_ICON_THEME_STRING_PATH:
			return theme->location_path;
			break;
		default:
			break;
	}
	
	return NULL;
}

PRBool rck_xdg_icon_theme_get_bool(RCKXDGIconTheme *theme, RCKXDGIconThemeBool bl, PRBool *ret) {
	if (!theme) {
		return PR_FALSE;
	}
	
	switch (bl) {
		case RCK_XDG_ICON_THEME_BOOL_HIDE:
			if (ret) {
				*ret = theme->hide;
			}
			return PR_TRUE;
			break;
		default:
			break;
	}
	
	return PR_FALSE;	
}

RCKXDGIconThemeLocation rck_xdg_icon_theme_get_location(RCKXDGIconTheme *theme) {
	if (!theme) {
		return RCK_XDG_ICON_THEME_LOCATION_UNKNOWN;
	}
	
	return theme->location;
}

static PRIntn PR_CALLBACK cleanup_localizations(PLHashEntry *he, PRIntn index, void *arg) {
	free((void*)he->value);
	free((void*)he->key);
	return HT_ENUMERATE_NEXT;
}

static PRIntn PR_CALLBACK cleanup_dirs(PLHashEntry *he, PRIntn index, void *arg) {
	IRCKXDGIconThemeDirectory *dir;

	dir = (IRCKXDGIconThemeDirectory *)he->value;
	PL_strfree(dir->name);
	PR_Free(dir);
	return HT_ENUMERATE_NEXT;
}

static void checked_strfree(void *ptr) {
	if (ptr) {
		PL_strfree(ptr);
	}
}

static void checked_free(void *ptr) {
	if (ptr) {
		free(ptr);
	}
}
static void free_string_list(char **list) {
	char *cursor;
	size_t i;
	
	if (!list) {
		return;
	}
	
	i = 0;
	cursor = list[i];
	while(cursor) {
		checked_strfree(list[i]);
		i++;
		cursor = list[i];
	}
	free(list);
}

static void free_icon_theme(RCKXDGIconTheme *theme) {
	checked_strfree(theme->location_path);
	checked_free(theme->name_non_localized);
	checked_free(theme->comment_non_localized);
	checked_free(theme->example);

	if (theme->name_localizations) {
		PL_HashTableEnumerateEntries(theme->name_localizations, cleanup_localizations, NULL);
		PL_HashTableDestroy(theme->name_localizations);
	}
	
	if (theme->comment_localizations) {
		PL_HashTableEnumerateEntries(theme->comment_localizations, cleanup_localizations, NULL);
		PL_HashTableDestroy(theme->comment_localizations);
	}
	
	free_string_list(theme->inherits);
	free_string_list(theme->scaled_directory_names);
	free_string_list(theme->directory_names);
	
	if (theme->directories) {
		PL_HashTableEnumerateEntries(theme->directories, cleanup_dirs, NULL);
		PL_HashTableDestroy(theme->directories);
	}
}

void rck_xdg_icon_themes_free(RCKXDGIconThemes *themes) {
	PRCList *head, *cursor;

	if (!themes) {
		return;
	}
	
	rck_xdg_bd_cache_destroy(themes->cache);
	
	head = &themes->themes;
    cursor = PR_LIST_HEAD(head);
    while (cursor != head) {
		RCKXDGIconTheme *th;
		 
		th = RCK_XDG_ICON_THEME(cursor);
        free_icon_theme(th);
        cursor = PR_NEXT_LINK(cursor);
        PR_Free(th);
    }
    
    PR_Free(themes);
}

RCKXDGIconTheme *rck_xdg_icon_themes_get_theme_by_name(RCKXDGIconThemes *themes, const char *name) {
	RCKXDGIconTheme *ret;
	PRCList *head, *cursor;
	
	if (!themes || !name) {
		return NULL;
	}
	
	ret = NULL;
	head = &themes->themes;
    cursor = PR_LIST_HEAD(head);
    while (cursor != head) {
		RCKXDGIconTheme *th;
		 
		th = RCK_XDG_ICON_THEME(cursor);
		if (!strcmp(th->internal_name, name)) {
			ret = th;
		}
        cursor = PR_NEXT_LINK(cursor);
    }
	
	return ret;
}

PRBool directory_size_match(RCKXDGIconTheme *theme, const char *sdir, unsigned int size, unsigned int scale) {
	IRCKXDGIconThemeDirectory *dir;
	
	if (!theme->directories) {
		return PR_FALSE;
	}
	
	dir = PL_HashTableLookup(theme->directories, sdir);
	if (!theme->directories) {
		return PR_FALSE;
	}

	if (dir->scale != scale) {
		return PR_FALSE;
	}

	if (dir->type == IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_FIXED && dir->size == size) {
		return PR_TRUE;
	}

	if (dir->type == IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_SCALABLE && (dir->min_size <= size <= dir->max_size)) {
		return PR_TRUE;
	}

	if (dir->type == IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_THRESHOLD && (dir->size - dir->threshold <= size <= dir->size + dir->threshold)) {
		return PR_TRUE;
	}
	
	return PR_FALSE;
}

unsigned int directory_size_distance(RCKXDGIconTheme *theme, const char *sdir, unsigned int size, unsigned int scale) {
	IRCKXDGIconThemeDirectory *dir;
	
	if (!theme->directories) {
		return 0;
	}
	
	dir = PL_HashTableLookup(theme->directories, sdir);
	if (!theme->directories) {
		return 0;
	}

	switch (dir->type) {
		case IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_FIXED:
			return abs(dir->size*dir->scale - size*scale);
			break;
		case IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_SCALABLE:
		    if (size*scale < dir->min_size*dir->scale) {
				return dir->min_size*dir->scale - size*scale;
			}	
			
		    if (size*scale > dir->max_size*dir->scale) {
				return size*scale - dir->max_size*dir->scale;
			}	
			break;
		case IRCK_XDG_ICON_THEME_DIRECTORY_TYPE_THRESHOLD:
		    if (size*scale < (dir->size - dir->threshold)*dir->scale) {
				return dir->min_size*dir->scale - size*scale;
			}	
			
		    if (size*scale > (dir->size + dir->threshold)*dir->scale) {
				return size*scale - dir->max_size*dir->scale;
			}	
			break;
		default:
			break;
	}
	
	return 0;
}

static const char *internal_lookup_helper(RCKXDGIconTheme *theme, const char *sdir, const char* name, unsigned int size, unsigned int scale) {
	PRDir *dir;
	PRDirEntry *de;
	char *sm;

	sm = PR_smprintf("%s/%s/%s.png", theme->location_path, sdir, name);
	if (PR_Access(sm, PR_ACCESS_EXISTS) == PR_SUCCESS) {
		char *ret;
		
		ret = PL_strdup(sm);
		PR_smprintf_free(sm);
		return ret;
	}
	PR_smprintf_free(sm);

	
	sm = PR_smprintf("%s/%s/%s.svg", theme->location_path, sdir, name);
	if (PR_Access(sm, PR_ACCESS_EXISTS) == PR_SUCCESS) {
		char *ret;
		
		ret = PL_strdup(sm);
		PR_smprintf_free(sm);
		return ret;
	}
	PR_smprintf_free(sm);

	
	sm = PR_smprintf("%s/%s/%s.xpm", theme->location_path, sdir, name);
	if (PR_Access(sm, PR_ACCESS_EXISTS) == PR_SUCCESS) {
		char *ret;
		
		ret = PL_strdup(sm);
		PR_smprintf_free(sm);
		return ret;
	}
	PR_smprintf_free(sm);

	return NULL;
}

static const char *internal_lookup_icon(RCKXDGIconTheme *theme, const char *name, unsigned int size, unsigned int scale) {
	const char *ret;
	char *cursor;
	unsigned int minimal_size;
	size_t i;
	
	if (!theme) {
		return NULL;
	}
	ret = NULL;
	
	if (theme->directory_names) {
		i = 0;
		cursor = theme->directory_names[i];
		while (cursor) {
			ret = internal_lookup_helper(theme, cursor, name, size, scale);
			if (ret && directory_size_match(theme, cursor, size, scale)) {
				return ret;
			}
			i++;
			cursor = theme->directory_names[i];
		}
	}
	
	if (theme->scaled_directory_names) {
		i = 0;
		cursor = theme->scaled_directory_names[i];
		while (cursor) {
			ret = internal_lookup_helper(theme, cursor, name, size, scale);
			if (ret && directory_size_match(theme, cursor, size, scale)) {
				return ret;
			}
			i++;
			cursor = theme->scaled_directory_names[i];
		}
	}
	
	minimal_size = UINT_MAX;
	if (theme->directory_names) {
		i = 0;
		cursor = theme->directory_names[i];
		while (cursor) {
			const char *cret;
			unsigned int dist;
			
			cret = internal_lookup_helper(theme, cursor, name, size, scale);
			dist = directory_size_distance(theme, cursor, size, scale);
			if (cret && dist < minimal_size) {
				ret = cret;
				minimal_size = dist;
			}
			i++;
			cursor = theme->directory_names[i];
		}
	}
	
	minimal_size = UINT_MAX;
	if (theme->scaled_directory_names) {
		i = 0;
		cursor = theme->scaled_directory_names[i];
		while (cursor) {
			const char *cret;
			unsigned int dist;
			
			cret = internal_lookup_helper(theme, cursor, name, size, scale);
			dist = directory_size_distance(theme, cursor, size, scale);
			if (cret && dist < minimal_size) {
				ret = cret;
				minimal_size = dist;
			}
			i++;
			cursor = theme->scaled_directory_names[i];
		}
	}
	
	return ret;
}


static const char *internal_lookup_icon_helper(RCKXDGIconTheme *theme, const char *name, unsigned int size, unsigned int scale) {
	RCKXDGIconTheme *hicolor;
	const char *ret;

	ret = internal_lookup_icon(theme, name, size, scale);
	if (ret) {
		return ret;
	}
	
	if (theme->inherits) {
		char *cursor;
		size_t i;

		i = 0;
		cursor = theme->inherits[i];
		while (cursor) {
			RCKXDGIconTheme *th;
			
			th = rck_xdg_icon_themes_get_theme_by_name(theme->parent, cursor);
			ret = internal_lookup_icon(th, name, size, scale);
			if (ret) {
				return ret;
			}

			i++;
			cursor = theme->inherits[i];
		}
	}
	
	return NULL;
}


const char *rck_xdg_icon_theme_lookup_icon(RCKXDGIconTheme *theme, const char *name, unsigned int size, unsigned int scale) {
	RCKXDGIconTheme *hicolor;
	const char *ret;
	
	if (!theme || !name) {
		return NULL;
	}
	
	ret = internal_lookup_icon_helper(theme, name, size, scale);
	if (ret) {
		return ret;
	}
	
	hicolor = rck_xdg_icon_themes_get_theme_by_name(theme->parent, "hicolor");
	if (hicolor) {		
		ret = internal_lookup_icon_helper(hicolor, name, size, scale);
		if (ret) {
			return ret;
		}
	}
	
	return NULL;
}

pixman_image_t *generate_missing_image(unsigned int size) {
	pixman_image_t *image;
	RCKIOVariant iov;

	iov.data.buf = missing_png;
	iov.data.sz = missing_png_len;
	image = rck_png_load(RCK_IO_VARIANT_TYPE_DATA, &iov);
	if (!image) {
		return NULL;
	}
	
	if (size == pixman_image_get_width(image)) {
		return image;
	} else {
		pixman_image_t *scaled;
		pixman_transform_t tf;
		pixman_fixed_t sx, sy;

		scaled = pixman_image_create_bits(pixman_image_get_format(image), size, size, NULL, -1);
		sx = pixman_double_to_fixed((double)pixman_image_get_width(image) / size);
		sy = pixman_double_to_fixed((double)pixman_image_get_height(image) / size);
		pixman_transform_init_scale(&tf, sx, sy);
		pixman_image_set_transform(image, &tf);
		pixman_image_set_filter(image, PIXMAN_FILTER_NEAREST, NULL, 0);
		pixman_image_composite(PIXMAN_OP_SRC, image, NULL, scaled, 0, 0, 0, 0, 0, 0, size, size);
		pixman_image_unref(image);
		return scaled;
	}
}

pixman_image_t *rck_xdg_icon_theme_load_icon(RCKXDGIconTheme *theme, const char *name, unsigned int size, unsigned int scale) {
	pixman_image_t *image;
	magic_t magic;
	const char *mime;
	RCKIOVariant iov;
	
	image = NULL;
	iov.filename = rck_xdg_icon_theme_lookup_icon(theme, name, size, scale);
	if (!iov.filename) {
		return generate_missing_image(size*scale);
	}
	
	magic = magic_open(MAGIC_MIME_TYPE); 
	magic_load(magic, NULL);
	mime = magic_file(magic, iov.filename);
	if (strstr(mime, "png")) {
		image = rck_png_load(RCK_IO_VARIANT_TYPE_FILENAME, &iov);
	} else if (strstr(mime, "svg")) {
		image = rck_svg_load(RCK_IO_VARIANT_TYPE_FILENAME, &iov, size*scale, size*scale);
	} else {
		image = rck_xpm_load(RCK_XPM_INPUT_TYPE_FROM_FILENAME, (char*)iov.filename, NULL, NULL, NULL, NULL);
	}
	magic_close(magic);
	rck_xdg_icon_theme_free_icon_path(iov.filename);
	
	if (!image) {
		return generate_missing_image(size*scale);
	}
	
	return image;
}
