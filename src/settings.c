#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include "rck.h"
#include "xsettings-client.h"

typedef struct {
	PRCList link;
	RCKDesktopSettingListener listener;
	void *udata;
} IRCKListener;

struct _RCKDesktopSettings {
	RCKDesktopSettingsBackend backend;
	
	PRCList listeners;

	PRBool (*get_value)(RCKDesktopSettings *, RCKDesktopSetting, va_list);
	void (*pump)(RCKDesktopSettings *);
	void (*destroy)(RCKDesktopSettings *);
};

typedef struct {
	RCKDesktopSettings parent;

	Display *dpy;
	int screen;
	PRBool close_dpy;
	XSettingsClient *client;
} RCKDesktopSettingsXSettings;

typedef struct {
	int type;
	dbus_uint32_t u;
	dbus_int32_t i;
	PRFloat64 d;
	char *s;
} PortalValue;

typedef struct {
	const char *ns;
	const char *key;
	RCKDesktopSetting setting;
} PortalMap;

#define PORTAL_MAP_MAX 32

typedef struct {
	RCKDesktopSettings parent;

	DBusConnection *conn;
	PortalMap map[PORTAL_MAP_MAX];
	unsigned int map_len;
} RCKDesktopSettingsPortal;

#define PORTAL_DESTINATION "org.freedesktop.portal.Desktop"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_INTERFACE "org.freedesktop.portal.Settings"
#define PORTAL_METHOD "Read"
#define SIGNAL_INTERFACE PORTAL_INTERFACE
#define SIGNAL_NAME "SettingChanged"
#define PORTAL_NS_FDO "org.freedesktop.appearance"
#define PORTAL_NS_GNOME "org.gnome.desktop.interface"
#define PORTAL_NS_GNOME_A11Y "org.gnome.desktop.a11y.interface"
#define PORTAL_NS_KDE_GENERAL "org.kde.kdeglobals.General"
#define PORTAL_NS_KDE_ICONS "org.kde.kdeglobals.Icons"
#define PORTAL_MATCH_RULE "type='signal',interface='"SIGNAL_INTERFACE"',member='"SIGNAL_NAME"'"

RCKDesktopSettingsBackend rck_desktop_settings_get_backend(RCKDesktopSettings *setiings) {
	if (setiings) {
		return setiings->backend;
	} else {
		return RCK_DESKTOP_SETTINGS_BACKEND_UNKNOWN;
	}
}

PRBool rck_desktop_settings_get_value_va_list(RCKDesktopSettings *setiings, RCKDesktopSetting setting, va_list va) {
	PRBool res;
    
	if (!setiings) {
		return PR_FALSE;
	}
	
    return setiings->get_value(setiings, setting, va);
}


PRBool rck_desktop_settings_get_value(RCKDesktopSettings *setiings, RCKDesktopSetting setting, ...) {
	PRBool res;
    va_list args;
    
	if (!setiings) {
		return PR_FALSE;
	}
	
    va_start(args, setting);
    res = rck_desktop_settings_get_value_va_list(setiings, setting, args);
    va_end(args);
    return res;
}

PRBool rck_desktop_settings_add_change_listener(RCKDesktopSettings *setiings, RCKDesktopSettingListener listener, void *udata) {
	IRCKListener *slistener;
	PRCList *head, *cursor;

	if (!setiings || !listener) {
		return PR_FALSE;
	}
	
	head = &setiings->listeners;
    cursor = PR_LIST_HEAD(head);
    while (cursor != head) {
		slistener = RCK_PR_CLIST_ENTRY(cursor, IRCKListener, link);
		if (slistener->listener == listener && slistener->udata == udata) {
			return PR_FALSE;
		}
        cursor = PR_NEXT_LINK(cursor);
    }

	slistener = PR_NEW(IRCKListener);
	slistener->listener = listener;
	slistener->udata = udata;
	PR_INIT_CLIST(&slistener->link);
	PR_APPEND_LINK(&slistener->link, &setiings->listeners);
	
	return PR_TRUE;
}

PRBool rck_desktop_settings_remove_change_listener(RCKDesktopSettings *setiings, RCKDesktopSettingListener listener, void *udata) {
	IRCKListener *slistener;
	PRCList *head, *cursor, *p;

	if (!setiings || !listener) {
		return PR_FALSE;
	}
	
	head = &setiings->listeners;
    cursor = PR_LIST_HEAD(head);
    while (cursor != head) {
		p = NULL;
		
		slistener = RCK_PR_CLIST_ENTRY(cursor, IRCKListener, link);
		if (slistener->listener == listener && slistener->udata == udata) {
			p = cursor;
		}
		
        cursor = PR_NEXT_LINK(cursor);
		if (p) {
			PR_REMOVE_LINK(&slistener->link);
			PR_Free(slistener);
			return PR_TRUE;
		}
    }

	return PR_FALSE;
}

void rck_desktop_settings_pump_events(RCKDesktopSettings *setiings) {
	if (setiings) {
		setiings->pump(setiings);
	}
}

void rck_desktop_settings_destroy(RCKDesktopSettings *setiings) {
	IRCKListener *slistener;
	PRCList *head, *cursor;

	if (!setiings) {
		return;
	}
	
	head = &setiings->listeners;
    cursor = PR_LIST_HEAD(head);
    while (cursor != head) {
		slistener = RCK_PR_CLIST_ENTRY(cursor, IRCKListener, link);
        cursor = PR_NEXT_LINK(cursor);
		PR_Free(slistener);
    }

	setiings->destroy(setiings);
}

static const char *enum_to_xsettings_name(RCKDesktopSetting s) {
	switch (s) {
		case RCK_DESKTOP_SETTING_TEXT_DPI:
			return "Xft/DPI";
			break;
		case RCK_DESKTOP_SETTING_TEXT_AA:
			return "Xft/Antialias";
			break;
		case RCK_DESKTOP_SETTING_TEXT_HINTING:
			return "Xft/Hinting";
			break;
		case RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE:
			return "Xft/HintStyle";
			break;
		case RCK_DESKTOP_SETTING_TEXT_SUBPIXEL:
			return "Xft/RGBA";
			break;
		case RCK_DESKTOP_SETTING_TEXT_FONT:
			return "Gtk/FontName";
			break;
		case RCK_DESKTOP_SETTING_ICON_THEME:
			return "Net/IconThemeName";
			break;
		default:
			return NULL;
	}	
}
 
