#include <rck.h>

int main(int argc, char **argp) {
	RCKDesktopSettings *settings;
	char *text;
	
	settings = rck_desktop_settings_new();
	rck_desktop_settings_destroy(settings);
	
	return 0;
}
