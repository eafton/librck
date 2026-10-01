#include <stdlib.h>
#include <string.h>
#include <plutosvg.h>
#include "rck.h"

static void destrroy_surface(pixman_image_t *image, void *data) {
	plutovg_surface_destroy((plutovg_surface_t *)data);
}

pixman_image_t *rck_svg_load(RCKIOVariantType mask, RCKIOVariant *input, float width, float height) {
	pixman_image_t *result;
	plutosvg_document_t *doc;
	plutovg_surface_t *surface;
	
	if (!input) {
		return NULL;
	}
	
	doc = NULL;
	if (mask == RCK_IO_VARIANT_TYPE_FILENAME && input->filename) {
		doc = plutosvg_document_load_from_file(input->filename, width, height);
	} else if (mask == RCK_IO_VARIANT_TYPE_DATA && input->data.buf && input->data.sz) {
		doc = plutosvg_document_load_from_data(input->data.buf, input->data.sz, width, height, NULL, NULL);
	}
	
	if (!doc) {
		return NULL;
	}
	
	surface = plutosvg_document_render_to_surface(doc, NULL, width, height, NULL, NULL, NULL);
	if (!surface) {
		plutosvg_document_destroy(doc);
		return NULL;
	}
	
	result = pixman_image_create_bits_no_clear(PIXMAN_a8r8g8b8, plutovg_surface_get_width(surface), plutovg_surface_get_height(surface), (uint32_t*)plutovg_surface_get_data(surface), plutovg_surface_get_stride(surface));    
	pixman_image_set_destroy_function(result, destrroy_surface, surface);
    plutosvg_document_destroy(doc);
	return result;
}
