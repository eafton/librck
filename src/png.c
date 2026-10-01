#include <stdlib.h>
#include <string.h>
#include <spng.h>
#include "rck.h"

static void destroy(pixman_image_t *image, void *data) {
	free(data);
}

pixman_image_t *rck_png_load(RCKIOVariantType mask, RCKIOVariant *input) {
	pixman_image_t *ret;
	void *data;
	spng_ctx *ctx;
	FILE *file;
	struct spng_ihdr hdr;
	size_t sz;
	
	if (!input) {
		return NULL;
	}
	
	ctx = spng_ctx_new(0);
	if (!ctx) {
		return NULL;
	}
	
	switch (mask) {
		case RCK_IO_VARIANT_TYPE_FILENAME:
			file = fopen(input->filename, "rb");
			if (!file) {
				spng_ctx_free(ctx);
				return NULL;
			}
			spng_set_png_file(ctx, file);
			break;
		case RCK_IO_VARIANT_TYPE_DATA:
			if (!input->data.buf || !input->data.sz) {
				spng_ctx_free(ctx);
				return NULL;
			}
			spng_set_png_buffer(ctx, input->data.buf, input->data.sz);
			break;
		default:
			spng_ctx_free(ctx);
			return NULL;
	}
	
	spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &sz);
	data = malloc(sz);
	if (!data) {
		spng_ctx_free(ctx);
		return NULL;
	}
	
	spng_get_ihdr(ctx, &hdr);
	spng_decode_image(ctx, data, sz, SPNG_FMT_RGBA8, 0);
	ret = pixman_image_create_bits_no_clear(PIXMAN_a8b8g8r8, hdr.width, hdr.height, data, hdr.width * 4);
	pixman_image_set_destroy_function(ret, destroy, data);
	spng_ctx_free(ctx);

	return ret;
}

PRBool rck_png_save(pixman_image_t *image, RCKIOVariantType mask, RCKIOVariant *input) {
	pixman_image_t *conv;
	spng_ctx *ctx;
	struct spng_ihdr hdr;
	
	if (!image) {
		return PR_FALSE;
	}
	
	conv = NULL;
	if (pixman_image_get_format(image) != PIXMAN_a8b8g8r8) {
		unsigned int w, h;
		
		w = pixman_image_get_width(image);
		h = pixman_image_get_height(image);
		conv = pixman_image_create_bits(PIXMAN_a8b8g8r8, w, h, NULL, -1);
		pixman_image_composite32(PIXMAN_OP_SRC, image, NULL, conv, 0, 0, 0, 0, 0, 0, w, h);
	}
	
	ctx = spng_ctx_new(SPNG_CTX_ENCODER);	
	if (mask == RCK_IO_VARIANT_TYPE_FILENAME) {
		FILE *file;
		void *data;
		size_t sz;
		
		memset(&hdr, 0, sizeof(struct spng_ihdr));
		hdr.bit_depth = 8;
		hdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
		if (conv) {
			hdr.width = pixman_image_get_width(conv);
			hdr.height = pixman_image_get_height(conv);
			data = pixman_image_get_data(conv);
			sz = pixman_image_get_stride(conv) * hdr.height;
		} else {
			hdr.width = pixman_image_get_width(image);
			hdr.height = pixman_image_get_height(image);
			data = pixman_image_get_data(image);
			sz = pixman_image_get_stride(image) * hdr.height;
		}
		file = fopen(input->filename, "wb");
		spng_set_png_file(ctx, file);
		spng_set_ihdr(ctx, &hdr);
		spng_encode_image(ctx, data, sz, SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE);
		fclose(file);
	} else if (mask == RCK_IO_VARIANT_TYPE_DATA) {
		return PR_FALSE;
	} 
	
	spng_ctx_free(ctx);
	if (conv) {
		pixman_image_unref(conv);
	}
	
	return PR_FALSE;
}
