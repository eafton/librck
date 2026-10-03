#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <regex.h>
#include <nspr.h>
#include <plstr.h>
#include <plhash.h>
#include "rck.h"
#include "rckpriv.h"
#define compare_uint32 irck_compare_uint32

/* XPM1/XPM2/XPM3 parser for the XDG icon theme implementation */
/* It is not meant to be fast, its just meant to parse most "modern" XPM files that some XDG icon themes may include or applications might put in the hicolor theme. */

typedef enum {
	IRCK_XPM_STAGE_ERROR = -2,
	IRCK_XPM_STAGE_DONE,
	IRCK_XPM_STAGE_NONE = 0,
	IRCK_XPM_STAGE_XPM2_INTEGER_BLOCK,
	IRCK_XPM_STAGE_XPM2_COLOR_DEFINES,
	IRCK_XPM_STAGE_XPM2_PIXEL_DATA,
	IRCK_XPM_STAGE_XPM1_IN_PROGESS
} IRCKXPMStage;

#define	RCK_XPM_INFO_MASK_DIMENSIONS 1 << 30
#define IRCK_XPM_COLOR_INFO_NONE 0
#define IRCK_XPM_COLOR_INFO_TRANSPARENT 1
#define IRCK_XPM_COLOR_INFO_RGB 2
#define IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY 3
#define IRCK_XPM_COLOR_INFO_RGB_USE_LIBC_FREE 4
#define IRCK_XPM_PACK_RGB(i, r, g, b) ((PRUint8)i << 24) | ((PRUint8)r << 16) | ((PRUint8)g << 8) | ((PRUint8)b);
#define IRCK_SKIP_WHITESPACE(k) while (k[0] == ' ' || k[0] == '\t' || k[0] == '\n' ||  k[0] == '\r' || k[0] == '\v' || k[0] == '\f') {k++;}		

typedef struct {
	RCKXPMInfoMask infomask;
	unsigned int version;
	unsigned int w;
	unsigned int h;
	unsigned int colorcount;
	unsigned int chrperpel;
	unsigned int hotx;
	unsigned int hoty;
	
	IRCKXPMStage stage;
	int iparam;
	char cparam;
	char *sparam;
	
	PLHashTable *color_table;
	char *color_table_key;
	PRUint32 *argb8888;
	pixman_image_t *image;
} RCKXPMImage;

PLHashTable *x11_color_table;

PRIntn PR_CALLBACK irck_compare_uint32(const void *v1, const void *v2) {
	if ((*(PRUint32*)v1) == (*(PRUint32*)v2)) {
		return 1;
	}
	
	return 0;
}

static PRIntn PR_CALLBACK cleanup_color_table(PLHashEntry *he, PRIntn index, void *arg) {
	PRUint8 info;
	
	info = (((*((PRUint32*)he->value)) >> 24) & 0xFF);
	if (info == IRCK_XPM_COLOR_INFO_RGB) {
		PL_strfree((char*)he->key);
	} else if (info == IRCK_XPM_COLOR_INFO_RGB_USE_LIBC_FREE) {
		free((void*)he->key);
	}

	PR_Free(he->value);
	return HT_ENUMERATE_NEXT;
}

void irck_xpm_init() {
	x11_color_table = NULL;
}

