#include <stdlib.h>
#include <string.h>
#include <unistr.h>
#include <plhash.h>
#include "rck.h"
#include "rckpriv.h"
#include "font.h"

static PLHashTable *ht = NULL;

static size_t cpcount(const char *str, size_t len) {
    const uint8_t *s;
    size_t bytes_processed, count;
    
    s = (const uint8_t *)str;
	count = 0;
	bytes_processed = 0;

    while (bytes_processed < len) {
        int res;
        
        res = u8_mblen(s + bytes_processed, len - bytes_processed);
        if (res <= 0) {
            break; 
        }
        
        bytes_processed += res;
        count++;
    }

    return count;
}

static void destroy_image(pixman_image_t *image, void *data) {
	free(data);
}

static void add_to_table(PRUint32 cp, char *bitmap) {
	pixman_image_t *img;
	PRUint32 *val;
	PRUint8 *buf;
	int row;
	
	val = PR_NEW(PRUint32);
	buf = malloc(8 * 4);
	
    for (row = 0; row < 8; row++) {
        buf[row * 4] = (PRUint8)bitmap[row];        
    }

    img = pixman_image_create_bits_no_clear(PIXMAN_a1, 8, 8, (PRUint32 *)buf, 4);
	pixman_image_set_destroy_function(img, destroy_image, buf);
	*val = cp;
	
	PL_HashTableAdd(ht, val, img);
}

PLHashNumber PR_CALLBACK hash_cp(const void *key) {
    return (PLHashNumber)(*(PRUint32*)key);
}

static void init_table(void) {
	int i;
	
	if (ht) {
		return;
	}
	
	ht = PL_NewHashTable(FONT_CODEPOINTS, hash_cp, irck_compare_uint32, PL_CompareValues, NULL, NULL);
 
    for (i = 0; i < 128; i++) {
        add_to_table(i, font8x8_basic[i]);
    }
    
    for (i = 0; i < 32; i++) {
        add_to_table(0x2580 + i, font8x8_block[i]);
    }

    for (i = 0; i < 128; i++) {
        add_to_table(0x2500 + i, font8x8_box[i]);
    }

    for (i = 0; i < 32; i++) {
        add_to_table(0x0080 + i, font8x8_control[i]);
    }

    for (i = 0; i < 96; i++) {
        add_to_table(0x00A0 + i, font8x8_ext_latin[i]);
    }

    for (i = 0; i < 58; i++) {
        add_to_table(0x0390 + i, font8x8_greek[i]);
    }

    for (i = 0; i < 96; i++) {
        add_to_table(0x3040 + i, font8x8_hiragana[i]);
    }

    for (i = 0; i < 26; i++) {
        add_to_table(0xE541 + i, font8x8_sga[i]);
    }
    
	add_to_table(0x20A7, font8x8_misc[0]);
	add_to_table(0x0192, font8x8_misc[1]);
	add_to_table(0x2310, font8x8_misc[4]);
	add_to_table(0x2266, font8x8_misc[5]);
	add_to_table(0x2265, font8x8_misc[6]);
	add_to_table(0x0300, font8x8_misc[7]);
	add_to_table(0x1EF2, font8x8_misc[8]);
	add_to_table(0x1EF3, font8x8_misc[9]);
}

int rck_fallback_text_line_measure(char *string, size_t len, int pel_size, int *outH) {
	if (!string) {
		return -1;
	}
	
	if (pel_size < 8) {
		pel_size = 8;
	}
	
	if (outH) {
		*outH = pel_size;
	}
	
	return cpcount(string, len) * pel_size;
}

