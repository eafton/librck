#include <rck.h>

int main(int argc, char **argp) {
	pixman_image_t *xpm;
	RCKIOVariant iov;
	
	xpm = rck_xpm_load(RCK_XPM_INPUT_TYPE_FROM_FILENAME, argp[1], NULL, NULL, NULL, NULL);
	
	iov.filename = argp[2];
	rck_png_save(xpm, RCK_IO_VARIANT_TYPE_FILENAME, &iov);
	
	pixman_image_unref(xpm);
	return 0;
}
