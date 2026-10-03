#include <string.h>
#include <rck.h>

int main(int argc, char **argp) {
	pixman_image_t *img;
	pixman_image_t *fill;
	pixman_color_t color;
	pixman_rectangle16_t rct;
	RCKIOVariant iov;
	int w, h;
	#define TEXT "Lorem ipsum dolor sit amet."
	
	rck_init();
	
	w = rck_fallback_text_line_measure(TEXT, strlen(TEXT), 12, &h);
    img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h, NULL, -1);

    color.red = color.green = color.blue = color.alpha = 0xffff;
    fill = pixman_image_create_solid_fill(&color);

	rct.x = rct.y = 0;
	rct.width = w;
	rct.height = h;
    color.red = color.green = 0;
	pixman_image_fill_rectangles(PIXMAN_OP_OVER, img, &color, 1, &rct);
    
    rck_fallback_text_line_draw(img, fill, TEXT, strlen(TEXT), 12, 0, 0);
    
	iov.filename = "test.png";
	rck_png_save(img, RCK_IO_VARIANT_TYPE_FILENAME, &iov);
	pixman_image_unref(img);
	
	rck_deinit();
	
	return 0;
}