static void init_color_table(void) {
	PRUint32 *val;
		
	x11_color_table = PL_NewHashTable(753, PL_HashString, PL_CompareStrings, compare_uint32, NULL, NULL);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 250, 250); PL_HashTableAdd(x11_color_table, "snow", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 248, 248, 255); PL_HashTableAdd(x11_color_table, "ghost white", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 248, 248, 255); PL_HashTableAdd(x11_color_table, "GhostWhite", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 245, 245, 245); PL_HashTableAdd(x11_color_table, "white smoke", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 245, 245, 245); PL_HashTableAdd(x11_color_table, "WhiteSmoke", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 220, 220, 220); PL_HashTableAdd(x11_color_table, "gainsboro", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 250, 240); PL_HashTableAdd(x11_color_table, "floral white", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 250, 240); PL_HashTableAdd(x11_color_table, "FloralWhite", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 253, 245, 230); PL_HashTableAdd(x11_color_table, "old lace", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 253, 245, 230); PL_HashTableAdd(x11_color_table, "OldLace", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 250, 240, 230); PL_HashTableAdd(x11_color_table, "linen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 250, 235, 215); PL_HashTableAdd(x11_color_table, "antique white", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 250, 235, 215); PL_HashTableAdd(x11_color_table, "AntiqueWhite", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 239, 213); PL_HashTableAdd(x11_color_table, "papaya whip", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 239, 213); PL_HashTableAdd(x11_color_table, "PapayaWhip", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 235, 205); PL_HashTableAdd(x11_color_table, "blanched almond", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 235, 205); PL_HashTableAdd(x11_color_table, "BlanchedAlmond", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 228, 196); PL_HashTableAdd(x11_color_table, "bisque", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 218, 185); PL_HashTableAdd(x11_color_table, "peach puff", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 218, 185); PL_HashTableAdd(x11_color_table, "PeachPuff", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 222, 173); PL_HashTableAdd(x11_color_table, "navajo white", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 222, 173); PL_HashTableAdd(x11_color_table, "NavajoWhite", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 228, 181); PL_HashTableAdd(x11_color_table, "moccasin", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 248, 220); PL_HashTableAdd(x11_color_table, "cornsilk", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 255, 240); PL_HashTableAdd(x11_color_table, "ivory", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 250, 205); PL_HashTableAdd(x11_color_table, "lemon chiffon", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 250, 205); PL_HashTableAdd(x11_color_table, "LemonChiffon", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 245, 238); PL_HashTableAdd(x11_color_table, "seashell", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 255, 240); PL_HashTableAdd(x11_color_table, "honeydew", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 245, 255, 250); PL_HashTableAdd(x11_color_table, "mint cream", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 245, 255, 250); PL_HashTableAdd(x11_color_table, "MintCream", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 255, 255); PL_HashTableAdd(x11_color_table, "azure", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 248, 255); PL_HashTableAdd(x11_color_table, "alice blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 248, 255); PL_HashTableAdd(x11_color_table, "AliceBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 230, 230, 250); PL_HashTableAdd(x11_color_table, "lavender", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 240, 245); PL_HashTableAdd(x11_color_table, "lavender blush", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 240, 245); PL_HashTableAdd(x11_color_table, "LavenderBlush", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 228, 225); PL_HashTableAdd(x11_color_table, "misty rose", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 228, 225); PL_HashTableAdd(x11_color_table, "MistyRose", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 255, 255); PL_HashTableAdd(x11_color_table, "white", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 0); PL_HashTableAdd(x11_color_table, "black", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 47, 79, 79); PL_HashTableAdd(x11_color_table, "dark slate gray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 47, 79, 79); PL_HashTableAdd(x11_color_table, "DarkSlateGray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 47, 79, 79); PL_HashTableAdd(x11_color_table, "dark slate grey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 47, 79, 79); PL_HashTableAdd(x11_color_table, "DarkSlateGrey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 105, 105, 105); PL_HashTableAdd(x11_color_table, "dim gray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 105, 105, 105); PL_HashTableAdd(x11_color_table, "DimGray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 105, 105, 105); PL_HashTableAdd(x11_color_table, "dim grey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 105, 105, 105); PL_HashTableAdd(x11_color_table, "DimGrey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 112, 128, 144); PL_HashTableAdd(x11_color_table, "slate gray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 112, 128, 144); PL_HashTableAdd(x11_color_table, "SlateGray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 112, 128, 144); PL_HashTableAdd(x11_color_table, "slate grey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 112, 128, 144); PL_HashTableAdd(x11_color_table, "SlateGrey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 119, 136, 153); PL_HashTableAdd(x11_color_table, "light slate gray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 119, 136, 153); PL_HashTableAdd(x11_color_table, "LightSlateGray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 119, 136, 153); PL_HashTableAdd(x11_color_table, "light slate grey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 119, 136, 153); PL_HashTableAdd(x11_color_table, "LightSlateGrey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 190, 190, 190); PL_HashTableAdd(x11_color_table, "gray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 190, 190, 190); PL_HashTableAdd(x11_color_table, "grey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 211, 211, 211); PL_HashTableAdd(x11_color_table, "light grey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 211, 211, 211); PL_HashTableAdd(x11_color_table, "LightGrey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 211, 211, 211); PL_HashTableAdd(x11_color_table, "light gray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 211, 211, 211); PL_HashTableAdd(x11_color_table, "LightGray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 25, 25, 112); PL_HashTableAdd(x11_color_table, "midnight blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 25, 25, 112); PL_HashTableAdd(x11_color_table, "MidnightBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 128); PL_HashTableAdd(x11_color_table, "navy", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 128); PL_HashTableAdd(x11_color_table, "navy blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 128); PL_HashTableAdd(x11_color_table, "NavyBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 100, 149, 237); PL_HashTableAdd(x11_color_table, "cornflower blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 100, 149, 237); PL_HashTableAdd(x11_color_table, "CornflowerBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 72, 61, 139); PL_HashTableAdd(x11_color_table, "dark slate blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 72, 61, 139); PL_HashTableAdd(x11_color_table, "DarkSlateBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 106, 90, 205); PL_HashTableAdd(x11_color_table, "slate blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 106, 90, 205); PL_HashTableAdd(x11_color_table, "SlateBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 123, 104, 238); PL_HashTableAdd(x11_color_table, "medium slate blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 123, 104, 238); PL_HashTableAdd(x11_color_table, "MediumSlateBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 132, 112, 255); PL_HashTableAdd(x11_color_table, "light slate blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 132, 112, 255); PL_HashTableAdd(x11_color_table, "LightSlateBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 205); PL_HashTableAdd(x11_color_table, "medium blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 205); PL_HashTableAdd(x11_color_table, "MediumBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 65, 105, 225); PL_HashTableAdd(x11_color_table, "royal blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 65, 105, 225); PL_HashTableAdd(x11_color_table, "RoyalBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 255); PL_HashTableAdd(x11_color_table, "blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 30, 144, 255); PL_HashTableAdd(x11_color_table, "dodger blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 30, 144, 255); PL_HashTableAdd(x11_color_table, "DodgerBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 191, 255); PL_HashTableAdd(x11_color_table, "deep sky blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 191, 255); PL_HashTableAdd(x11_color_table, "DeepSkyBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 135, 206, 235); PL_HashTableAdd(x11_color_table, "sky blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 135, 206, 235); PL_HashTableAdd(x11_color_table, "SkyBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 135, 206, 250); PL_HashTableAdd(x11_color_table, "light sky blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 135, 206, 250); PL_HashTableAdd(x11_color_table, "LightSkyBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 70, 130, 180); PL_HashTableAdd(x11_color_table, "steel blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 70, 130, 180); PL_HashTableAdd(x11_color_table, "SteelBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 176, 196, 222); PL_HashTableAdd(x11_color_table, "light steel blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 176, 196, 222); PL_HashTableAdd(x11_color_table, "LightSteelBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 173, 216, 230); PL_HashTableAdd(x11_color_table, "light blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 173, 216, 230); PL_HashTableAdd(x11_color_table, "LightBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 176, 224, 230); PL_HashTableAdd(x11_color_table, "powder blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 176, 224, 230); PL_HashTableAdd(x11_color_table, "PowderBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 175, 238, 238); PL_HashTableAdd(x11_color_table, "pale turquoise", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 175, 238, 238); PL_HashTableAdd(x11_color_table, "PaleTurquoise", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 206, 209); PL_HashTableAdd(x11_color_table, "dark turquoise", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 206, 209); PL_HashTableAdd(x11_color_table, "DarkTurquoise", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 72, 209, 204); PL_HashTableAdd(x11_color_table, "medium turquoise", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 72, 209, 204); PL_HashTableAdd(x11_color_table, "MediumTurquoise", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 64, 224, 208); PL_HashTableAdd(x11_color_table, "turquoise", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 255, 255); PL_HashTableAdd(x11_color_table, "cyan", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 224, 255, 255); PL_HashTableAdd(x11_color_table, "light cyan", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 224, 255, 255); PL_HashTableAdd(x11_color_table, "LightCyan", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 95, 158, 160); PL_HashTableAdd(x11_color_table, "cadet blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 95, 158, 160); PL_HashTableAdd(x11_color_table, "CadetBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 102, 205, 170); PL_HashTableAdd(x11_color_table, "medium aquamarine", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 102, 205, 170); PL_HashTableAdd(x11_color_table, "MediumAquamarine", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 127, 255, 212); PL_HashTableAdd(x11_color_table, "aquamarine", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 100, 0); PL_HashTableAdd(x11_color_table, "dark green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 100, 0); PL_HashTableAdd(x11_color_table, "DarkGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 85, 107, 47); PL_HashTableAdd(x11_color_table, "dark olive green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 85, 107, 47); PL_HashTableAdd(x11_color_table, "DarkOliveGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 143, 188, 143); PL_HashTableAdd(x11_color_table, "dark sea green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 143, 188, 143); PL_HashTableAdd(x11_color_table, "DarkSeaGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 46, 139, 87); PL_HashTableAdd(x11_color_table, "sea green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 46, 139, 87); PL_HashTableAdd(x11_color_table, "SeaGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 60, 179, 113); PL_HashTableAdd(x11_color_table, "medium sea green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 60, 179, 113); PL_HashTableAdd(x11_color_table, "MediumSeaGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 32, 178, 170); PL_HashTableAdd(x11_color_table, "light sea green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 32, 178, 170); PL_HashTableAdd(x11_color_table, "LightSeaGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 152, 251, 152); PL_HashTableAdd(x11_color_table, "pale green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 152, 251, 152); PL_HashTableAdd(x11_color_table, "PaleGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 255, 127); PL_HashTableAdd(x11_color_table, "spring green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 255, 127); PL_HashTableAdd(x11_color_table, "SpringGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 124, 252, 0); PL_HashTableAdd(x11_color_table, "lawn green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 124, 252, 0); PL_HashTableAdd(x11_color_table, "LawnGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 255, 0); PL_HashTableAdd(x11_color_table, "green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 127, 255, 0); PL_HashTableAdd(x11_color_table, "chartreuse", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 250, 154); PL_HashTableAdd(x11_color_table, "medium spring green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 250, 154); PL_HashTableAdd(x11_color_table, "MediumSpringGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 173, 255, 47); PL_HashTableAdd(x11_color_table, "green yellow", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 173, 255, 47); PL_HashTableAdd(x11_color_table, "GreenYellow", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 50, 205, 50); PL_HashTableAdd(x11_color_table, "lime green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 50, 205, 50); PL_HashTableAdd(x11_color_table, "LimeGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 154, 205, 50); PL_HashTableAdd(x11_color_table, "yellow green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 154, 205, 50); PL_HashTableAdd(x11_color_table, "YellowGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 34, 139, 34); PL_HashTableAdd(x11_color_table, "forest green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 34, 139, 34); PL_HashTableAdd(x11_color_table, "ForestGreen", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 107, 142, 35); PL_HashTableAdd(x11_color_table, "olive drab", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 107, 142, 35); PL_HashTableAdd(x11_color_table, "OliveDrab", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 189, 183, 107); PL_HashTableAdd(x11_color_table, "dark khaki", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 189, 183, 107); PL_HashTableAdd(x11_color_table, "DarkKhaki", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 230, 140); PL_HashTableAdd(x11_color_table, "khaki", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 232, 170); PL_HashTableAdd(x11_color_table, "pale goldenrod", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 232, 170); PL_HashTableAdd(x11_color_table, "PaleGoldenrod", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 250, 250, 210); PL_HashTableAdd(x11_color_table, "light goldenrod yellow", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 250, 250, 210); PL_HashTableAdd(x11_color_table, "LightGoldenrodYellow", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 255, 224); PL_HashTableAdd(x11_color_table, "light yellow", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 255, 224); PL_HashTableAdd(x11_color_table, "LightYellow", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 255, 0); PL_HashTableAdd(x11_color_table, "yellow", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 215, 0); PL_HashTableAdd(x11_color_table, "gold", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 221, 130); PL_HashTableAdd(x11_color_table, "light goldenrod", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 221, 130); PL_HashTableAdd(x11_color_table, "LightGoldenrod", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 218, 165, 32); PL_HashTableAdd(x11_color_table, "goldenrod", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 184, 134, 11); PL_HashTableAdd(x11_color_table, "dark goldenrod", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 184, 134, 11); PL_HashTableAdd(x11_color_table, "DarkGoldenrod", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 188, 143, 143); PL_HashTableAdd(x11_color_table, "rosy brown", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 188, 143, 143); PL_HashTableAdd(x11_color_table, "RosyBrown", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 92, 92); PL_HashTableAdd(x11_color_table, "indian red", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 92, 92); PL_HashTableAdd(x11_color_table, "IndianRed", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 69, 19); PL_HashTableAdd(x11_color_table, "saddle brown", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 69, 19); PL_HashTableAdd(x11_color_table, "SaddleBrown", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 160, 82, 45); PL_HashTableAdd(x11_color_table, "sienna", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 133, 63); PL_HashTableAdd(x11_color_table, "peru", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 222, 184, 135); PL_HashTableAdd(x11_color_table, "burlywood", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 245, 245, 220); PL_HashTableAdd(x11_color_table, "beige", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 245, 222, 179); PL_HashTableAdd(x11_color_table, "wheat", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 244, 164, 96); PL_HashTableAdd(x11_color_table, "sandy brown", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 244, 164, 96); PL_HashTableAdd(x11_color_table, "SandyBrown", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 210, 180, 140); PL_HashTableAdd(x11_color_table, "tan", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 210, 105, 30); PL_HashTableAdd(x11_color_table, "chocolate", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 178, 34, 34); PL_HashTableAdd(x11_color_table, "firebrick", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 165, 42, 42); PL_HashTableAdd(x11_color_table, "brown", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 233, 150, 122); PL_HashTableAdd(x11_color_table, "dark salmon", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 233, 150, 122); PL_HashTableAdd(x11_color_table, "DarkSalmon", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 250, 128, 114); PL_HashTableAdd(x11_color_table, "salmon", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 160, 122); PL_HashTableAdd(x11_color_table, "light salmon", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 160, 122); PL_HashTableAdd(x11_color_table, "LightSalmon", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 165, 0); PL_HashTableAdd(x11_color_table, "orange", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 140, 0); PL_HashTableAdd(x11_color_table, "dark orange", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 140, 0); PL_HashTableAdd(x11_color_table, "DarkOrange", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 127, 80); PL_HashTableAdd(x11_color_table, "coral", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 128, 128); PL_HashTableAdd(x11_color_table, "light coral", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 128, 128); PL_HashTableAdd(x11_color_table, "LightCoral", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 99, 71); PL_HashTableAdd(x11_color_table, "tomato", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 69, 0); PL_HashTableAdd(x11_color_table, "orange red", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 69, 0); PL_HashTableAdd(x11_color_table, "OrangeRed", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 0, 0); PL_HashTableAdd(x11_color_table, "red", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 105, 180); PL_HashTableAdd(x11_color_table, "hot pink", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 105, 180); PL_HashTableAdd(x11_color_table, "HotPink", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 20, 147); PL_HashTableAdd(x11_color_table, "deep pink", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 20, 147); PL_HashTableAdd(x11_color_table, "DeepPink", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 192, 203); PL_HashTableAdd(x11_color_table, "pink", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 182, 193); PL_HashTableAdd(x11_color_table, "light pink", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 182, 193); PL_HashTableAdd(x11_color_table, "LightPink", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 219, 112, 147); PL_HashTableAdd(x11_color_table, "pale violet red", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 219, 112, 147); PL_HashTableAdd(x11_color_table, "PaleVioletRed", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 176, 48, 96); PL_HashTableAdd(x11_color_table, "maroon", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 199, 21, 133); PL_HashTableAdd(x11_color_table, "medium violet red", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 199, 21, 133); PL_HashTableAdd(x11_color_table, "MediumVioletRed", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 208, 32, 144); PL_HashTableAdd(x11_color_table, "violet red", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 208, 32, 144); PL_HashTableAdd(x11_color_table, "VioletRed", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 0, 255); PL_HashTableAdd(x11_color_table, "magenta", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 130, 238); PL_HashTableAdd(x11_color_table, "violet", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 221, 160, 221); PL_HashTableAdd(x11_color_table, "plum", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 218, 112, 214); PL_HashTableAdd(x11_color_table, "orchid", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 186, 85, 211); PL_HashTableAdd(x11_color_table, "medium orchid", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 186, 85, 211); PL_HashTableAdd(x11_color_table, "MediumOrchid", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 153, 50, 204); PL_HashTableAdd(x11_color_table, "dark orchid", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 153, 50, 204); PL_HashTableAdd(x11_color_table, "DarkOrchid", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 148, 0, 211); PL_HashTableAdd(x11_color_table, "dark violet", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 148, 0, 211); PL_HashTableAdd(x11_color_table, "DarkViolet", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 138, 43, 226); PL_HashTableAdd(x11_color_table, "blue violet", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 138, 43, 226); PL_HashTableAdd(x11_color_table, "BlueViolet", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 160, 32, 240); PL_HashTableAdd(x11_color_table, "purple", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 147, 112, 219); PL_HashTableAdd(x11_color_table, "medium purple", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 147, 112, 219); PL_HashTableAdd(x11_color_table, "MediumPurple", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 216, 191, 216); PL_HashTableAdd(x11_color_table, "thistle", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 250, 250); PL_HashTableAdd(x11_color_table, "snow1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 233, 233); PL_HashTableAdd(x11_color_table, "snow2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 201, 201); PL_HashTableAdd(x11_color_table, "snow3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 137, 137); PL_HashTableAdd(x11_color_table, "snow4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 245, 238); PL_HashTableAdd(x11_color_table, "seashell1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 229, 222); PL_HashTableAdd(x11_color_table, "seashell2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 197, 191); PL_HashTableAdd(x11_color_table, "seashell3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 134, 130); PL_HashTableAdd(x11_color_table, "seashell4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 239, 219); PL_HashTableAdd(x11_color_table, "AntiqueWhite1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 223, 204); PL_HashTableAdd(x11_color_table, "AntiqueWhite2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 192, 176); PL_HashTableAdd(x11_color_table, "AntiqueWhite3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 131, 120); PL_HashTableAdd(x11_color_table, "AntiqueWhite4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 228, 196); PL_HashTableAdd(x11_color_table, "bisque1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 213, 183); PL_HashTableAdd(x11_color_table, "bisque2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 183, 158); PL_HashTableAdd(x11_color_table, "bisque3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 125, 107); PL_HashTableAdd(x11_color_table, "bisque4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 218, 185); PL_HashTableAdd(x11_color_table, "PeachPuff1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 203, 173); PL_HashTableAdd(x11_color_table, "PeachPuff2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 175, 149); PL_HashTableAdd(x11_color_table, "PeachPuff3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 119, 101); PL_HashTableAdd(x11_color_table, "PeachPuff4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 222, 173); PL_HashTableAdd(x11_color_table, "NavajoWhite1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 207, 161); PL_HashTableAdd(x11_color_table, "NavajoWhite2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 179, 139); PL_HashTableAdd(x11_color_table, "NavajoWhite3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 121, 94); PL_HashTableAdd(x11_color_table, "NavajoWhite4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 250, 205); PL_HashTableAdd(x11_color_table, "LemonChiffon1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 233, 191); PL_HashTableAdd(x11_color_table, "LemonChiffon2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 201, 165); PL_HashTableAdd(x11_color_table, "LemonChiffon3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 137, 112); PL_HashTableAdd(x11_color_table, "LemonChiffon4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 248, 220); PL_HashTableAdd(x11_color_table, "cornsilk1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 232, 205); PL_HashTableAdd(x11_color_table, "cornsilk2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 200, 177); PL_HashTableAdd(x11_color_table, "cornsilk3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 136, 120); PL_HashTableAdd(x11_color_table, "cornsilk4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 255, 240); PL_HashTableAdd(x11_color_table, "ivory1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 238, 224); PL_HashTableAdd(x11_color_table, "ivory2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 205, 193); PL_HashTableAdd(x11_color_table, "ivory3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 139, 131); PL_HashTableAdd(x11_color_table, "ivory4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 255, 240); PL_HashTableAdd(x11_color_table, "honeydew1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 224, 238, 224); PL_HashTableAdd(x11_color_table, "honeydew2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 193, 205, 193); PL_HashTableAdd(x11_color_table, "honeydew3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 131, 139, 131); PL_HashTableAdd(x11_color_table, "honeydew4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 240, 245); PL_HashTableAdd(x11_color_table, "LavenderBlush1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 224, 229); PL_HashTableAdd(x11_color_table, "LavenderBlush2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 193, 197); PL_HashTableAdd(x11_color_table, "LavenderBlush3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 131, 134); PL_HashTableAdd(x11_color_table, "LavenderBlush4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 228, 225); PL_HashTableAdd(x11_color_table, "MistyRose1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 213, 210); PL_HashTableAdd(x11_color_table, "MistyRose2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 183, 181); PL_HashTableAdd(x11_color_table, "MistyRose3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 125, 123); PL_HashTableAdd(x11_color_table, "MistyRose4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 255, 255); PL_HashTableAdd(x11_color_table, "azure1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 224, 238, 238); PL_HashTableAdd(x11_color_table, "azure2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 193, 205, 205); PL_HashTableAdd(x11_color_table, "azure3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 131, 139, 139); PL_HashTableAdd(x11_color_table, "azure4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 131, 111, 255); PL_HashTableAdd(x11_color_table, "SlateBlue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 122, 103, 238); PL_HashTableAdd(x11_color_table, "SlateBlue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 105, 89, 205); PL_HashTableAdd(x11_color_table, "SlateBlue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 71, 60, 139); PL_HashTableAdd(x11_color_table, "SlateBlue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 72, 118, 255); PL_HashTableAdd(x11_color_table, "RoyalBlue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 67, 110, 238); PL_HashTableAdd(x11_color_table, "RoyalBlue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 58, 95, 205); PL_HashTableAdd(x11_color_table, "RoyalBlue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 39, 64, 139); PL_HashTableAdd(x11_color_table, "RoyalBlue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 255); PL_HashTableAdd(x11_color_table, "blue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 238); PL_HashTableAdd(x11_color_table, "blue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 205); PL_HashTableAdd(x11_color_table, "blue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 139); PL_HashTableAdd(x11_color_table, "blue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 30, 144, 255); PL_HashTableAdd(x11_color_table, "DodgerBlue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 28, 134, 238); PL_HashTableAdd(x11_color_table, "DodgerBlue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 24, 116, 205); PL_HashTableAdd(x11_color_table, "DodgerBlue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 16, 78, 139); PL_HashTableAdd(x11_color_table, "DodgerBlue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 99, 184, 255); PL_HashTableAdd(x11_color_table, "SteelBlue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 92, 172, 238); PL_HashTableAdd(x11_color_table, "SteelBlue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 79, 148, 205); PL_HashTableAdd(x11_color_table, "SteelBlue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 54, 100, 139); PL_HashTableAdd(x11_color_table, "SteelBlue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 191, 255); PL_HashTableAdd(x11_color_table, "DeepSkyBlue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 178, 238); PL_HashTableAdd(x11_color_table, "DeepSkyBlue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 154, 205); PL_HashTableAdd(x11_color_table, "DeepSkyBlue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 104, 139); PL_HashTableAdd(x11_color_table, "DeepSkyBlue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 135, 206, 255); PL_HashTableAdd(x11_color_table, "SkyBlue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 126, 192, 238); PL_HashTableAdd(x11_color_table, "SkyBlue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 108, 166, 205); PL_HashTableAdd(x11_color_table, "SkyBlue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 74, 112, 139); PL_HashTableAdd(x11_color_table, "SkyBlue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 176, 226, 255); PL_HashTableAdd(x11_color_table, "LightSkyBlue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 164, 211, 238); PL_HashTableAdd(x11_color_table, "LightSkyBlue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 141, 182, 205); PL_HashTableAdd(x11_color_table, "LightSkyBlue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 96, 123, 139); PL_HashTableAdd(x11_color_table, "LightSkyBlue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 198, 226, 255); PL_HashTableAdd(x11_color_table, "SlateGray1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 185, 211, 238); PL_HashTableAdd(x11_color_table, "SlateGray2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 159, 182, 205); PL_HashTableAdd(x11_color_table, "SlateGray3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 108, 123, 139); PL_HashTableAdd(x11_color_table, "SlateGray4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 202, 225, 255); PL_HashTableAdd(x11_color_table, "LightSteelBlue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 188, 210, 238); PL_HashTableAdd(x11_color_table, "LightSteelBlue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 162, 181, 205); PL_HashTableAdd(x11_color_table, "LightSteelBlue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 110, 123, 139); PL_HashTableAdd(x11_color_table, "LightSteelBlue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 191, 239, 255); PL_HashTableAdd(x11_color_table, "LightBlue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 178, 223, 238); PL_HashTableAdd(x11_color_table, "LightBlue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 154, 192, 205); PL_HashTableAdd(x11_color_table, "LightBlue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 104, 131, 139); PL_HashTableAdd(x11_color_table, "LightBlue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 224, 255, 255); PL_HashTableAdd(x11_color_table, "LightCyan1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 209, 238, 238); PL_HashTableAdd(x11_color_table, "LightCyan2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 180, 205, 205); PL_HashTableAdd(x11_color_table, "LightCyan3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 122, 139, 139); PL_HashTableAdd(x11_color_table, "LightCyan4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 187, 255, 255); PL_HashTableAdd(x11_color_table, "PaleTurquoise1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 174, 238, 238); PL_HashTableAdd(x11_color_table, "PaleTurquoise2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 150, 205, 205); PL_HashTableAdd(x11_color_table, "PaleTurquoise3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 102, 139, 139); PL_HashTableAdd(x11_color_table, "PaleTurquoise4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 152, 245, 255); PL_HashTableAdd(x11_color_table, "CadetBlue1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 142, 229, 238); PL_HashTableAdd(x11_color_table, "CadetBlue2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 122, 197, 205); PL_HashTableAdd(x11_color_table, "CadetBlue3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 83, 134, 139); PL_HashTableAdd(x11_color_table, "CadetBlue4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 245, 255); PL_HashTableAdd(x11_color_table, "turquoise1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 229, 238); PL_HashTableAdd(x11_color_table, "turquoise2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 197, 205); PL_HashTableAdd(x11_color_table, "turquoise3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 134, 139); PL_HashTableAdd(x11_color_table, "turquoise4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 255, 255); PL_HashTableAdd(x11_color_table, "cyan1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 238, 238); PL_HashTableAdd(x11_color_table, "cyan2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 205, 205); PL_HashTableAdd(x11_color_table, "cyan3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 139, 139); PL_HashTableAdd(x11_color_table, "cyan4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 151, 255, 255); PL_HashTableAdd(x11_color_table, "DarkSlateGray1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 141, 238, 238); PL_HashTableAdd(x11_color_table, "DarkSlateGray2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 121, 205, 205); PL_HashTableAdd(x11_color_table, "DarkSlateGray3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 82, 139, 139); PL_HashTableAdd(x11_color_table, "DarkSlateGray4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 127, 255, 212); PL_HashTableAdd(x11_color_table, "aquamarine1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 118, 238, 198); PL_HashTableAdd(x11_color_table, "aquamarine2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 102, 205, 170); PL_HashTableAdd(x11_color_table, "aquamarine3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 69, 139, 116); PL_HashTableAdd(x11_color_table, "aquamarine4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 193, 255, 193); PL_HashTableAdd(x11_color_table, "DarkSeaGreen1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 180, 238, 180); PL_HashTableAdd(x11_color_table, "DarkSeaGreen2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 155, 205, 155); PL_HashTableAdd(x11_color_table, "DarkSeaGreen3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 105, 139, 105); PL_HashTableAdd(x11_color_table, "DarkSeaGreen4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 84, 255, 159); PL_HashTableAdd(x11_color_table, "SeaGreen1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 78, 238, 148); PL_HashTableAdd(x11_color_table, "SeaGreen2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 67, 205, 128); PL_HashTableAdd(x11_color_table, "SeaGreen3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 46, 139, 87); PL_HashTableAdd(x11_color_table, "SeaGreen4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 154, 255, 154); PL_HashTableAdd(x11_color_table, "PaleGreen1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 144, 238, 144); PL_HashTableAdd(x11_color_table, "PaleGreen2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 124, 205, 124); PL_HashTableAdd(x11_color_table, "PaleGreen3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 84, 139, 84); PL_HashTableAdd(x11_color_table, "PaleGreen4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 255, 127); PL_HashTableAdd(x11_color_table, "SpringGreen1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 238, 118); PL_HashTableAdd(x11_color_table, "SpringGreen2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 205, 102); PL_HashTableAdd(x11_color_table, "SpringGreen3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 139, 69); PL_HashTableAdd(x11_color_table, "SpringGreen4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 255, 0); PL_HashTableAdd(x11_color_table, "green1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 238, 0); PL_HashTableAdd(x11_color_table, "green2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 205, 0); PL_HashTableAdd(x11_color_table, "green3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 139, 0); PL_HashTableAdd(x11_color_table, "green4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 127, 255, 0); PL_HashTableAdd(x11_color_table, "chartreuse1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 118, 238, 0); PL_HashTableAdd(x11_color_table, "chartreuse2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 102, 205, 0); PL_HashTableAdd(x11_color_table, "chartreuse3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 69, 139, 0); PL_HashTableAdd(x11_color_table, "chartreuse4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 192, 255, 62); PL_HashTableAdd(x11_color_table, "OliveDrab1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 179, 238, 58); PL_HashTableAdd(x11_color_table, "OliveDrab2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 154, 205, 50); PL_HashTableAdd(x11_color_table, "OliveDrab3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 105, 139, 34); PL_HashTableAdd(x11_color_table, "OliveDrab4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 202, 255, 112); PL_HashTableAdd(x11_color_table, "DarkOliveGreen1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 188, 238, 104); PL_HashTableAdd(x11_color_table, "DarkOliveGreen2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 162, 205, 90); PL_HashTableAdd(x11_color_table, "DarkOliveGreen3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 110, 139, 61); PL_HashTableAdd(x11_color_table, "DarkOliveGreen4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 246, 143); PL_HashTableAdd(x11_color_table, "khaki1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 230, 133); PL_HashTableAdd(x11_color_table, "khaki2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 198, 115); PL_HashTableAdd(x11_color_table, "khaki3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 134, 78); PL_HashTableAdd(x11_color_table, "khaki4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 236, 139); PL_HashTableAdd(x11_color_table, "LightGoldenrod1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 220, 130); PL_HashTableAdd(x11_color_table, "LightGoldenrod2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 190, 112); PL_HashTableAdd(x11_color_table, "LightGoldenrod3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 129, 76); PL_HashTableAdd(x11_color_table, "LightGoldenrod4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 255, 224); PL_HashTableAdd(x11_color_table, "LightYellow1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 238, 209); PL_HashTableAdd(x11_color_table, "LightYellow2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 205, 180); PL_HashTableAdd(x11_color_table, "LightYellow3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 139, 122); PL_HashTableAdd(x11_color_table, "LightYellow4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 255, 0); PL_HashTableAdd(x11_color_table, "yellow1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 238, 0); PL_HashTableAdd(x11_color_table, "yellow2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 205, 0); PL_HashTableAdd(x11_color_table, "yellow3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 139, 0); PL_HashTableAdd(x11_color_table, "yellow4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 215, 0); PL_HashTableAdd(x11_color_table, "gold1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 201, 0); PL_HashTableAdd(x11_color_table, "gold2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 173, 0); PL_HashTableAdd(x11_color_table, "gold3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 117, 0); PL_HashTableAdd(x11_color_table, "gold4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 193, 37); PL_HashTableAdd(x11_color_table, "goldenrod1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 180, 34); PL_HashTableAdd(x11_color_table, "goldenrod2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 155, 29); PL_HashTableAdd(x11_color_table, "goldenrod3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 105, 20); PL_HashTableAdd(x11_color_table, "goldenrod4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 185, 15); PL_HashTableAdd(x11_color_table, "DarkGoldenrod1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 173, 14); PL_HashTableAdd(x11_color_table, "DarkGoldenrod2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 149, 12); PL_HashTableAdd(x11_color_table, "DarkGoldenrod3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 101, 8); PL_HashTableAdd(x11_color_table, "DarkGoldenrod4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 193, 193); PL_HashTableAdd(x11_color_table, "RosyBrown1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 180, 180); PL_HashTableAdd(x11_color_table, "RosyBrown2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 155, 155); PL_HashTableAdd(x11_color_table, "RosyBrown3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 105, 105); PL_HashTableAdd(x11_color_table, "RosyBrown4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 106, 106); PL_HashTableAdd(x11_color_table, "IndianRed1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 99, 99); PL_HashTableAdd(x11_color_table, "IndianRed2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 85, 85); PL_HashTableAdd(x11_color_table, "IndianRed3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 58, 58); PL_HashTableAdd(x11_color_table, "IndianRed4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 130, 71); PL_HashTableAdd(x11_color_table, "sienna1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 121, 66); PL_HashTableAdd(x11_color_table, "sienna2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 104, 57); PL_HashTableAdd(x11_color_table, "sienna3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 71, 38); PL_HashTableAdd(x11_color_table, "sienna4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 211, 155); PL_HashTableAdd(x11_color_table, "burlywood1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 197, 145); PL_HashTableAdd(x11_color_table, "burlywood2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 170, 125); PL_HashTableAdd(x11_color_table, "burlywood3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 115, 85); PL_HashTableAdd(x11_color_table, "burlywood4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 231, 186); PL_HashTableAdd(x11_color_table, "wheat1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 216, 174); PL_HashTableAdd(x11_color_table, "wheat2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 186, 150); PL_HashTableAdd(x11_color_table, "wheat3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 126, 102); PL_HashTableAdd(x11_color_table, "wheat4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 165, 79); PL_HashTableAdd(x11_color_table, "tan1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 154, 73); PL_HashTableAdd(x11_color_table, "tan2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 133, 63); PL_HashTableAdd(x11_color_table, "tan3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 90, 43); PL_HashTableAdd(x11_color_table, "tan4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 127, 36); PL_HashTableAdd(x11_color_table, "chocolate1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 118, 33); PL_HashTableAdd(x11_color_table, "chocolate2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 102, 29); PL_HashTableAdd(x11_color_table, "chocolate3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 69, 19); PL_HashTableAdd(x11_color_table, "chocolate4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 48, 48); PL_HashTableAdd(x11_color_table, "firebrick1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 44, 44); PL_HashTableAdd(x11_color_table, "firebrick2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 38, 38); PL_HashTableAdd(x11_color_table, "firebrick3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 26, 26); PL_HashTableAdd(x11_color_table, "firebrick4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 64, 64); PL_HashTableAdd(x11_color_table, "brown1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 59, 59); PL_HashTableAdd(x11_color_table, "brown2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 51, 51); PL_HashTableAdd(x11_color_table, "brown3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 35, 35); PL_HashTableAdd(x11_color_table, "brown4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 140, 105); PL_HashTableAdd(x11_color_table, "salmon1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 130, 98); PL_HashTableAdd(x11_color_table, "salmon2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 112, 84); PL_HashTableAdd(x11_color_table, "salmon3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 76, 57); PL_HashTableAdd(x11_color_table, "salmon4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 160, 122); PL_HashTableAdd(x11_color_table, "LightSalmon1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 149, 114); PL_HashTableAdd(x11_color_table, "LightSalmon2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 129, 98); PL_HashTableAdd(x11_color_table, "LightSalmon3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 87, 66); PL_HashTableAdd(x11_color_table, "LightSalmon4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 165, 0); PL_HashTableAdd(x11_color_table, "orange1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 154, 0); PL_HashTableAdd(x11_color_table, "orange2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 133, 0); PL_HashTableAdd(x11_color_table, "orange3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 90, 0); PL_HashTableAdd(x11_color_table, "orange4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 127, 0); PL_HashTableAdd(x11_color_table, "DarkOrange1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 118, 0); PL_HashTableAdd(x11_color_table, "DarkOrange2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 102, 0); PL_HashTableAdd(x11_color_table, "DarkOrange3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 69, 0); PL_HashTableAdd(x11_color_table, "DarkOrange4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 114, 86); PL_HashTableAdd(x11_color_table, "coral1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 106, 80); PL_HashTableAdd(x11_color_table, "coral2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 91, 69); PL_HashTableAdd(x11_color_table, "coral3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 62, 47); PL_HashTableAdd(x11_color_table, "coral4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 99, 71); PL_HashTableAdd(x11_color_table, "tomato1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 92, 66); PL_HashTableAdd(x11_color_table, "tomato2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 79, 57); PL_HashTableAdd(x11_color_table, "tomato3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 54, 38); PL_HashTableAdd(x11_color_table, "tomato4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 69, 0); PL_HashTableAdd(x11_color_table, "OrangeRed1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 64, 0); PL_HashTableAdd(x11_color_table, "OrangeRed2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 55, 0); PL_HashTableAdd(x11_color_table, "OrangeRed3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 37, 0); PL_HashTableAdd(x11_color_table, "OrangeRed4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 0, 0); PL_HashTableAdd(x11_color_table, "red1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 0, 0); PL_HashTableAdd(x11_color_table, "red2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 0, 0); PL_HashTableAdd(x11_color_table, "red3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 0, 0); PL_HashTableAdd(x11_color_table, "red4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 215, 7, 81); PL_HashTableAdd(x11_color_table, "DebianRed", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 20, 147); PL_HashTableAdd(x11_color_table, "DeepPink1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 18, 137); PL_HashTableAdd(x11_color_table, "DeepPink2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 16, 118); PL_HashTableAdd(x11_color_table, "DeepPink3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 10, 80); PL_HashTableAdd(x11_color_table, "DeepPink4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 110, 180); PL_HashTableAdd(x11_color_table, "HotPink1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 106, 167); PL_HashTableAdd(x11_color_table, "HotPink2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 96, 144); PL_HashTableAdd(x11_color_table, "HotPink3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 58, 98); PL_HashTableAdd(x11_color_table, "HotPink4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 181, 197); PL_HashTableAdd(x11_color_table, "pink1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 169, 184); PL_HashTableAdd(x11_color_table, "pink2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 145, 158); PL_HashTableAdd(x11_color_table, "pink3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 99, 108); PL_HashTableAdd(x11_color_table, "pink4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 174, 185); PL_HashTableAdd(x11_color_table, "LightPink1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 162, 173); PL_HashTableAdd(x11_color_table, "LightPink2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 140, 149); PL_HashTableAdd(x11_color_table, "LightPink3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 95, 101); PL_HashTableAdd(x11_color_table, "LightPink4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 130, 171); PL_HashTableAdd(x11_color_table, "PaleVioletRed1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 121, 159); PL_HashTableAdd(x11_color_table, "PaleVioletRed2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 104, 137); PL_HashTableAdd(x11_color_table, "PaleVioletRed3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 71, 93); PL_HashTableAdd(x11_color_table, "PaleVioletRed4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 52, 179); PL_HashTableAdd(x11_color_table, "maroon1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 48, 167); PL_HashTableAdd(x11_color_table, "maroon2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 41, 144); PL_HashTableAdd(x11_color_table, "maroon3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 28, 98); PL_HashTableAdd(x11_color_table, "maroon4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 62, 150); PL_HashTableAdd(x11_color_table, "VioletRed1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 58, 140); PL_HashTableAdd(x11_color_table, "VioletRed2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 50, 120); PL_HashTableAdd(x11_color_table, "VioletRed3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 34, 82); PL_HashTableAdd(x11_color_table, "VioletRed4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 0, 255); PL_HashTableAdd(x11_color_table, "magenta1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 0, 238); PL_HashTableAdd(x11_color_table, "magenta2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 0, 205); PL_HashTableAdd(x11_color_table, "magenta3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 0, 139); PL_HashTableAdd(x11_color_table, "magenta4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 131, 250); PL_HashTableAdd(x11_color_table, "orchid1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 122, 233); PL_HashTableAdd(x11_color_table, "orchid2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 105, 201); PL_HashTableAdd(x11_color_table, "orchid3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 71, 137); PL_HashTableAdd(x11_color_table, "orchid4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 187, 255); PL_HashTableAdd(x11_color_table, "plum1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 174, 238); PL_HashTableAdd(x11_color_table, "plum2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 150, 205); PL_HashTableAdd(x11_color_table, "plum3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 102, 139); PL_HashTableAdd(x11_color_table, "plum4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 224, 102, 255); PL_HashTableAdd(x11_color_table, "MediumOrchid1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 209, 95, 238); PL_HashTableAdd(x11_color_table, "MediumOrchid2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 180, 82, 205); PL_HashTableAdd(x11_color_table, "MediumOrchid3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 122, 55, 139); PL_HashTableAdd(x11_color_table, "MediumOrchid4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 191, 62, 255); PL_HashTableAdd(x11_color_table, "DarkOrchid1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 178, 58, 238); PL_HashTableAdd(x11_color_table, "DarkOrchid2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 154, 50, 205); PL_HashTableAdd(x11_color_table, "DarkOrchid3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 104, 34, 139); PL_HashTableAdd(x11_color_table, "DarkOrchid4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 155, 48, 255); PL_HashTableAdd(x11_color_table, "purple1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 145, 44, 238); PL_HashTableAdd(x11_color_table, "purple2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 125, 38, 205); PL_HashTableAdd(x11_color_table, "purple3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 85, 26, 139); PL_HashTableAdd(x11_color_table, "purple4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 171, 130, 255); PL_HashTableAdd(x11_color_table, "MediumPurple1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 159, 121, 238); PL_HashTableAdd(x11_color_table, "MediumPurple2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 137, 104, 205); PL_HashTableAdd(x11_color_table, "MediumPurple3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 93, 71, 139); PL_HashTableAdd(x11_color_table, "MediumPurple4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 225, 255); PL_HashTableAdd(x11_color_table, "thistle1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 238, 210, 238); PL_HashTableAdd(x11_color_table, "thistle2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 205, 181, 205); PL_HashTableAdd(x11_color_table, "thistle3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 123, 139); PL_HashTableAdd(x11_color_table, "thistle4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 0); PL_HashTableAdd(x11_color_table, "gray0", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 0); PL_HashTableAdd(x11_color_table, "grey0", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 3, 3, 3); PL_HashTableAdd(x11_color_table, "gray1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 3, 3, 3); PL_HashTableAdd(x11_color_table, "grey1", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 5, 5, 5); PL_HashTableAdd(x11_color_table, "gray2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 5, 5, 5); PL_HashTableAdd(x11_color_table, "grey2", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 8, 8, 8); PL_HashTableAdd(x11_color_table, "gray3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 8, 8, 8); PL_HashTableAdd(x11_color_table, "grey3", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 10, 10, 10); PL_HashTableAdd(x11_color_table, "gray4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 10, 10, 10); PL_HashTableAdd(x11_color_table, "grey4", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 13, 13, 13); PL_HashTableAdd(x11_color_table, "gray5", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 13, 13, 13); PL_HashTableAdd(x11_color_table, "grey5", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 15, 15, 15); PL_HashTableAdd(x11_color_table, "gray6", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 15, 15, 15); PL_HashTableAdd(x11_color_table, "grey6", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 18, 18, 18); PL_HashTableAdd(x11_color_table, "gray7", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 18, 18, 18); PL_HashTableAdd(x11_color_table, "grey7", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 20, 20, 20); PL_HashTableAdd(x11_color_table, "gray8", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 20, 20, 20); PL_HashTableAdd(x11_color_table, "grey8", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 23, 23, 23); PL_HashTableAdd(x11_color_table, "gray9", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 23, 23, 23); PL_HashTableAdd(x11_color_table, "grey9", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 26, 26, 26); PL_HashTableAdd(x11_color_table, "gray10", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 26, 26, 26); PL_HashTableAdd(x11_color_table, "grey10", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 28, 28, 28); PL_HashTableAdd(x11_color_table, "gray11", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 28, 28, 28); PL_HashTableAdd(x11_color_table, "grey11", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 31, 31, 31); PL_HashTableAdd(x11_color_table, "gray12", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 31, 31, 31); PL_HashTableAdd(x11_color_table, "grey12", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 33, 33, 33); PL_HashTableAdd(x11_color_table, "gray13", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 33, 33, 33); PL_HashTableAdd(x11_color_table, "grey13", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 36, 36, 36); PL_HashTableAdd(x11_color_table, "gray14", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 36, 36, 36); PL_HashTableAdd(x11_color_table, "grey14", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 38, 38, 38); PL_HashTableAdd(x11_color_table, "gray15", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 38, 38, 38); PL_HashTableAdd(x11_color_table, "grey15", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 41, 41, 41); PL_HashTableAdd(x11_color_table, "gray16", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 41, 41, 41); PL_HashTableAdd(x11_color_table, "grey16", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 43, 43, 43); PL_HashTableAdd(x11_color_table, "gray17", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 43, 43, 43); PL_HashTableAdd(x11_color_table, "grey17", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 46, 46, 46); PL_HashTableAdd(x11_color_table, "gray18", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 46, 46, 46); PL_HashTableAdd(x11_color_table, "grey18", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 48, 48, 48); PL_HashTableAdd(x11_color_table, "gray19", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 48, 48, 48); PL_HashTableAdd(x11_color_table, "grey19", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 51, 51, 51); PL_HashTableAdd(x11_color_table, "gray20", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 51, 51, 51); PL_HashTableAdd(x11_color_table, "grey20", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 54, 54, 54); PL_HashTableAdd(x11_color_table, "gray21", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 54, 54, 54); PL_HashTableAdd(x11_color_table, "grey21", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 56, 56, 56); PL_HashTableAdd(x11_color_table, "gray22", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 56, 56, 56); PL_HashTableAdd(x11_color_table, "grey22", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 59, 59, 59); PL_HashTableAdd(x11_color_table, "gray23", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 59, 59, 59); PL_HashTableAdd(x11_color_table, "grey23", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 61, 61, 61); PL_HashTableAdd(x11_color_table, "gray24", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 61, 61, 61); PL_HashTableAdd(x11_color_table, "grey24", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 64, 64, 64); PL_HashTableAdd(x11_color_table, "gray25", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 64, 64, 64); PL_HashTableAdd(x11_color_table, "grey25", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 66, 66, 66); PL_HashTableAdd(x11_color_table, "gray26", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 66, 66, 66); PL_HashTableAdd(x11_color_table, "grey26", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 69, 69, 69); PL_HashTableAdd(x11_color_table, "gray27", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 69, 69, 69); PL_HashTableAdd(x11_color_table, "grey27", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 71, 71, 71); PL_HashTableAdd(x11_color_table, "gray28", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 71, 71, 71); PL_HashTableAdd(x11_color_table, "grey28", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 74, 74, 74); PL_HashTableAdd(x11_color_table, "gray29", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 74, 74, 74); PL_HashTableAdd(x11_color_table, "grey29", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 77, 77, 77); PL_HashTableAdd(x11_color_table, "gray30", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 77, 77, 77); PL_HashTableAdd(x11_color_table, "grey30", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 79, 79, 79); PL_HashTableAdd(x11_color_table, "gray31", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 79, 79, 79); PL_HashTableAdd(x11_color_table, "grey31", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 82, 82, 82); PL_HashTableAdd(x11_color_table, "gray32", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 82, 82, 82); PL_HashTableAdd(x11_color_table, "grey32", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 84, 84, 84); PL_HashTableAdd(x11_color_table, "gray33", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 84, 84, 84); PL_HashTableAdd(x11_color_table, "grey33", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 87, 87, 87); PL_HashTableAdd(x11_color_table, "gray34", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 87, 87, 87); PL_HashTableAdd(x11_color_table, "grey34", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 89, 89, 89); PL_HashTableAdd(x11_color_table, "gray35", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 89, 89, 89); PL_HashTableAdd(x11_color_table, "grey35", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 92, 92, 92); PL_HashTableAdd(x11_color_table, "gray36", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 92, 92, 92); PL_HashTableAdd(x11_color_table, "grey36", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 94, 94, 94); PL_HashTableAdd(x11_color_table, "gray37", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 94, 94, 94); PL_HashTableAdd(x11_color_table, "grey37", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 97, 97, 97); PL_HashTableAdd(x11_color_table, "gray38", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 97, 97, 97); PL_HashTableAdd(x11_color_table, "grey38", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 99, 99, 99); PL_HashTableAdd(x11_color_table, "gray39", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 99, 99, 99); PL_HashTableAdd(x11_color_table, "grey39", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 102, 102, 102); PL_HashTableAdd(x11_color_table, "gray40", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 102, 102, 102); PL_HashTableAdd(x11_color_table, "grey40", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 105, 105, 105); PL_HashTableAdd(x11_color_table, "gray41", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 105, 105, 105); PL_HashTableAdd(x11_color_table, "grey41", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 107, 107, 107); PL_HashTableAdd(x11_color_table, "gray42", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 107, 107, 107); PL_HashTableAdd(x11_color_table, "grey42", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 110, 110, 110); PL_HashTableAdd(x11_color_table, "gray43", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 110, 110, 110); PL_HashTableAdd(x11_color_table, "grey43", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 112, 112, 112); PL_HashTableAdd(x11_color_table, "gray44", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 112, 112, 112); PL_HashTableAdd(x11_color_table, "grey44", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 115, 115, 115); PL_HashTableAdd(x11_color_table, "gray45", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 115, 115, 115); PL_HashTableAdd(x11_color_table, "grey45", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 117, 117, 117); PL_HashTableAdd(x11_color_table, "gray46", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 117, 117, 117); PL_HashTableAdd(x11_color_table, "grey46", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 120, 120, 120); PL_HashTableAdd(x11_color_table, "gray47", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 120, 120, 120); PL_HashTableAdd(x11_color_table, "grey47", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 122, 122, 122); PL_HashTableAdd(x11_color_table, "gray48", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 122, 122, 122); PL_HashTableAdd(x11_color_table, "grey48", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 125, 125, 125); PL_HashTableAdd(x11_color_table, "gray49", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 125, 125, 125); PL_HashTableAdd(x11_color_table, "grey49", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 127, 127, 127); PL_HashTableAdd(x11_color_table, "gray50", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 127, 127, 127); PL_HashTableAdd(x11_color_table, "grey50", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 130, 130, 130); PL_HashTableAdd(x11_color_table, "gray51", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 130, 130, 130); PL_HashTableAdd(x11_color_table, "grey51", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 133, 133, 133); PL_HashTableAdd(x11_color_table, "gray52", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 133, 133, 133); PL_HashTableAdd(x11_color_table, "grey52", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 135, 135, 135); PL_HashTableAdd(x11_color_table, "gray53", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 135, 135, 135); PL_HashTableAdd(x11_color_table, "grey53", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 138, 138, 138); PL_HashTableAdd(x11_color_table, "gray54", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 138, 138, 138); PL_HashTableAdd(x11_color_table, "grey54", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 140, 140, 140); PL_HashTableAdd(x11_color_table, "gray55", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 140, 140, 140); PL_HashTableAdd(x11_color_table, "grey55", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 143, 143, 143); PL_HashTableAdd(x11_color_table, "gray56", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 143, 143, 143); PL_HashTableAdd(x11_color_table, "grey56", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 145, 145, 145); PL_HashTableAdd(x11_color_table, "gray57", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 145, 145, 145); PL_HashTableAdd(x11_color_table, "grey57", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 148, 148, 148); PL_HashTableAdd(x11_color_table, "gray58", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 148, 148, 148); PL_HashTableAdd(x11_color_table, "grey58", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 150, 150, 150); PL_HashTableAdd(x11_color_table, "gray59", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 150, 150, 150); PL_HashTableAdd(x11_color_table, "grey59", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 153, 153, 153); PL_HashTableAdd(x11_color_table, "gray60", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 153, 153, 153); PL_HashTableAdd(x11_color_table, "grey60", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 156, 156, 156); PL_HashTableAdd(x11_color_table, "gray61", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 156, 156, 156); PL_HashTableAdd(x11_color_table, "grey61", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 158, 158, 158); PL_HashTableAdd(x11_color_table, "gray62", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 158, 158, 158); PL_HashTableAdd(x11_color_table, "grey62", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 161, 161, 161); PL_HashTableAdd(x11_color_table, "gray63", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 161, 161, 161); PL_HashTableAdd(x11_color_table, "grey63", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 163, 163, 163); PL_HashTableAdd(x11_color_table, "gray64", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 163, 163, 163); PL_HashTableAdd(x11_color_table, "grey64", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 166, 166, 166); PL_HashTableAdd(x11_color_table, "gray65", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 166, 166, 166); PL_HashTableAdd(x11_color_table, "grey65", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 168, 168, 168); PL_HashTableAdd(x11_color_table, "gray66", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 168, 168, 168); PL_HashTableAdd(x11_color_table, "grey66", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 171, 171, 171); PL_HashTableAdd(x11_color_table, "gray67", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 171, 171, 171); PL_HashTableAdd(x11_color_table, "grey67", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 173, 173, 173); PL_HashTableAdd(x11_color_table, "gray68", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 173, 173, 173); PL_HashTableAdd(x11_color_table, "grey68", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 176, 176, 176); PL_HashTableAdd(x11_color_table, "gray69", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 176, 176, 176); PL_HashTableAdd(x11_color_table, "grey69", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 179, 179, 179); PL_HashTableAdd(x11_color_table, "gray70", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 179, 179, 179); PL_HashTableAdd(x11_color_table, "grey70", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 181, 181, 181); PL_HashTableAdd(x11_color_table, "gray71", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 181, 181, 181); PL_HashTableAdd(x11_color_table, "grey71", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 184, 184, 184); PL_HashTableAdd(x11_color_table, "gray72", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 184, 184, 184); PL_HashTableAdd(x11_color_table, "grey72", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 186, 186, 186); PL_HashTableAdd(x11_color_table, "gray73", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 186, 186, 186); PL_HashTableAdd(x11_color_table, "grey73", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 189, 189, 189); PL_HashTableAdd(x11_color_table, "gray74", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 189, 189, 189); PL_HashTableAdd(x11_color_table, "grey74", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 191, 191, 191); PL_HashTableAdd(x11_color_table, "gray75", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 191, 191, 191); PL_HashTableAdd(x11_color_table, "grey75", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 194, 194, 194); PL_HashTableAdd(x11_color_table, "gray76", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 194, 194, 194); PL_HashTableAdd(x11_color_table, "grey76", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 196, 196, 196); PL_HashTableAdd(x11_color_table, "gray77", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 196, 196, 196); PL_HashTableAdd(x11_color_table, "grey77", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 199, 199, 199); PL_HashTableAdd(x11_color_table, "gray78", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 199, 199, 199); PL_HashTableAdd(x11_color_table, "grey78", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 201, 201, 201); PL_HashTableAdd(x11_color_table, "gray79", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 201, 201, 201); PL_HashTableAdd(x11_color_table, "grey79", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 204, 204, 204); PL_HashTableAdd(x11_color_table, "gray80", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 204, 204, 204); PL_HashTableAdd(x11_color_table, "grey80", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 207, 207, 207); PL_HashTableAdd(x11_color_table, "gray81", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 207, 207, 207); PL_HashTableAdd(x11_color_table, "grey81", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 209, 209, 209); PL_HashTableAdd(x11_color_table, "gray82", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 209, 209, 209); PL_HashTableAdd(x11_color_table, "grey82", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 212, 212, 212); PL_HashTableAdd(x11_color_table, "gray83", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 212, 212, 212); PL_HashTableAdd(x11_color_table, "grey83", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 214, 214, 214); PL_HashTableAdd(x11_color_table, "gray84", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 214, 214, 214); PL_HashTableAdd(x11_color_table, "grey84", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 217, 217, 217); PL_HashTableAdd(x11_color_table, "gray85", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 217, 217, 217); PL_HashTableAdd(x11_color_table, "grey85", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 219, 219, 219); PL_HashTableAdd(x11_color_table, "gray86", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 219, 219, 219); PL_HashTableAdd(x11_color_table, "grey86", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 222, 222, 222); PL_HashTableAdd(x11_color_table, "gray87", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 222, 222, 222); PL_HashTableAdd(x11_color_table, "grey87", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 224, 224, 224); PL_HashTableAdd(x11_color_table, "gray88", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 224, 224, 224); PL_HashTableAdd(x11_color_table, "grey88", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 227, 227, 227); PL_HashTableAdd(x11_color_table, "gray89", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 227, 227, 227); PL_HashTableAdd(x11_color_table, "grey89", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 229, 229, 229); PL_HashTableAdd(x11_color_table, "gray90", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 229, 229, 229); PL_HashTableAdd(x11_color_table, "grey90", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 232, 232, 232); PL_HashTableAdd(x11_color_table, "gray91", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 232, 232, 232); PL_HashTableAdd(x11_color_table, "grey91", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 235, 235, 235); PL_HashTableAdd(x11_color_table, "gray92", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 235, 235, 235); PL_HashTableAdd(x11_color_table, "grey92", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 237, 237, 237); PL_HashTableAdd(x11_color_table, "gray93", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 237, 237, 237); PL_HashTableAdd(x11_color_table, "grey93", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 240, 240); PL_HashTableAdd(x11_color_table, "gray94", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 240, 240, 240); PL_HashTableAdd(x11_color_table, "grey94", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 242, 242, 242); PL_HashTableAdd(x11_color_table, "gray95", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 242, 242, 242); PL_HashTableAdd(x11_color_table, "grey95", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 245, 245, 245); PL_HashTableAdd(x11_color_table, "gray96", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 245, 245, 245); PL_HashTableAdd(x11_color_table, "grey96", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 247, 247, 247); PL_HashTableAdd(x11_color_table, "gray97", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 247, 247, 247); PL_HashTableAdd(x11_color_table, "grey97", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 250, 250, 250); PL_HashTableAdd(x11_color_table, "gray98", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 250, 250, 250); PL_HashTableAdd(x11_color_table, "grey98", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 252, 252, 252); PL_HashTableAdd(x11_color_table, "gray99", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 252, 252, 252); PL_HashTableAdd(x11_color_table, "grey99", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 255, 255); PL_HashTableAdd(x11_color_table, "gray100", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 255, 255, 255); PL_HashTableAdd(x11_color_table, "grey100", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 169, 169, 169); PL_HashTableAdd(x11_color_table, "dark grey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 169, 169, 169); PL_HashTableAdd(x11_color_table, "DarkGrey", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 169, 169, 169); PL_HashTableAdd(x11_color_table, "dark gray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 169, 169, 169); PL_HashTableAdd(x11_color_table, "DarkGray", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 139); PL_HashTableAdd(x11_color_table, "dark blue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 0, 139); PL_HashTableAdd(x11_color_table, "DarkBlue", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 139, 139); PL_HashTableAdd(x11_color_table, "dark cyan", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 0, 139, 139); PL_HashTableAdd(x11_color_table, "DarkCyan", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 0, 139); PL_HashTableAdd(x11_color_table, "dark magenta", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 0, 139); PL_HashTableAdd(x11_color_table, "DarkMagenta", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 0, 0); PL_HashTableAdd(x11_color_table, "dark red", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 139, 0, 0); PL_HashTableAdd(x11_color_table, "DarkRed", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 144, 238, 144); PL_HashTableAdd(x11_color_table, "light green", val);
	val = PR_NEW(PRUint32); *val = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_DO_NOT_FREE_KEY, 144, 238, 144); PL_HashTableAdd(x11_color_table, "LightGreen", val);
}