static RCKDesktopSetting xsettings_name_to_enum(const char *n) {
	if (!strcmp(n, "Xft/DPI")) {
		return RCK_DESKTOP_SETTING_TEXT_DPI;
	}

	if (!strcmp(n, "Xft/Antialias")) {
		return RCK_DESKTOP_SETTING_TEXT_AA;
	}

	if (!strcmp(n, "Xft/HintStyle")) {
		return RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE;
	}

	if (!strcmp(n, "Xft/Hinting")) {
		return RCK_DESKTOP_SETTING_TEXT_HINTING;
	}
	
	if (!strcmp(n, "Xft/RGBA")) {
		return RCK_DESKTOP_SETTING_TEXT_SUBPIXEL;
	}

	if (!strcmp(n, "Gtk/FontName")) {
		return RCK_DESKTOP_SETTING_TEXT_FONT;
	}
	
	if (!strcmp(n, "Net/IconThemeName")) {
		return RCK_DESKTOP_SETTING_ICON_THEME;
	}

	return RCK_DESKTOP_SETTING_UNKNOWN;
}

static void xsettings_backend_notify(const char *name, XSettingsAction action, XSettingsSetting *setting, void *udata) {
	RCKDesktopSettingsXSettings *xs;
	RCKDesktopSettings *s;
	RCKDesktopSetting rs;
	RCKDesktopSetting ars;
	PRCList *head, *cursor;

	xs = (RCKDesktopSettingsXSettings *)udata;
	s = (RCKDesktopSettings *)udata;

	if (action == XSETTINGS_ACTION_DELETED) {
		return;
	}
	
	ars = RCK_DESKTOP_SETTING_UNKNOWN;
	rs = xsettings_name_to_enum(name);
	if (!strcmp(name, "Gtk/ApplicationPreferDarkTheme")) {
		rs = RCK_DESKTOP_SETTING_THEME;
	}

	if (!strcmp(name, "Gtk/InterfaceContrast")) {
		rs = RCK_DESKTOP_SETTING_CONTRAST;
	}
	
	if (!strcmp(name, "Net/ThemeName")) {
		rs = RCK_DESKTOP_SETTING_THEME;
		ars = RCK_DESKTOP_SETTING_CONTRAST;
	}
	
	if (rs == RCK_DESKTOP_SETTING_UNKNOWN) {
		return;
	}
	
	head = &s->listeners;
    cursor = PR_LIST_HEAD(head);
    while (cursor != head) {
		if (cursor) {
			IRCKListener *slistener;
			
			slistener = RCK_PR_CLIST_ENTRY(cursor, IRCKListener, link);
			slistener->listener(s, rs, slistener->udata);
			if (ars != RCK_DESKTOP_SETTING_UNKNOWN) {
				slistener->listener(s, ars, slistener->udata);
			}
			cursor = PR_NEXT_LINK(cursor);
		} else {
			break;
		}
    }
}

static void xsettings_backend_pump(RCKDesktopSettings *s) {
	RCKDesktopSettingsXSettings *xs;
	int ewaiting;
	
	xs = (RCKDesktopSettingsXSettings *)s;
	ewaiting = XPending(xs->dpy);
	while (ewaiting > 0) {
		XEvent event;
		
		XNextEvent(xs->dpy, &event);
		ixsettings_client_process_event(xs->client, &event);		
		ewaiting--;
	}
}

static void xsettings_backend_destroy(RCKDesktopSettings *s) {
	RCKDesktopSettingsXSettings *xs;
	
	xs = (RCKDesktopSettingsXSettings *)s;
	ixsettings_client_destroy(xs->client);
	if (xs->close_dpy) {
		XCloseDisplay(xs->dpy);
	}
	PR_Free(xs);
}

