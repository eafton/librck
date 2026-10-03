#include <rck.h>

int main(int argc, char **argp) {
	RCKXDGIconThemes *themes;
	RCKXDGIconTheme *theme;
	pixman_image_t *img;
	RCKIOVariant iov;
	
	rck_init();
	themes = rck_xdg_icon_themes_new();
	theme = rck_xdg_icon_themes_get_theme_by_name(themes, "mate");
	
	img = rck_xdg_icon_theme_load_icon(theme, "audio-card", 32, 1, PR_TRUE);
	iov.filename = "test.png";
	rck_png_save(img, RCK_IO_VARIANT_TYPE_FILENAME, &iov);
	pixman_image_unref(img);
	
	rck_xdg_icon_themes_free(themes);
	rck_deinit();
	
	return 0;
}