void irck_xpm_deinit(void) {
	if (x11_color_table) {
		PL_HashTableEnumerateEntries(x11_color_table, cleanup_color_table, NULL);
		PL_HashTableDestroy(x11_color_table);
	}
}

static PRUint32 parse_named_color(char *name) {
	if (!PL_strcasecmp(name, "None")) {
		return IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_TRANSPARENT, 0, 0, 0);
	} else {
		PRUint32 *val;
		
		if (!x11_color_table) {
			init_color_table();
		}
		val = (PRUint32*)PL_HashTableLookup(x11_color_table, name);
		if (val) {
			return *val;
		} else {
			return IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB, 0, 0, 0);
		}
	}
	
	return 0;
}

int count_valid_chars(char *tocount) {
	int i, c;
		
	for (i = c = 0; i < strlen(tocount); i++) {
		if (tocount[i] == '#' || rck_isdigit_ascii(tocount[i]) || rck_isletter_ascii(tocount[i])) {
			c++;
		}
	}
	
	return c;
}

static void parse_xpm2_line(RCKXPMImage *hxpm, char *line) {
	char *k, *kstate;
	int i;
	
	i = 0;
	if (!hxpm->stage) {
		hxpm->stage = IRCK_XPM_STAGE_XPM2_INTEGER_BLOCK;
				
		kstate = line;
		while ((k = PL_strtok_r(kstate, " ", &kstate))) {
			unsigned int klen;
			int j;
						
			klen = strlen(k);
			if (klen > 8) {
				hxpm->stage = IRCK_XPM_STAGE_ERROR;
				return;
			}
			
			j = 0;
			while (klen > j) {
				if (!rck_isdigit_ascii(k[j])) {
					hxpm->stage = IRCK_XPM_STAGE_ERROR;
					return;
				}
				
				j++;
			}
						
			switch (i) {
			  case 0:
				hxpm->w = atoi(k);
				break;
			  case 1:
				hxpm->h = atoi(k);
				hxpm->infomask |= RCK_XPM_INFO_MASK_DIMENSIONS;
				break;
			  case 2:
				hxpm->colorcount = atoi(k);
				break;
			  case 3:
				hxpm->chrperpel = atoi(k);
				break;
			  case 4:
				hxpm->hotx = atoi(k);
				break;
			  case 5:
				hxpm->hoty = atoi(k);
				hxpm->infomask |= RCK_XPM_INFO_MASK_HOTSPOT;
				break;
			  default:
				return;
			}

			i++;
		}
		
		if (hxpm->w == 0 || hxpm->h == 0 || hxpm->colorcount == 0 || hxpm->chrperpel == 0) {
			hxpm->stage = IRCK_XPM_STAGE_ERROR;
			return;
		}
		
		return;
	} else if (hxpm->stage == IRCK_XPM_STAGE_XPM2_INTEGER_BLOCK) {		
		PRUint32 *table_val;
		PRUint32 color_best, color_worse, color_worst;
		
		if (hxpm->iparam >= hxpm->colorcount) {
			unsigned int total_pixels;
			
			hxpm->iparam = 0;
			hxpm->stage = IRCK_XPM_STAGE_XPM2_COLOR_DEFINES;
			
			if (hxpm->w > 0 && hxpm->h > 0 && hxpm->w > (UINT_MAX / hxpm->h)) {
				hxpm->stage = IRCK_XPM_STAGE_ERROR;
				return;
			}
			
			total_pixels = hxpm->w * hxpm->h;
			if (total_pixels > (UINT_MAX / sizeof(PRUint32))) {
				hxpm->stage = IRCK_XPM_STAGE_ERROR;
				return;
			}

			hxpm->argb8888 = calloc(total_pixels, sizeof(PRUint32));
			if (!hxpm->argb8888) {
				hxpm->stage = IRCK_XPM_STAGE_ERROR;
				return;
			}
			goto PARSE_PIXELS;
		} else if (!hxpm->iparam) {			
			hxpm->color_table = PL_NewHashTable(hxpm->colorcount, PL_HashString, PL_CompareStrings, compare_uint32, NULL, NULL);
			
			if (hxpm->chrperpel >= UINT_MAX) {
				hxpm->stage = IRCK_XPM_STAGE_ERROR;
				return;
			}
			
			hxpm->color_table_key = calloc(hxpm->chrperpel + 1, sizeof(char));
			if (!hxpm->color_table_key) {
				hxpm->stage = IRCK_XPM_STAGE_ERROR;
				return;
			}
		}

		if (strlen(line) <= hxpm->chrperpel) {
			hxpm->stage = IRCK_XPM_STAGE_ERROR;
			return;
		}
		
		color_best = color_worse = color_worst = 0;
		strncpy(hxpm->color_table_key, line, hxpm->chrperpel);
		hxpm->color_table_key[hxpm->chrperpel] = '\0';
		line += hxpm->chrperpel;
		kstate = line;
		while ((k = PL_strtok_r(kstate, " ", &kstate))) {
			IRCK_SKIP_WHITESPACE(k);
			if (!hxpm->cparam) {
				hxpm->cparam = k[0];
			} else {
				switch (hxpm->cparam) {
				  case 'c':
					if (k[0] == '#' && count_valid_chars(k) == 13) {
						PRUint32 r, g, b;
						
						if (sscanf(k, "#%4x%4x%4x", &r, &g, &b) == 3) {							
							r = (r & 0xFFFF) / 257;
							g = (g & 0xFFFF) / 257;
							b = (b & 0xFFFF) / 257;
							color_best = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB, r, g, b);
						} else {
							hxpm->stage = IRCK_XPM_STAGE_ERROR;
							return;
						}
					} else if (k[0] == '#' && count_valid_chars(k) == 7) {
						unsigned int r, g, b;
						
						if (sscanf(k, "#%02x%02x%02x", &r, &g, &b) == 3) {							
							color_best = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB, r, g, b);
						} else {
							hxpm->stage = IRCK_XPM_STAGE_ERROR;
							return;
						}
					} else if (k[0] == '#') {
						hxpm->stage = IRCK_XPM_STAGE_ERROR;
						return;
					} else {
						color_best = parse_named_color(k);
					}
					break;
				  case 'm':
					color_worst = parse_named_color(k);
					break;
				  case 'g':
					color_worse = parse_named_color(k);
					break;
				  case 's':
					break;
				  default:
					hxpm->stage = IRCK_XPM_STAGE_ERROR;
					return;
				}
								
				hxpm->cparam = 0;
			}
		}
		
		if (!color_worse) {	
			color_worse = color_worst;
		}
				
		if (!color_best) {
			color_best = color_worse;
		}
		
		table_val = PR_NEW(PRUint32); 
		*table_val = color_best;
		PL_HashTableAdd(hxpm->color_table, PL_strdup(hxpm->color_table_key), table_val);
		
		hxpm->iparam++;
	} else if (hxpm->stage == IRCK_XPM_STAGE_XPM2_COLOR_DEFINES) {		
		PARSE_PIXELS:
		int i;
		
		if (hxpm->iparam >= hxpm->h) {
			hxpm->stage = IRCK_XPM_STAGE_ERROR;
			return;
		}

		if (strlen(line) < (hxpm->w * hxpm->chrperpel)) {
			hxpm->stage = IRCK_XPM_STAGE_ERROR;
			return;
		}			
		
		for (i = 0; i < hxpm->w; i++) {
			PRUint32 *valp;
			unsigned int line_len;
			
			line_len = strlen(line);
		
			if (!line_len) {
				hxpm->argb8888[hxpm->iparam * hxpm->w + i] = IRCK_XPM_PACK_RGB(255, 0, 0, 0);
				continue;
			}
			
			if (line_len < hxpm->chrperpel) {
				hxpm->argb8888[hxpm->iparam * hxpm->w + i] = IRCK_XPM_PACK_RGB(255, 0, 0, 0);
				continue;
			}
			
			strncpy(hxpm->color_table_key, line, hxpm->chrperpel);
			hxpm->color_table_key[hxpm->chrperpel] = '\0';
			line += hxpm->chrperpel;

			valp = (PRUint32*)PL_HashTableLookup(hxpm->color_table, hxpm->color_table_key);
			if (valp) {
				PRUint32 val;
				
				val = *valp;
				if (((val >> 24) & 0xFF) == IRCK_XPM_COLOR_INFO_TRANSPARENT) {
					val = (val & 0x00FFFFFF);
				} else {
					val = (val & 0x00FFFFFF) | (255u << 24);
				}
				
				hxpm->argb8888[hxpm->iparam * hxpm->w + i] = val;
			} else {
				hxpm->argb8888[hxpm->iparam * hxpm->w + i] = IRCK_XPM_PACK_RGB(255, 0, 0, 0);
			}
		}

		hxpm->iparam++;
		if (hxpm->iparam == hxpm->h) {
			free(hxpm->color_table_key);
			hxpm->stage = IRCK_XPM_STAGE_DONE;
		}
	}
}