static PRBool xsettings_backend_get(RCKDesktopSettings *s, RCKDesktopSetting k, va_list va) {
	RCKDesktopSettingsXSettings *xs;
	XSettingsSetting *setting;
	const char *n;
	void *vp;
	
	xs = (RCKDesktopSettingsXSettings *)s;
	n = enum_to_xsettings_name(k);
	vp = va_arg(va, void*);

	if (n) {
		switch (k) {
			case RCK_DESKTOP_SETTING_TEXT_DPI:
				if (ixsettings_client_get_setting(xs->client, n, &setting) == XSETTINGS_SUCCESS) {
					int *p;
					
					p = vp;
					if (p && setting && setting->type == XSETTINGS_TYPE_INT) {
						*p = setting->data.v_int/1024;
					}
					ixsettings_setting_free(setting);
					return PR_TRUE;
				}
				break;
			case RCK_DESKTOP_SETTING_TEXT_HINTING:
			case RCK_DESKTOP_SETTING_TEXT_AA:
				if (ixsettings_client_get_setting(xs->client, n, &setting) == XSETTINGS_SUCCESS) {
					RCKDesktopSettingDefaultableBool *p;
					
					p = vp;
					if (p && setting && setting->type == XSETTINGS_TYPE_INT) {
						if (setting->data.v_int == -1) {
							*p = RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_DEFAULT;
						} else if (!setting->data.v_int) {
							*p = RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_OFF;
						} else if (setting->data.v_int == 1) {
							*p = RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_ON;
						} else {
							*p = RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_UNKNOWN;
						}
					}
					ixsettings_setting_free(setting);
					return PR_TRUE;
				}
				break;
			case RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE:
				if (ixsettings_client_get_setting(xs->client, n, &setting) == XSETTINGS_SUCCESS) {
					RCKDesktopSettingTextHintingStyle *p;
					
					p = vp;
					if (p && setting && setting->type == XSETTINGS_TYPE_STRING) {
						if (!PL_strcasecmp(setting->data.v_string, "hintnone")) {
							*p = RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_NONE;
						} else if (!PL_strcasecmp(setting->data.v_string, "hintslight")) {
							*p = RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_SLIGHT;
						} else if (!PL_strcasecmp(setting->data.v_string, "hintmedium")) {
							*p = RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_MEDIUM;
						} else if (!PL_strcasecmp(setting->data.v_string, "hintfull")) {
							*p = RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_FULL;
						} else {
							*p = RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_UNKNOWN;
						}
					}
					ixsettings_setting_free(setting);
					return PR_TRUE;
				}
				break;
			case RCK_DESKTOP_SETTING_TEXT_SUBPIXEL:
				if (ixsettings_client_get_setting(xs->client, n, &setting) == XSETTINGS_SUCCESS) {
					RCKDesktopSettingTextSubpixel *p;
					
					p = vp;
					if (p && setting && setting->type == XSETTINGS_TYPE_STRING) {
						if (!PL_strcasecmp(setting->data.v_string, "none")) {
							*p = RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_NONE;
						} else if (!PL_strcasecmp(setting->data.v_string, "rgb")) {
							*p = RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_RGB;
						} else if (!PL_strcasecmp(setting->data.v_string, "bgr")) {
							*p = RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_BGR;
						} else if (!PL_strcasecmp(setting->data.v_string, "vrgb")) {
							*p = RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_VRGB;
						} else if (!PL_strcasecmp(setting->data.v_string, "vbgr")) {
							*p = RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_VBGR;
						} else {
							*p = RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_UNKNOWN;
						}
					}
					ixsettings_setting_free(setting);
					return PR_TRUE;
				}
				break;
			case RCK_DESKTOP_SETTING_TEXT_FONT:
			case RCK_DESKTOP_SETTING_ICON_THEME:
				if (ixsettings_client_get_setting(xs->client, n, &setting) == XSETTINGS_SUCCESS) {
					char **p;
					
					p = vp;
					if (p && setting && setting->type == XSETTINGS_TYPE_STRING) {
						*p = PL_strdup(setting->data.v_string);
					}
					ixsettings_setting_free(setting);
					return PR_TRUE;
				}
				break;
			default:
				return PR_FALSE;
		}	
	} else if (!n && k == RCK_DESKTOP_SETTING_THEME) {
		if (ixsettings_client_get_setting(xs->client, "Gtk/ApplicationPreferDarkTheme", &setting) == XSETTINGS_SUCCESS) {
			RCKDesktopSettingTheme *p;
			
			p = vp;
			if (p && setting && setting->type == XSETTINGS_TYPE_INT) {
				if (!setting->data.v_int) {
					*p = RCK_DESKTOP_SETTING_THEME_LIGHT;
				} else if (setting->data.v_int == 1) {
					*p = RCK_DESKTOP_SETTING_THEME_DARK;
				} else {
					*p = RCK_DESKTOP_SETTING_THEME_UNKNOWN;
				}
			}
			ixsettings_setting_free(setting);
			return PR_TRUE;
		}
		
		if (ixsettings_client_get_setting(xs->client, "Net/ThemeName", &setting) == XSETTINGS_SUCCESS) {
			RCKDesktopSettingTheme *p;
			
			p = vp;
			if (p && setting && setting->type == XSETTINGS_TYPE_STRING) {
				if (!rck_strcasesuffix(setting->data.v_string, "dark")) {
					*p = RCK_DESKTOP_SETTING_THEME_DARK;
				} else {
					*p = RCK_DESKTOP_SETTING_THEME_LIGHT;
				}
			}
			ixsettings_setting_free(setting);
			return PR_TRUE;
		}
	} else if (!n && k == RCK_DESKTOP_SETTING_CONTRAST) {
		if (ixsettings_client_get_setting(xs->client, "Gtk/InterfaceContrast", &setting) == XSETTINGS_SUCCESS) {
			RCKDesktopSettingContrast *p;
			
			p = vp;
			if (p && setting && setting->type == XSETTINGS_TYPE_STRING) {
				if (!PL_strcasecmp(setting->data.v_string, "high")) {
					*p = RCK_DESKTOP_SETTING_CONTRAST_HIGHER;
				} else if (!PL_strcasecmp(setting->data.v_string, "normal")) {
					*p = RCK_DESKTOP_SETTING_CONTRAST_STANDARD;
				} else if (!PL_strcasecmp(setting->data.v_string, "low")) {
					*p = RCK_DESKTOP_SETTING_CONTRAST_LOWER;
				} else {
					*p = RCK_DESKTOP_SETTING_CONTRAST_UNKNOWN;
				}
			}
			ixsettings_setting_free(setting);
			return PR_TRUE;
		}
		
		if (ixsettings_client_get_setting(xs->client, "Net/ThemeName", &setting) == XSETTINGS_SUCCESS) {
			RCKDesktopSettingContrast *p;
			
			p = vp;
			if (p && setting && setting->type == XSETTINGS_TYPE_STRING) {
				if (rck_strcaseprefix(setting->data.v_string, "HighContrast")) {
					*p = RCK_DESKTOP_SETTING_CONTRAST_HIGHER;
				} else {
					*p = RCK_DESKTOP_SETTING_CONTRAST_STANDARD;
				}
			}
			ixsettings_setting_free(setting);
			return PR_TRUE;
		}
	} else {
		return PR_FALSE;
	}

	return PR_FALSE;
}