void rck_fallback_text_line_draw(pixman_image_t *image, pixman_image_t *source, char *string, size_t len, int pel_size, int x, int y) {
	pixman_image_t *pix;
	const uint8_t *s;
    const uint8_t *end;
    int ix;
	pixman_transform_t transform;
	
	if (!image || !source || !string) {
		return;
	}
	
	if (pel_size < 8) {
		pel_size = 8;
	}
	
	if (pel_size != 8) {
		pixman_transform_init_scale(&transform, pixman_double_to_fixed((8.0 / (double)pel_size)), pixman_double_to_fixed((8.0 / (double)pel_size)));
	}
	
	ix = x;
	s = (const uint8_t *)string;
	end = s + len;
	
	init_table();
	
    while (s < end) {
        ucs4_t uc;
		int bc;
		
        bc = u8_mbtouc(&uc, s, end - s);
        if (bc <= 0) {
            break; 
        }
        
        pix = PL_HashTableLookup(ht, &uc);
        if (!pix) {
			uc = '?';
			pix = PL_HashTableLookup(ht, &uc);
		}
		
		if (pel_size != 8) {
			pixman_image_set_transform(pix, &transform);
			pixman_image_set_filter(pix, PIXMAN_FILTER_NEAREST, NULL, 0);
		}

		pixman_image_composite32(PIXMAN_OP_OVER, source, pix, image, 0, 0, 0, 0, ix, y, pel_size, pel_size);

		if (pel_size != 8) {
			pixman_image_set_transform(pix, NULL);
		}
		
        s += bc; 
        ix += pel_size;
    }	
}

static PRIntn PR_CALLBACK cleanup_table(PLHashEntry *he, PRIntn index, void *arg) {
	PR_Free((void*)he->key);
	pixman_image_unref((pixman_image_t*)he->value);
	return HT_ENUMERATE_NEXT;
}

void irck_fallback_text_deinit() {
	if (ht) {
		PL_HashTableEnumerateEntries(ht, cleanup_table, NULL);
		PL_HashTableDestroy(ht);
	}
}

RCKFallbackGlyph *rck_fallback_text_line_rasterize(char *string, size_t len, int pel_size) {
	RCKFallbackGlyph *glyphs;
	const uint8_t *s;
    const uint8_t *end;
	pixman_transform_t transform;
	size_t sz, i;
	
	if (!string) {
		return NULL;
	}
	
	if (pel_size < 8) {
		pel_size = 8;
	}
	
	if (pel_size != 8) {
		pixman_transform_init_scale(&transform, pixman_double_to_fixed((8.0 / (double)pel_size)), pixman_double_to_fixed((8.0 / (double)pel_size)));
	}
	
	i = 0;
	sz = cpcount(string, len);
	s = (const uint8_t *)string;
	end = s + len;
	glyphs = calloc(sz, sizeof(RCKFallbackGlyph));
	
	init_table();
	
    while (s < end) {
		pixman_image_t *pix;
		ucs4_t uc;
		int bc;
		
        bc = u8_mbtouc(&uc, s, end - s);
        if (bc <= 0) {
            break; 
        }
        
        pix = PL_HashTableLookup(ht, &uc);
        if (!pix) {
			uc = '?';
			pix = PL_HashTableLookup(ht, &uc);
		}
		
		if (pel_size != 8) {
			pixman_image_set_transform(pix, &transform);
			pixman_image_set_filter(pix, PIXMAN_FILTER_NEAREST, NULL, 0);
		}

		glyphs[i].pix = pixman_image_create_bits(PIXMAN_a1, pel_size, pel_size, NULL, -1);
		pixman_image_composite32(PIXMAN_OP_SRC, pix, NULL, glyphs[i].pix, 0, 0, 0, 0, 0, 0, pel_size, pel_size);

		glyphs[i].sz = sz;
		glyphs[i].w = pel_size;
		glyphs[i].h = pel_size;
		glyphs[i].xadvance = pel_size;
		glyphs[i].yadvance = 0;

		if (pel_size != 8) {
			pixman_image_set_transform(pix, NULL);
		}
		
        s += bc; 
        i++;
    }
    
    return glyphs;
}

void rck_fallback_text_free_glyphs(RCKFallbackGlyph *glyphs) {
	size_t i;
	
	for (i = 0; i < glyphs->sz; i++) {
		pixman_image_unref(glyphs[i].pix);
	}
	free(glyphs);
}