static int xpm1_check(char* input, char** outPrefix) {
    regex_t regex;
    regmatch_t matches[3]; 
    int version;
    
    version = -1;
    *outPrefix = NULL; 

    if (regcomp(&regex, "^[[:space:]]*#[[:space:]]*define[[:space:]]+([a-zA-Z0-9_]+)_format[[:space:]]+([0-9]+)", REG_EXTENDED | REG_NEWLINE)) {
        return -1;
    }

    if (!regexec(&regex, input, 3, matches, 0)) {
		char *prefix;
		regoff_t f_start;
        regoff_t f_end;
        regoff_t p_start;
        regoff_t p_end;
        size_t p_len;
        size_t f_len;
        
		p_start = matches[1].rm_so;
        p_end = matches[1].rm_eo;
        p_len = p_end - p_start;
     
        prefix = malloc(p_len + 1);
        if (prefix) {
            strncpy(prefix, input + p_start, p_len);
            prefix[p_len] = '\0';
        }
		*outPrefix = prefix; 

        f_start = matches[2].rm_so;
        f_end = matches[2].rm_eo; 
        f_len = f_end - f_start; 

        if (f_len > 3) {
            if (*outPrefix) { 
                free(*outPrefix); 
                *outPrefix = NULL; 
            }
            regfree(&regex);
            return -1;
        }

        version = atoi(input + f_start);
    }

    regfree(&regex);
    return version;
}