static RCKDesktopSettings *xsettings_backend_new(void) {
	RCKDesktopSettingsXSettings *xs;
	RCKDesktopSettings *s;
	
	xs = PR_NEW(RCKDesktopSettingsXSettings);
	s = (RCKDesktopSettings *)xs;
	
	if (!xs) {
		return NULL;
	}
	
	s->backend = RCK_DESKTOP_SETTINGS_BACKEND_XSETTINGS;
	PR_INIT_CLIST(&s->listeners);
	xs->dpy = XOpenDisplay(NULL);
	if (!xs->dpy) {
		PR_Free(xs);
		return NULL;
	}
	
	xs->close_dpy = PR_TRUE;
	xs->screen = DefaultScreen(xs->dpy);
	xs->client = ixsettings_client_new(xs->dpy, xs->screen, xsettings_backend_notify, NULL, xs);
	if (!xs->client) {
		if (xs->close_dpy) {
			XCloseDisplay(xs->dpy);
		}
		PR_Free(xs);
		return NULL;
	}
	
	s->pump = xsettings_backend_pump;
	s->get_value = xsettings_backend_get;
	s->destroy = xsettings_backend_destroy;
	
	xsettings_backend_pump(s);
	
	return s;
}

static void pv_clear(PortalValue *v) {
	if (v->s) {
		PR_Free(v->s);
	}
	memset(v, 0, sizeof(*v));
	v->type = DBUS_TYPE_INVALID;
}

static PRBool pv_str_is(const PortalValue *v, const char *s) {
	return (v->type == DBUS_TYPE_STRING && v->s && !PL_strcasecmp(v->s, s)) ? PR_TRUE : PR_FALSE;
}

static PRBool pv_to_int(const PortalValue *v, PRInt32 *out) {
	char *end;
	long l;

	switch (v->type) {
		case DBUS_TYPE_UINT32:
			*out = (PRInt32)v->u;
			return PR_TRUE;
		case DBUS_TYPE_INT32:
			*out = (PRInt32)v->i;
			return PR_TRUE;
		case DBUS_TYPE_BOOLEAN:
			*out = v->u ? 1 : 0;
			return PR_TRUE;
		case DBUS_TYPE_DOUBLE:
			*out = (PRInt32)v->d;
			return PR_TRUE;
		case DBUS_TYPE_STRING:
			if (!v->s || !*v->s) {
				return PR_FALSE;
			}
			l = strtol(v->s, &end, 10);
			if (*end) {
				return PR_FALSE;
			}
			*out = (PRInt32)l;
			return PR_TRUE;
		default:
			return PR_FALSE;
	}
}

static PRBool pv_to_double(const PortalValue *v, PRFloat64 *out) {
	char *end;

	switch (v->type) {
		case DBUS_TYPE_DOUBLE:
			*out = v->d;
			return PR_TRUE;
		case DBUS_TYPE_UINT32:
			*out = (PRFloat64)v->u;
			return PR_TRUE;
		case DBUS_TYPE_INT32:
			*out = (PRFloat64)v->i;
			return PR_TRUE;
		case DBUS_TYPE_STRING:
			if (!v->s || !*v->s) {
				return PR_FALSE;
			}
			*out = strtod(v->s, &end);
			return *end ? PR_FALSE : PR_TRUE;
		default:
			return PR_FALSE;
	}
}

static PRBool pv_to_bool(const PortalValue *v, PRBool *out) {
	switch (v->type) {
		case DBUS_TYPE_BOOLEAN:
		case DBUS_TYPE_UINT32:
			*out = v->u ? PR_TRUE : PR_FALSE;
			return PR_TRUE;
		case DBUS_TYPE_INT32:
			*out = v->i ? PR_TRUE : PR_FALSE;
			return PR_TRUE;
		case DBUS_TYPE_DOUBLE:
			*out = v->d != 0 ? PR_TRUE : PR_FALSE;
			return PR_TRUE;
		case DBUS_TYPE_STRING:
			if (!v->s) {
				return PR_FALSE;
			}
			if (!PL_strcasecmp(v->s, "true") || !PL_strcasecmp(v->s, "1") || !PL_strcasecmp(v->s, "yes") || !PL_strcasecmp(v->s, "on")) {
				*out = PR_TRUE;
				return PR_TRUE;
			}
			if (!PL_strcasecmp(v->s, "false") || !PL_strcasecmp(v->s, "0") || !PL_strcasecmp(v->s, "no") || !PL_strcasecmp(v->s, "off")) {
				*out = PR_FALSE;
				return PR_TRUE;
			}
			return PR_FALSE;
		default:
			return PR_FALSE;
	}
}

static PRBool portal_read(DBusConnection *conn, const char *ns, const char *key, PortalValue *v) {
	DBusMessage *msg;
	DBusMessage *reply;
	const char *str;
	DBusMessageIter iter;
	DBusMessageIter sub;
	DBusError error;
	dbus_bool_t b;
	PRBool ok;
	int depth;

	memset(v, 0, sizeof(*v));
	v->type = DBUS_TYPE_INVALID;

	msg = dbus_message_new_method_call(PORTAL_DESTINATION, PORTAL_PATH, PORTAL_INTERFACE, PORTAL_METHOD);
	if (!msg) {
		return PR_FALSE;
	}

	if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &ns, DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID)) {
		dbus_message_unref(msg);
		return PR_FALSE;
	}

	dbus_error_init(&error);
	reply = dbus_connection_send_with_reply_and_block(conn, msg, 500, &error);
	dbus_message_unref(msg);
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
	}

	if (!reply) {
		return PR_FALSE;
	}

	ok = PR_FALSE;
	if (dbus_message_iter_init(reply, &iter)) {
		depth = 0;
		while (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT && depth < 4) {
			dbus_message_iter_recurse(&iter, &sub);
			iter = sub;
			depth++;
		}

		switch (dbus_message_iter_get_arg_type(&iter)) {
			case DBUS_TYPE_STRING:
			case DBUS_TYPE_OBJECT_PATH:
				str = NULL;
				dbus_message_iter_get_basic(&iter, &str);
				if (str) {
					v->s = PL_strdup(str);
					if (v->s) {
						v->type = DBUS_TYPE_STRING;
						ok = PR_TRUE;
					}
				}
				break;
			case DBUS_TYPE_UINT32:
				dbus_message_iter_get_basic(&iter, &v->u);
				v->type = DBUS_TYPE_UINT32;
				ok = PR_TRUE;
				break;
			case DBUS_TYPE_INT32:
				dbus_message_iter_get_basic(&iter, &v->i);
				v->type = DBUS_TYPE_INT32;
				ok = PR_TRUE;
				break;
			case DBUS_TYPE_BOOLEAN:
				b = 0;
				dbus_message_iter_get_basic(&iter, &b);
				v->u = b ? 1 : 0;
				v->type = DBUS_TYPE_BOOLEAN;
				ok = PR_TRUE;
				break;
			case DBUS_TYPE_DOUBLE:
				dbus_message_iter_get_basic(&iter, &v->d);
				v->type = DBUS_TYPE_DOUBLE;
				ok = PR_TRUE;
				break;
			default:
				break;
		}
	}

	dbus_message_unref(reply);
	return ok;
}

