#include <stddef.h>
#include <stdio.h>
#include <nspr.h>
#include <prclist.h>
#include <plstr.h>
#include <pixman.h>
#include <rckexport.h>

#ifndef LIBRCK_H
#define LIBRCK_H

#ifdef __cplusplus
extern "C" {
#endif 

/* VERSION */
#define RCK_MAJOR_VERSION 0
#define RCK_MINOR_VERSION 0

RCK_EXPORT unsigned int rck_version(unsigned int *minor);

/* INIT */
RCK_EXPORT PRBool rck_init();
RCK_EXPORT void rck_deinit(void);

/* UTILITES */
RCK_EXPORT void rck_free(void *ptr);
RCK_EXPORT PRBool rck_strprefix(const char *string, const char *prefix);
RCK_EXPORT PRBool rck_strsuffix(const char *string, const char *suffix);
RCK_EXPORT PRBool rck_strcaseprefix(const char *string, const char *prefix);
#define rck_isdigit_ascii(c) ((c) >= '0' && (c) <= '9')
#define rck_isletter_ascii(c) ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
#define RCK_PR_CLIST_ENTRY(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

/* XPM LOADER */
typedef enum {
	RCK_XPM_INPUT_TYPE_FROM_FILENAME,
	RCK_XPM_INPUT_TYPE_FROM_STRING
} RCKXPMInputType;

typedef enum {
	RCK_XPM_INFO_MASK_NONE = 0,
	RCK_XPM_INFO_MASK_VERSION = 1 << 0,
	RCK_XPM_INFO_MASK_HOTSPOT = 1 << 1
} RCKXPMInfoMask;

RCK_EXPORT pixman_image_t *rck_xpm_load(RCKXPMInputType load_mask, char *input, RCKXPMInfoMask *outInfoMask, unsigned int *outVersion, unsigned int *outHotX, unsigned int *outHotY);

/* SVG LOADER */
typedef union {
	const char* filename;
	struct RCKInputVariantData {
		void *buf;
		size_t sz;
	} data;
} RCKIOVariant;

typedef enum {
	RCK_IO_VARIANT_TYPE_FILENAME,
	RCK_IO_VARIANT_TYPE_DATA
} RCKIOVariantType;

RCK_EXPORT pixman_image_t *rck_svg_load(RCKIOVariantType mask, RCKIOVariant *input, float width, float height);

/* PNG CODEC */
RCK_EXPORT pixman_image_t *rck_png_load(RCKIOVariantType mask, RCKIOVariant *input);
RCK_EXPORT PRBool rck_png_save(pixman_image_t *image, RCKIOVariantType mask, RCKIOVariant *input);

/* XDG BASE DIRECTORY */
typedef struct _RCKXDGBDCache RCKXDGBDCache;

typedef enum {
	RCK_XDG_BD_DATA_HOME = 0,
	RCK_XDG_BD_CONFIG_HOME,
	RCK_XDG_BD_STATE_HOME,
	RCK_XDG_BD_CACHE_HOME,
	RCK_XDG_BD_RUNTIME_DIR,
	RCK_XDG_BD_DIRECTORY_COUNT
} RCKXDGBDDirectory;

typedef enum {
	RCK_XDG_BD_DATA_DIRS = 0,
	RCK_XDG_BD_CONFIG_DIRS,
	RCK_XDG_BD_DIRECTORY_LIST_COUNT
} RCKXDGBDDirectoryList;

typedef enum {
	RCK_XDG_BD_FLAGS_NONE = 0,
	RCK_XDG_BD_FLAG_SEARCHABLE = 1 << 0	
} RCKXDGBDLookupFlags;

RCK_EXPORT RCKXDGBDCache *rck_xdg_bd_cache_new(void);
RCK_EXPORT void rck_xdg_bd_cache_update(RCKXDGBDCache *cache);
RCK_EXPORT const char *rck_xdg_bd_cache_get_directory(RCKXDGBDCache *cache, RCKXDGBDDirectory dir);
RCK_EXPORT const char **rck_xdg_bd_cache_get_directory_list(RCKXDGBDCache *cache, RCKXDGBDDirectoryList list, RCKXDGBDLookupFlags flags, unsigned int *count);
RCK_EXPORT void rck_xdg_bd_cache_destroy(RCKXDGBDCache *cache);

/* XDG ICON THEME */
typedef struct _RCKXDGIconThemes RCKXDGIconThemes;
typedef struct _RCKXDGIconTheme RCKXDGIconTheme;

typedef enum {
	RCK_XDG_ICON_THEME_LOCATION_UNKNOWN,
	RCK_XDG_ICON_THEME_LOCATION_USER,
	RCK_XDG_ICON_THEME_LOCATION_SYSTEM,
	RCK_XDG_ICON_THEME_LOCATION_LEGACY
} RCKXDGIconThemeLocation;

typedef enum {
	RCK_XDG_ICON_THEME_LOCALIZED_STRING_NAME,
	RCK_XDG_ICON_THEME_LOCALIZED_STRING_COMMENT
} RCKXDGIconThemeLocalizedString;

typedef enum {
	RCK_XDG_ICON_THEME_STRING_INTERNAL_NAME,
	RCK_XDG_ICON_THEME_STRING_EXAMPLE,
	RCK_XDG_ICON_THEME_STRING_PATH
} RCKXDGIconThemeString;

typedef enum {
	RCK_XDG_ICON_THEME_BOOL_HIDE
} RCKXDGIconThemeBool;

RCK_EXPORT RCKXDGIconThemes *rck_xdg_icon_themes_new(void);
RCK_EXPORT PRCList *rck_xdg_icon_themes_get_themes(RCKXDGIconThemes *themes);
RCK_EXPORT RCKXDGIconTheme *rck_xdg_icon_themes_get_theme_by_name(RCKXDGIconThemes *themes, const char *name);
RCK_EXPORT void rck_xdg_icon_themes_free(RCKXDGIconThemes *themes);

#define RCK_XDG_ICON_THEME(o) ((RCKXDGIconTheme*)o)
RCK_EXPORT const char *rck_xdg_icon_theme_get_localized_string(RCKXDGIconTheme *theme, RCKXDGIconThemeLocalizedString str, const char *locale);
RCK_EXPORT const char **rck_xdg_icon_theme_get_string_localizations(RCKXDGIconTheme *theme, RCKXDGIconThemeLocalizedString str);
RCK_EXPORT const char *rck_xdg_icon_theme_get_string(RCKXDGIconTheme *theme, RCKXDGIconThemeString str);
RCK_EXPORT PRBool rck_xdg_icon_theme_get_bool(RCKXDGIconTheme *theme, RCKXDGIconThemeBool bl, PRBool *ret);
RCK_EXPORT RCKXDGIconThemeLocation rck_xdg_icon_theme_get_location(RCKXDGIconTheme *theme);
RCK_EXPORT const char *rck_xdg_icon_theme_lookup_icon(RCKXDGIconTheme *theme, const char *name, unsigned int size, unsigned int scale);
RCK_EXPORT pixman_image_t *rck_xdg_icon_theme_load_icon(RCKXDGIconTheme *theme, const char *name, unsigned int size, unsigned int scale);
#define rck_xdg_icon_theme_free_icon_path(x) PL_strfree((char*)x)

/* DESKTOP SETTINGS */
typedef struct _RCKDesktopSettings RCKDesktopSettings;

typedef enum {
	RCK_DESKTOP_SETTINGS_BACKEND_UNKNOWN,
	RCK_DESKTOP_SETTINGS_BACKEND_XSETTINGS,
	RCK_DESKTOP_SETTINGS_BACKEND_PORTAL
} RCKDesktopSettingsBackend;

typedef enum {
	RCK_DESKTOP_SETTING_THEME_UNKNOWN,
	RCK_DESKTOP_SETTING_THEME_LIGHT,
	RCK_DESKTOP_SETTING_THEME_DARK
} RCKDesktopSettingTheme;

typedef enum {
	RCK_DESKTOP_SETTING_CONTRAST_UNKNOWN,
	RCK_DESKTOP_SETTING_CONTRAST_STANDARD,
	RCK_DESKTOP_SETTING_CONTRAST_HIGHER,
	RCK_DESKTOP_SETTING_CONTRAST_LOWER
} RCKDesktopSettingContrast;

typedef enum {
	RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_UNKNOWN,
	RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_DEFAULT,
	RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_OFF,
	RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_ON
} RCKDesktopSettingDefaultableBool;

typedef enum {
	RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_UNKNOWN,
	RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_NONE,
	RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_SLIGHT,
	RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_MEDIUM,
	RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_FULL,
} RCKDesktopSettingTextHintingStyle;

typedef enum {
	RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_UNKNOWN,
	RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_NONE,
	RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_RGB,
	RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_BGR,
	RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_VRGB,
	RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_VBGR
} RCKDesktopSettingTextSubpixel;

typedef enum {
	RCK_DESKTOP_SETTING_UNKNOWN,
	RCK_DESKTOP_SETTING_THEME,
	RCK_DESKTOP_SETTING_CONTRAST,
	RCK_DESKTOP_SETTING_TEXT_DPI,
	RCK_DESKTOP_SETTING_TEXT_AA,
	RCK_DESKTOP_SETTING_TEXT_HINTING,
	RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE,
	RCK_DESKTOP_SETTING_TEXT_SUBPIXEL,
	RCK_DESKTOP_SETTING_TEXT_FONT,
	RCK_DESKTOP_SETTING_ICON_THEME
} RCKDesktopSetting;

typedef void (*RCKDesktopSettingListener)(RCKDesktopSettings *, RCKDesktopSetting, void *);

RCK_EXPORT RCKDesktopSettings *rck_desktop_settings_new(void);
RCK_EXPORT RCKDesktopSettingsBackend rck_desktop_settings_get_backend(RCKDesktopSettings *setiings);
RCK_EXPORT PRBool rck_desktop_settings_get_value(RCKDesktopSettings *setiings, RCKDesktopSetting setting, ...);
RCK_EXPORT PRBool rck_desktop_settings_get_value_va_list(RCKDesktopSettings *setiings, RCKDesktopSetting setting, va_list va);
RCK_EXPORT PRBool rck_desktop_settings_add_change_listener(RCKDesktopSettings *setiings, RCKDesktopSettingListener listener, void *udata);
RCK_EXPORT PRBool rck_desktop_settings_remove_change_listener(RCKDesktopSettings *setiings, RCKDesktopSettingListener listener, void *udata);
RCK_EXPORT void rck_desktop_settings_pump_events(RCKDesktopSettings *setiings);
RCK_EXPORT void rck_desktop_settings_destroy(RCKDesktopSettings *setiings);

/* TODO: GIO-like filesystem object icon getter */
/* TODO: Move fallback toolkit PR into here */
/* TODO: Metatoolkit (QT/GTK/Fallback) wrapper */
/* TODO: Wayland compositor for embedding GTK/Qt menus into SDL? */

#ifdef __cplusplus
}
#endif 

#endif /* LIBRCK_H */