static int xpm1_get_value(char* input, const char* prefix, const char* suffix) {
	char *pattern;
    regex_t regex;
    regmatch_t matches[2];
	int value;
	#define PATTERN_TEMPLATE "^[[:space:]]*#[[:space:]]*define[[:space:]]+%s_%s[[:space:]]+([0-9]+)"
	
	value = -1;
	pattern = PR_smprintf(PATTERN_TEMPLATE, prefix, suffix);
    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NEWLINE)) {
        return -1;
    }
	PR_smprintf_free(pattern);

    if (!regexec(&regex, input, 2, matches, 0)) {
        regoff_t f_start;
        regoff_t f_end;
        size_t f_len;

        f_start = matches[1].rm_so;
        f_end = matches[1].rm_eo;
        f_len = f_end - f_start;
        
        if (f_len > 8) {
            regfree(&regex);
            return -1;
        }

        value = atoi(input + f_start);
    }

    regfree(&regex);
    return value;
}

static void xpm1_parse(RCKXPMImage *hxpm, char* input) {
    regex_t regex;
    regex_t h_regex;
    regmatch_t matches[3];
    regmatch_t h_matches[1];
    char *pattern;
    const char *cursor;
	unsigned int total_pixels;
	int ival;
	#define PATTERN_TEMPLATE_HPALETTE "static[[:space:]]+char[[:space:]]+\\*[[:space:]]*%s_colors\\[\\][[:space:]]*=[[:space:]]*\\{"
	#define PATTERN_TEMPLATE_HPIXELS "static[[:space:]]+char[[:space:]]+\\*[[:space:]]*%s_pixels\\[\\][[:space:]]*=[[:space:]]*\\{"
	
	hxpm->stage = IRCK_XPM_STAGE_XPM1_IN_PROGESS;
	hxpm->infomask |= RCK_XPM_INFO_MASK_VERSION;

	/* int values */
	ival = xpm1_get_value(input, hxpm->sparam, "width");
	if (ival >= 0) {
		hxpm->w = ival;
	} else {
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}
	
	ival = xpm1_get_value(input, hxpm->sparam, "height");
	if (ival >= 0) {
		hxpm->h = ival;
	} else {
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}
	
	hxpm->infomask |= RCK_XPM_INFO_MASK_DIMENSIONS;
	
	ival = xpm1_get_value(input, hxpm->sparam, "ncolors");
	if (ival >= 0) {
		hxpm->colorcount = ival;
	} else {
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}

	ival = xpm1_get_value(input, hxpm->sparam, "chars_per_pixel");
	if (ival >= 0) {
		hxpm->chrperpel = ival;
	} else {
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}
	
	if (hxpm->chrperpel >= UINT_MAX) {
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}

	/* palette */
	hxpm->color_table = PL_NewHashTable(hxpm->colorcount, PL_HashString, PL_CompareStrings, compare_uint32, NULL, NULL);
		
	pattern = PR_smprintf(PATTERN_TEMPLATE_HPALETTE, hxpm->sparam);
    if (regcomp(&h_regex, pattern, REG_EXTENDED)) {
        hxpm->stage = IRCK_XPM_STAGE_ERROR;
        return;
    }
    PR_smprintf_free(pattern);

    if (regexec(&h_regex, input, 1, h_matches, 0)) {
        regfree(&h_regex);
        hxpm->stage = IRCK_XPM_STAGE_ERROR;
        return;
    }

    cursor = input + h_matches[0].rm_eo;
    regfree(&h_regex);

    if (regcomp(&regex, "\"([^\"]+)\"[[:space:],]*\"([^\"]+)\"", REG_EXTENDED)) {
        hxpm->stage = IRCK_XPM_STAGE_ERROR;
        return;
    }
	
    while (!regexec(&regex, cursor, 3, matches, 0)) {
        char *key, *value;
        regoff_t k_start;
        regoff_t k_end;
        regoff_t v_start;
        regoff_t v_end;
        size_t k_len;
        size_t v_len;
        regoff_t i;
        PRBool stop_detected, added;

		added = PR_FALSE;
        stop_detected = PR_FALSE;
        for (i = 0; i < matches[0].rm_so; i++) {
            if (cursor[i] == '}') {
                stop_detected = PR_TRUE;
                break;
            }
        }
        
        if (stop_detected) {
            break;
        }
        
        k_start = matches[1].rm_so;
        k_end = matches[1].rm_eo;
        k_len = k_end - k_start;

        key = malloc(k_len + 1);
        if (key) {
            strncpy(key, cursor + k_start, k_len);
            key[k_len] = '\0';
        }

        v_start = matches[2].rm_so;
        v_end = matches[2].rm_eo;
        v_len = v_end - v_start;

        value = malloc(v_len + 1);
        if (value) {
            strncpy(value, cursor + v_start, v_len);
            value[v_len] = '\0';
        }

        if (key && value) {
			PRUint32 color_best, *table_val;
			
			color_best = 0;
			if (value[0] == '#' && count_valid_chars(value) == 13) {
				PRUint32 r, g, b;
						
				if (sscanf(value, "#%4x%4x%4x", &r, &g, &b) == 3) {							
					r = (r & 0xFFFF) / 257;
					g = (g & 0xFFFF) / 257;
					b = (b & 0xFFFF) / 257;
					color_best = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_USE_LIBC_FREE, r, g, b);
				} else {
					hxpm->stage = IRCK_XPM_STAGE_ERROR;
					return;
				}
			} else if (value[0] == '#' && count_valid_chars(value) == 7) {
				unsigned int r, g, b;
						
				if (sscanf(value, "#%02x%02x%02x", &r, &g, &b) == 3) {							
					color_best = IRCK_XPM_PACK_RGB(IRCK_XPM_COLOR_INFO_RGB_USE_LIBC_FREE, r, g, b);
				} else {
					hxpm->stage = IRCK_XPM_STAGE_ERROR;
					return;
				}
			} else if (value[0] == '#') {
				hxpm->stage = IRCK_XPM_STAGE_ERROR;
				return;
			} else {
				color_best = parse_named_color(value);
			}
			
			table_val = PR_NEW(PRUint32); 
			*table_val = color_best;
			if (PL_HashTableAdd(hxpm->color_table, key, table_val)) {
				added = PR_TRUE;
			}
        }

		if (!added) {
			free(key);
		}
        free(value);
        cursor += matches[0].rm_eo;
    }

    regfree(&regex);
	
	/* pixels */
	if (hxpm->w > 0 && hxpm->h > 0 && hxpm->w > (UINT_MAX / hxpm->h)) {
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
 		return;
	}

	total_pixels = hxpm->w * hxpm->h;
	if (total_pixels > (UINT_MAX / sizeof(PRUint32))) {
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}

	if (hxpm->w > (UINT_MAX / hxpm->h / sizeof(PRUint32))) {
		hxpm->stage = IRCK_XPM_STAGE_ERROR; 
		return;
	}

	hxpm->argb8888 = calloc(total_pixels, sizeof(PRUint32));
	if (!hxpm->argb8888) {
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}

	hxpm->color_table_key = calloc(hxpm->chrperpel + 1, sizeof(char));
	if (!hxpm->color_table_key) {
		free(hxpm->argb8888);
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}

	hxpm->iparam = 0; 	
	pattern = PR_smprintf(PATTERN_TEMPLATE_HPIXELS, hxpm->sparam);
	if (regcomp(&h_regex, pattern, REG_EXTENDED)) {
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}
	PR_smprintf_free(pattern);

	if (regexec(&h_regex, input, 1, h_matches, 0)) {
		regfree(&h_regex);
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}

	cursor = input + h_matches[0].rm_eo;
	regfree(&h_regex);

	if (regcomp(&regex, "\"([^\"]+)\"", REG_EXTENDED)) {
		hxpm->stage = IRCK_XPM_STAGE_ERROR;
		return;
	}

	while (!regexec(&regex, cursor, 2, matches, 0)) {
		char *pixel_line;
		regoff_t p_start;
		regoff_t p_end;
		size_t p_len;
		regoff_t i;
		PRBool stop_detected;

		stop_detected = PR_FALSE;
		for (i = 0; i < matches[0].rm_so; i++) {
			if (cursor[i] == '}') {
				stop_detected = PR_TRUE;
				break;
			}
		}
		
		if (stop_detected) {
			break;
		}
		
		p_start = matches[1].rm_so;
		p_end = matches[1].rm_eo;
		p_len = p_end - p_start;

		pixel_line = malloc(p_len + 1);
		if (pixel_line) {
			char *line_ptr;
			unsigned int line_len;
            int i;

			line_len = p_len;
            strncpy(pixel_line, cursor + p_start, p_len);
            pixel_line[p_len] = '\0';
            line_ptr = pixel_line;

            if (hxpm->iparam >= hxpm->h) {
                hxpm->stage = IRCK_XPM_STAGE_ERROR;
                free(pixel_line);
                free(hxpm->color_table_key);
                regfree(&regex);
                return;
            }

            if (strlen(line_ptr) < (hxpm->w * hxpm->chrperpel)) {
                hxpm->stage = IRCK_XPM_STAGE_ERROR;
                free(pixel_line);
                free(hxpm->color_table_key);
                regfree(&regex);
                return;
            }

            for (i = 0; i < hxpm->w; i++) {
                PRUint32 *valp;
              
                if (!line_len || line_len < hxpm->chrperpel) {
                    hxpm->argb8888[hxpm->iparam * hxpm->w + i] = IRCK_XPM_PACK_RGB(255, 0, 0, 0);
                    continue;
                }
				
                strncpy(hxpm->color_table_key, line_ptr, hxpm->chrperpel);
                hxpm->color_table_key[hxpm->chrperpel] = '\0';
                line_ptr += hxpm->chrperpel;

                valp = (PRUint32*)PL_HashTableLookup(hxpm->color_table, hxpm->color_table_key);
                if (valp) {
                    PRUint32 val = *valp;
                    if (((val >> 24) & 0xFF) == IRCK_XPM_COLOR_INFO_TRANSPARENT) {
                        val = (val & 0x00FFFFFF);
                    } else {
                        val = (val & 0x00FFFFFF) | (255u << 24);
                    }
                    hxpm->argb8888[hxpm->iparam * hxpm->w + i] = val;
                } else {
                    hxpm->argb8888[hxpm->iparam * hxpm->w + i] = IRCK_XPM_PACK_RGB(255, 0, 0, 0);
                }
            }

            hxpm->iparam++;
            free(pixel_line);
        } else {
			hxpm->stage = IRCK_XPM_STAGE_ERROR;
			regfree(&regex);
			return;
		}

		cursor += matches[0].rm_eo;
	}

	if (hxpm->iparam == hxpm->h) {
		hxpm->stage = IRCK_XPM_STAGE_DONE;
	} else {
		hxpm->stage = IRCK_XPM_STAGE_ERROR; 
		free(hxpm->argb8888);
		hxpm->argb8888 = NULL;
	}
	free(hxpm->color_table_key);
	regfree(&regex);

	if (hxpm->stage != IRCK_XPM_STAGE_ERROR) {
		hxpm->stage = IRCK_XPM_STAGE_DONE;
	}
}