static PRBool portal_read_string(DBusConnection *conn, const char *ns, const char *key, PortalValue *v) {
	if (!portal_read(conn, ns, key, v)) {
		return PR_FALSE;
	}

	if (v->type != DBUS_TYPE_STRING || !v->s || !*v->s) {
		pv_clear(v);
		return PR_FALSE;
	}

	return PR_TRUE;
}

static PRBool portal_probe(DBusConnection *conn) {
	PortalValue v;
	PRBool ok;

	ok = PR_FALSE;
	if (portal_read(conn, PORTAL_NS_FDO, "color-scheme", &v) || portal_read(conn, PORTAL_NS_GNOME, "font-name", &v) || portal_read(conn, PORTAL_NS_KDE_GENERAL, "font", &v)) {
		ok = PR_TRUE;
		pv_clear(&v);
	}

	return ok;
}

static const char *qt5_weight_name(int w) {
	if (w <= 6) {
		return "Thin";
	} else if (w <= 18) {
		return "Ultra-Light";
	} else if (w <= 37) {
		return "Light";
	} else if (w <= 53) {
		return "";
	} else if (w <= 60) {
		return "Medium";
	} else if (w <= 69) {
		return "Semi-Bold";
	} else if (w <= 78) {
		return "Bold";
	} else if (w <= 84) {
		return "Ultra-Bold";
	}
	return "Heavy";
}

static const char *qt6_weight_name(int w) {
	if (w <= 150) {
		return "Thin";
	} else if (w <= 250) {
		return "Ultra-Light";
	} else if (w <= 350) {
		return "Light";
	} else if (w <= 449) {
		return "";
	} else if (w <= 549) {
		return "Medium";
	} else if (w <= 649) {
		return "Semi-Bold";
	} else if (w <= 749) {
		return "Bold";
	} else if (w <= 849) {
		return "Ultra-Bold";
	}
	return "Heavy";
}

static char *font_qt_to_pango(const char *str) {
	char *buf;
	char *fields[20];
	char *p;
	char *end;
	char *res;
	char *o;
	const char *wname;
	const char *sname;
	PRFloat64 size;
	long pixels;
	long weight;
	long italic;
	int n;

	if (!strchr(str, ',')) {
		return PL_strdup(str);
	}

	buf = PL_strdup(str);
	if (!buf) {
		return NULL;
	}

	n = 0;
	p = buf;
	fields[n++] = p;
	while (*p && n < 20) {
		if (*p == ',') {
			*p = '\0';
			fields[n++] = p + 1;
		}
		p++;
	}

	if (n < 2 || !*fields[0]) {
		PR_Free(buf);
		return PL_strdup(str);
	}

	size = strtod(fields[1], &end);
	if (end == fields[1]) {
		size = 0;
	}

	pixels = -1;
	if (n > 2) {
		pixels = strtol(fields[2], NULL, 10);
	}

	weight = -1;
	if (n > 4) {
		weight = strtol(fields[4], NULL, 10);
	}

	italic = 0;
	if (n > 5) {
		italic = strtol(fields[5], NULL, 10);
	}

	wname = "";
	if (weight >= 0) {
		if (n >= 16 || weight > 99) {
			wname = qt6_weight_name((int)weight);
		} else {
			wname = qt5_weight_name((int)weight);
		}
	}

	sname = "";
	if (italic == 1) {
		sname = "Italic";
	} else if (italic == 2) {
		sname = "Oblique";
	}

	if (size > 0) {
		res = PR_smprintf("%s%s%s%s%s %g", fields[0], wname[0] ? " " : "", wname, sname[0] ? " " : "", sname, size);
	} else if (pixels > 0) {
		res = PR_smprintf("%s%s%s%s%s %dpx", fields[0], wname[0] ? " " : "", wname, sname[0] ? " " : "", sname, (int)pixels);
	} else {
		res = PR_smprintf("%s%s%s%s%s", fields[0], wname[0] ? " " : "", wname, sname[0] ? " " : "", sname);
	}

	o = res;
	res = PL_strdup(res);
	PR_smprintf_free(o);
	PL_strfree(buf);
	return res;
}

static RCKDesktopSettingTextHintingStyle parse_hint_style(const char *s) {
	if (!PL_strncasecmp(s, "hint", 4)) {
		s += 4;
	}

	if (!PL_strcasecmp(s, "none")) {
		return RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_NONE;
	} else if (!PL_strcasecmp(s, "slight")) {
		return RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_SLIGHT;
	} else if (!PL_strcasecmp(s, "medium")) {
		return RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_MEDIUM;
	} else if (!PL_strcasecmp(s, "full")) {
		return RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_FULL;
	}

	return RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_UNKNOWN;
}

static RCKDesktopSettingTextSubpixel parse_subpixel(const char *s) {
	if (!PL_strcasecmp(s, "none")) {
		return RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_NONE;
	} else if (!PL_strcasecmp(s, "rgb")) {
		return RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_RGB;
	} else if (!PL_strcasecmp(s, "bgr")) {
		return RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_BGR;
	} else if (!PL_strcasecmp(s, "vrgb")) {
		return RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_VRGB;
	} else if (!PL_strcasecmp(s, "vbgr")) {
		return RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_VBGR;
	}

	return RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_UNKNOWN;
}

