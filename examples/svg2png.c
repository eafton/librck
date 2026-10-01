#include <rck.h>

int main(int argc, char **argp) {
	pixman_image_t *xpm;
	RCKIOVariant iov;
	
	rck_init();

	iov.filename = argp[1];
	xpm = rck_svg_load(RCK_IO_VARIANT_TYPE_FILENAME, &iov, -1, -1);
	
	iov.filename = argp[2];
	rck_png_save(xpm, RCK_IO_VARIANT_TYPE_FILENAME, &iov);
	
	pixman_image_unref(xpm);
	rck_deinit();
	
	return 0;
}