static RCKXPMImage *rck_xpm_load_from_string(char *contents) {
	RCKXPMImage *xpm;
	char *k, *kstate;
	
	if (!contents) {
		return NULL;
	}
	
	xpm = malloc(sizeof(RCKXPMImage));
	if (!xpm) {
		return NULL;
	}
	xpm->infomask = RCK_XPM_INFO_MASK_NONE;
	xpm->stage = IRCK_XPM_STAGE_NONE;
	xpm->iparam = 0;
	xpm->cparam = 0;
	
	xpm->color_table = NULL;
	xpm->color_table_key = NULL;
	xpm->argb8888 = NULL;
	xpm->sparam = NULL;
	xpm->image = NULL;
	
	if (rck_strprefix(contents, "/* XPM */")) {
		xpm->version = 3;
	} else if (rck_strprefix(contents, "! XPM2")) {
		xpm->version = 2;
	} else {
		if (xpm1_check(contents, &xpm->sparam) == 1) {
			xpm->version = 1;
		} else {
			free(xpm);
			return NULL;
		}
	}
	xpm->infomask |= RCK_XPM_INFO_MASK_VERSION;

	kstate = contents;
	if (xpm->version == 3) {
		regex_t c_regex;
		regmatch_t c_matches[3];

		if (regcomp(&c_regex, "\"(([^\"\\\\]|\\\\.)*)\"|(\\/\\/.*|\\/\\*([^*]|\\*+[^\\/*])*\\*+\\/)", REG_EXTENDED)) {
			xpm->stage = IRCK_XPM_STAGE_ERROR;
			return NULL;
		}

		while (!regexec(&c_regex, kstate, 3, c_matches, 0)) {
			if (c_matches[1].rm_so != -1) {
				char *clean_line;
				regoff_t start_off;
				regoff_t end_off;
				size_t match_len;

				start_off = c_matches[1].rm_so;
				end_off = c_matches[1].rm_eo;
				match_len = end_off - start_off;
				clean_line = malloc(match_len + 1);
				if (clean_line) {
					strncpy(clean_line, kstate + start_off, match_len);
					clean_line[match_len] = '\0';

					parse_xpm2_line(xpm, clean_line);
					free(clean_line);

					if (xpm->stage == IRCK_XPM_STAGE_DONE) {
						regfree(&c_regex);
						goto DONE;
					} else if (xpm->stage == IRCK_XPM_STAGE_ERROR) {
						break;
					}
				} else {
					xpm->stage = IRCK_XPM_STAGE_ERROR;
					break;
				}
			}

			kstate += c_matches[0].rm_eo;
		}

		regfree(&c_regex);
	} else if (xpm->version == 2) {
		while ((k = PL_strtok_r(kstate, "\n", &kstate))) {
			char *cpy;
			
			cpy = PL_strdup(k);
			parse_xpm2_line(xpm, cpy);
			PL_strfree(cpy);
			if (xpm->stage == IRCK_XPM_STAGE_DONE) {
				return xpm;
			} else if (xpm->stage == IRCK_XPM_STAGE_ERROR) {
				break;
			}
		}
	} else if (xpm->version == 1) {
		xpm1_parse(xpm, contents);
		free(xpm->sparam);
	} else {
		free(xpm);
		return NULL;
	}
	
DONE:
	if (xpm->stage == IRCK_XPM_STAGE_DONE) {
		xpm->image = pixman_image_create_bits_no_clear(PIXMAN_a8r8g8b8, xpm->w, xpm->h, xpm->argb8888, xpm->w * sizeof(PRUint32));
	} else {
		if (xpm->color_table) {
			PL_HashTableEnumerateEntries(xpm->color_table, cleanup_color_table, NULL);
			PL_HashTableDestroy(xpm->color_table);	
		}

		if (xpm->color_table_key) {
			free(xpm->color_table_key);
		}
		
		if (xpm->argb8888) {
			free(xpm->argb8888);
		}
		
		if (xpm->sparam) {
			free(xpm->sparam);
		}
	
		free(xpm);
		return NULL;
	}

	return xpm;
}