static PRBool portal_get_theme(DBusConnection *c, RCKDesktopSettingTheme *out) {
	PortalValue v;
	PRInt32 n;
	PRBool nopref;
	PRBool found;

	nopref = PR_FALSE;

	if (portal_read(c, PORTAL_NS_FDO, "color-scheme", &v)) {
		found = PR_FALSE;
		if (pv_to_int(&v, &n)) {
			if (n == 1) {
				*out = RCK_DESKTOP_SETTING_THEME_DARK;
				found = PR_TRUE;
			} else if (n == 2) {
				*out = RCK_DESKTOP_SETTING_THEME_LIGHT;
				found = PR_TRUE;
			} else {
				nopref = PR_TRUE;
			}
		}
		pv_clear(&v);
		if (found) {
			return PR_TRUE;
		}
	}

	if (portal_read_string(c, PORTAL_NS_GNOME, "color-scheme", &v)) {
		found = PR_FALSE;
		if (pv_str_is(&v, "prefer-dark")) {
			*out = RCK_DESKTOP_SETTING_THEME_DARK;
			found = PR_TRUE;
		} else if (pv_str_is(&v, "prefer-light")) {
			*out = RCK_DESKTOP_SETTING_THEME_LIGHT;
			found = PR_TRUE;
		} else {
			nopref = PR_TRUE;
		}
		pv_clear(&v);
		if (found) {
			return PR_TRUE;
		}
	}

	if (portal_read_string(c, PORTAL_NS_GNOME, "gtk-theme", &v)) {
		*out = (!rck_strcasesuffix(v.s, "dark")) ? RCK_DESKTOP_SETTING_THEME_DARK : RCK_DESKTOP_SETTING_THEME_LIGHT;
		pv_clear(&v);
		return PR_TRUE;
	}

	if (portal_read_string(c, PORTAL_NS_KDE_GENERAL, "ColorScheme", &v)) {
		*out = (!rck_strcasesuffix(v.s, "dark")) ? RCK_DESKTOP_SETTING_THEME_DARK : RCK_DESKTOP_SETTING_THEME_LIGHT;
		pv_clear(&v);
		return PR_TRUE;
	}

	if (nopref) {
		*out = RCK_DESKTOP_SETTING_THEME_UNKNOWN;
		return PR_TRUE;
	}

	return PR_FALSE;
}

static PRBool portal_get_contrast(DBusConnection *c, RCKDesktopSettingContrast *out) {
	PortalValue v;
	PRInt32 n;
	PRBool b;
	PRBool found;
	PRBool high;

	if (portal_read(c, PORTAL_NS_FDO, "contrast", &v)) {
		found = PR_FALSE;
		if (pv_to_int(&v, &n)) {
			*out = n == 1 ? RCK_DESKTOP_SETTING_CONTRAST_HIGHER : RCK_DESKTOP_SETTING_CONTRAST_STANDARD;
			found = PR_TRUE;
		}
		pv_clear(&v);
		if (found) {
			return PR_TRUE;
		}
	}

	found = PR_FALSE;
	high = PR_FALSE;

	if (portal_read(c, PORTAL_NS_GNOME_A11Y, "high-contrast", &v)) {
		if (pv_to_bool(&v, &b)) {
			found = PR_TRUE;
			high = b;
		}
		pv_clear(&v);
	}

	if (portal_read_string(c, PORTAL_NS_GNOME, "gtk-theme", &v)) {
		found = PR_TRUE;
		if (rck_strcaseprefix(v.s, "HighContrast")) {
			high = PR_TRUE;
		}
		pv_clear(&v);
	}

	if (found) {
		*out = high ? RCK_DESKTOP_SETTING_CONTRAST_HIGHER : RCK_DESKTOP_SETTING_CONTRAST_STANDARD;
		return PR_TRUE;
	}

	return PR_FALSE;
}

static PRBool portal_get_dpi(DBusConnection *c, int *out) {
	PortalValue v;
	PRFloat64 f;
	PRInt32 n;
	PRBool ok;

	ok = PR_FALSE;
	if (portal_read(c, PORTAL_NS_GNOME, "text-scaling-factor", &v)) {
		if (pv_to_double(&v, &f) && f > 0) {
			*out = (int)(f * 96 + 0.5);
			ok = PR_TRUE;
		}
		pv_clear(&v);
		if (ok) {
			return PR_TRUE;
		}
	}

	if (portal_read(c, PORTAL_NS_KDE_GENERAL, "forceFontDPI", &v)) {
		if (pv_to_int(&v, &n) && n > 0) {
			*out = (int)n;
			ok = PR_TRUE;
		}
		pv_clear(&v);
		if (ok) {
			return PR_TRUE;
		}
	}

	return PR_FALSE;
}

static PRBool portal_get_aa(DBusConnection *c, RCKDesktopSettingDefaultableBool *out) {
	PortalValue v;
	PRBool b;
	PRBool ok;

	if (portal_read_string(c, PORTAL_NS_GNOME, "font-antialiasing", &v)) {
		if (pv_str_is(&v, "none")) {
			*out = RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_OFF;
		} else if (pv_str_is(&v, "grayscale") || pv_str_is(&v, "rgba")) {
			*out = RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_ON;
		} else {
			*out = RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_UNKNOWN;
		}
		pv_clear(&v);
		return PR_TRUE;
	}

	if (portal_read(c, PORTAL_NS_KDE_GENERAL, "XftAntialias", &v)) {
		ok = pv_to_bool(&v, &b);
		pv_clear(&v);
		if (ok) {
			*out = b ? RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_ON : RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_OFF;
			return PR_TRUE;
		}
	}

	return PR_FALSE;
}

static PRBool portal_get_hint_style(DBusConnection *c, RCKDesktopSettingTextHintingStyle *out) {
	PortalValue v;

	if (portal_read_string(c, PORTAL_NS_GNOME, "font-hinting", &v) ||
		portal_read_string(c, PORTAL_NS_KDE_GENERAL, "XftHintStyle", &v)) {
		*out = parse_hint_style(v.s);
		pv_clear(&v);
		return PR_TRUE;
	}

	return PR_FALSE;
}