static RCKXPMInfoMask rck_xpm_query_info(RCKXPMImage *xpm, unsigned int *outWidth, unsigned int *outHeight, unsigned int *outHotX, unsigned int *outHotY, unsigned int *outVersion) {
	if (!xpm) {
		return RCK_XPM_INFO_MASK_NONE;
	}
	
	if (outWidth && (xpm->infomask & RCK_XPM_INFO_MASK_DIMENSIONS)) {
		*outWidth = xpm->w;
	}
	
	if (outHeight && (xpm->infomask & RCK_XPM_INFO_MASK_DIMENSIONS)) {
		*outHeight = xpm->h;
	}

	if (outHotX && (xpm->infomask & RCK_XPM_INFO_MASK_HOTSPOT)) {
		*outHotX = xpm->hotx;
	}

	if (outHotY && (xpm->infomask & RCK_XPM_INFO_MASK_HOTSPOT)) {
		*outHotY = xpm->hoty;
	}
	
	if (outVersion && (xpm->infomask & RCK_XPM_INFO_MASK_VERSION)) {
		*outVersion = xpm->version;
	}
	
	return xpm->infomask;
}

static void rck_xpm_destroy(RCKXPMImage *xpm) {
	if (!xpm) {
		return;
	}

	PL_HashTableEnumerateEntries(xpm->color_table, cleanup_color_table, NULL);
	PL_HashTableDestroy(xpm->color_table);
	free(xpm->argb8888);
	free(xpm);
}

static RCKXPMImage *rck_xpm_load_from_filename(char *filename) {
	RCKXPMImage *xpm;
	FILE *file;
	char *data;
	size_t data_sz;
	
	
	file = fopen(filename, "r");
	if (!file) {
		return NULL;
	}
		
	fseek(file, 0, SEEK_END);
	data_sz = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = malloc(data_sz + 1);
	if (!data) {
		fclose(file);
		return NULL;
	}
	fread(data, 1, data_sz, file);
	data[data_sz] = '\0';
	fclose(file);
	xpm = rck_xpm_load_from_string(data);
	free(data);
	return xpm;
}

void destroy_xpm(pixman_image_t *image, void *data) {
	rck_xpm_destroy((RCKXPMImage *)data);
}

pixman_image_t *rck_xpm_load(RCKXPMInputType load_mask, char *input, RCKXPMInfoMask *outInfoMask, unsigned int *outHotX, unsigned int *outHotY, unsigned int *outVersion) {
	RCKXPMImage *xpm;
	RCKXPMInfoMask mask;
	
	if (!input) {
		return NULL;
	}
	
	xpm = NULL;
	if (load_mask == RCK_XPM_INPUT_TYPE_FROM_FILENAME) {
		xpm = rck_xpm_load_from_filename(input);
	} else if (load_mask == RCK_XPM_INPUT_TYPE_FROM_STRING) {
		xpm = rck_xpm_load_from_string(input);
	}
	
	if (!xpm) {
		return NULL;
	}
	
	mask = rck_xpm_query_info(xpm, NULL, NULL, outHotX, outHotY, outVersion);
	if (outInfoMask) {
		*outInfoMask = mask & ~RCK_XPM_INFO_MASK_DIMENSIONS;
	}
	
	pixman_image_set_destroy_function(xpm->image, destroy_xpm, xpm);
	return xpm->image;
}