static PRBool portal_get_hinting(DBusConnection *c, RCKDesktopSettingDefaultableBool *out) {
	RCKDesktopSettingTextHintingStyle style;

	if (!portal_get_hint_style(c, &style)) {
		return PR_FALSE;
	}

	if (style == RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_NONE) {
		*out = RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_OFF;
	} else if (style == RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE_UNKNOWN) {
		*out = RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_UNKNOWN;
	} else {
		*out = RCK_DESKTOP_SETTING_DEFAULTABLE_BOOL_ON;
	}

	return PR_TRUE;
}

static PRBool portal_get_subpixel(DBusConnection *c, RCKDesktopSettingTextSubpixel *out) {
	PortalValue v;
	PortalValue o;

	if (portal_read_string(c, PORTAL_NS_GNOME, "font-antialiasing", &v)) {
		if (pv_str_is(&v, "rgba")) {
			if (portal_read_string(c, PORTAL_NS_GNOME, "font-rgba-order", &o)) {
				*out = parse_subpixel(o.s);
				pv_clear(&o);
			} else {
				*out = RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_RGB;
			}
		} else {
			*out = RCK_DESKTOP_SETTING_TEXT_SUBPIXEL_NONE;
		}
		pv_clear(&v);
		return PR_TRUE;
	}

	if (portal_read_string(c, PORTAL_NS_KDE_GENERAL, "XftSubPixel", &v)) {
		*out = parse_subpixel(v.s);
		pv_clear(&v);
		return PR_TRUE;
	}

	return PR_FALSE;
}

static PRBool portal_get_font(DBusConnection *c, char **out) {
	PortalValue v;
	char *font;

	if (portal_read_string(c, PORTAL_NS_GNOME, "font-name", &v)) {
		*out = PL_strdup(v.s);
		v.s = NULL;
		return PR_TRUE;
	}

	if (portal_read_string(c, PORTAL_NS_KDE_GENERAL, "font", &v)) {
		font = font_qt_to_pango(v.s);
		pv_clear(&v);
		if (font) {
			*out = font;
			return PR_TRUE;
		}
	}

	return PR_FALSE;
}

static PRBool portal_get_icon_theme(DBusConnection *c, char **out) {
	PortalValue v;

	if (portal_read_string(c, PORTAL_NS_GNOME, "icon-theme", &v) || portal_read_string(c, PORTAL_NS_KDE_ICONS, "Theme", &v)) {
		*out = PL_strdup(v.s);
		v.s = NULL;
		return PR_TRUE;
	}

	return PR_FALSE;
}

static PRBool portal_backend_get(RCKDesktopSettings *s, RCKDesktopSetting k, va_list va) {
	RCKDesktopSettingsPortal *xs;
	void *vp;

	xs = (RCKDesktopSettingsPortal *)s;
	vp = va_arg(va, void*);
	if (!vp) {
		return PR_FALSE;
	}

	switch (k) {
		case RCK_DESKTOP_SETTING_TEXT_DPI:
			return portal_get_dpi(xs->conn, (int *)vp);
		case RCK_DESKTOP_SETTING_TEXT_AA:
			return portal_get_aa(xs->conn, (RCKDesktopSettingDefaultableBool *)vp);
		case RCK_DESKTOP_SETTING_TEXT_HINTING:
			return portal_get_hinting(xs->conn, (RCKDesktopSettingDefaultableBool *)vp);
		case RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE:
			return portal_get_hint_style(xs->conn, (RCKDesktopSettingTextHintingStyle *)vp);
		case RCK_DESKTOP_SETTING_TEXT_SUBPIXEL:
			return portal_get_subpixel(xs->conn, (RCKDesktopSettingTextSubpixel *)vp);
		case RCK_DESKTOP_SETTING_TEXT_FONT:
			return portal_get_font(xs->conn, (char **)vp);
		case RCK_DESKTOP_SETTING_ICON_THEME:
			return portal_get_icon_theme(xs->conn, (char **)vp);
		case RCK_DESKTOP_SETTING_THEME:
			return portal_get_theme(xs->conn, (RCKDesktopSettingTheme *)vp);
		case RCK_DESKTOP_SETTING_CONTRAST:
			return portal_get_contrast(xs->conn, (RCKDesktopSettingContrast *)vp);
		default:
			return PR_FALSE;
	}
}

static void portal_backend_notify(RCKDesktopSettings *s, RCKDesktopSetting rs) {
	IRCKListener *slistener;
	PRCList *head, *cursor, *next;

	head = &s->listeners;
	cursor = PR_LIST_HEAD(head);
	while (cursor != head) {
		next = PR_NEXT_LINK(cursor);
		slistener = RCK_PR_CLIST_ENTRY(cursor, IRCKListener, link);
		slistener->listener(s, rs, slistener->udata);
		cursor = next;
	}
}

static void portal_backend_pump(RCKDesktopSettings *s) {
	RCKDesktopSettingsPortal *xs;
	DBusDispatchStatus status;

	xs = (RCKDesktopSettingsPortal *)s;
	dbus_connection_read_write(xs->conn, 0);
	do {
		status = dbus_connection_dispatch(xs->conn);
	} while (status == DBUS_DISPATCH_DATA_REMAINS);
}

static DBusHandlerResult portal_backend_filter(DBusConnection *conn, DBusMessage *msg, void *data) {
	RCKDesktopSettings *s;
	DBusMessageIter iter;
	const PortalMap *m;
	const char *ns;
	const char *key;

	s = (RCKDesktopSettings *)data;
	if (!dbus_message_is_signal(msg, SIGNAL_INTERFACE, SIGNAL_NAME)) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (!dbus_message_iter_init(msg, &iter)) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	dbus_message_iter_get_basic(&iter, &ns);

	if (!dbus_message_iter_next(&iter)) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	dbus_message_iter_get_basic(&iter, &key);

	for (m = ((RCKDesktopSettingsPortal *)s)->map; m->ns; m++) {
		if (!strcmp(m->ns, ns) && !strcmp(m->key, key)) {
			portal_backend_notify(s, m->setting);
		}
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void portal_backend_destroy(RCKDesktopSettings *s) {
	RCKDesktopSettingsPortal *xs;

	xs = (RCKDesktopSettingsPortal *)s;
	dbus_connection_remove_filter(xs->conn, portal_backend_filter, s);
	dbus_bus_remove_match(xs->conn, PORTAL_MATCH_RULE, NULL);
	dbus_connection_unref(xs->conn);
	PR_Free(xs);
}

static void portal_map_add(RCKDesktopSettingsPortal *xs, const char *ns, const char *key, RCKDesktopSetting setting) {
	PortalMap *m;

	if (xs->map_len >= PORTAL_MAP_MAX) {
		return;
	}

	m = &xs->map[xs->map_len++];
	m->ns = ns;
	m->key = key;
	m->setting = setting;
}

static void portal_map_init(RCKDesktopSettingsPortal *xs) {
	xs->map_len = 0;

	portal_map_add(xs, PORTAL_NS_FDO, "color-scheme", RCK_DESKTOP_SETTING_THEME);
	portal_map_add(xs, PORTAL_NS_FDO, "contrast", RCK_DESKTOP_SETTING_CONTRAST);

	portal_map_add(xs, PORTAL_NS_GNOME, "color-scheme", RCK_DESKTOP_SETTING_THEME);
	portal_map_add(xs, PORTAL_NS_GNOME, "gtk-theme", RCK_DESKTOP_SETTING_THEME);
	portal_map_add(xs, PORTAL_NS_GNOME, "gtk-theme", RCK_DESKTOP_SETTING_CONTRAST);
	portal_map_add(xs, PORTAL_NS_GNOME_A11Y, "high-contrast", RCK_DESKTOP_SETTING_CONTRAST);
	portal_map_add(xs, PORTAL_NS_GNOME, "text-scaling-factor", RCK_DESKTOP_SETTING_TEXT_DPI);
	portal_map_add(xs, PORTAL_NS_GNOME, "font-antialiasing", RCK_DESKTOP_SETTING_TEXT_AA);
	portal_map_add(xs, PORTAL_NS_GNOME, "font-antialiasing", RCK_DESKTOP_SETTING_TEXT_SUBPIXEL);
	portal_map_add(xs, PORTAL_NS_GNOME, "font-hinting", RCK_DESKTOP_SETTING_TEXT_HINTING);
	portal_map_add(xs, PORTAL_NS_GNOME, "font-hinting", RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE);
	portal_map_add(xs, PORTAL_NS_GNOME, "font-rgba-order", RCK_DESKTOP_SETTING_TEXT_SUBPIXEL);
	portal_map_add(xs, PORTAL_NS_GNOME, "font-name", RCK_DESKTOP_SETTING_TEXT_FONT);
	portal_map_add(xs, PORTAL_NS_GNOME, "icon-theme", RCK_DESKTOP_SETTING_ICON_THEME);

	portal_map_add(xs, PORTAL_NS_KDE_GENERAL, "ColorScheme", RCK_DESKTOP_SETTING_THEME);
	portal_map_add(xs, PORTAL_NS_KDE_GENERAL, "forceFontDPI", RCK_DESKTOP_SETTING_TEXT_DPI);
	portal_map_add(xs, PORTAL_NS_KDE_GENERAL, "XftAntialias", RCK_DESKTOP_SETTING_TEXT_AA);
	portal_map_add(xs, PORTAL_NS_KDE_GENERAL, "XftHintStyle", RCK_DESKTOP_SETTING_TEXT_HINTING);
	portal_map_add(xs, PORTAL_NS_KDE_GENERAL, "XftHintStyle", RCK_DESKTOP_SETTING_TEXT_HINTING_STYLE);
	portal_map_add(xs, PORTAL_NS_KDE_GENERAL, "XftSubPixel", RCK_DESKTOP_SETTING_TEXT_SUBPIXEL);
	portal_map_add(xs, PORTAL_NS_KDE_GENERAL, "font", RCK_DESKTOP_SETTING_TEXT_FONT);
	portal_map_add(xs, PORTAL_NS_KDE_ICONS, "Theme", RCK_DESKTOP_SETTING_ICON_THEME);
}

static RCKDesktopSettings *portal_backend_new(PRBool probe) {
	RCKDesktopSettingsPortal *xs;
	RCKDesktopSettings *s;
	DBusError error;

	xs = PR_NEW(RCKDesktopSettingsPortal);
	if (!xs) {
		return NULL;
	}

	s = (RCKDesktopSettings *)xs;
	s->backend = RCK_DESKTOP_SETTINGS_BACKEND_PORTAL;
	PR_INIT_CLIST(&s->listeners);
	portal_map_init(xs);
	
	dbus_error_init(&error);
	xs->conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		PR_Free(xs);
		return NULL;
	}

	if (!xs->conn) {
		PR_Free(xs);
		return NULL;
	}

	dbus_connection_set_exit_on_disconnect(xs->conn, FALSE);

	if (probe && !portal_probe(xs->conn)) {
		dbus_connection_unref(xs->conn);
		PR_Free(xs);
		return NULL;
	}

	s->pump = portal_backend_pump;
	s->get_value = portal_backend_get;
	s->destroy = portal_backend_destroy;

	dbus_bus_add_match(xs->conn, PORTAL_MATCH_RULE, NULL);
	dbus_connection_add_filter(xs->conn, &portal_backend_filter, s, NULL);
	dbus_connection_flush(xs->conn);

	portal_backend_pump(s);

	return s;
}

RCKDesktopSettings *rck_desktop_settings_new(void) {
	RCKDesktopSettings *ret;
	const char *env;

	env = PR_GetEnv("RCK_DESKTOP_SETTINGS_BACKEND");
	
	if (env && !PL_strcasecmp(env, "xsettings")) {
		return xsettings_backend_new();
	}

	if (env && !PL_strcasecmp(env, "portal")) {
		return portal_backend_new(PR_FALSE);
	}

	ret = portal_backend_new(PR_TRUE);
	if (!ret) {
		ret = xsettings_backend_new();
	}

	return ret;
}
