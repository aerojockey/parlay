/* parlay.c */

#define _CRT_SECURE_NO_WARNINGS

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H

#include "parlay.h"
#include "parlay-internal.h"

#if PARLAY_USE_MINIXML
#include <mxml.h>
#endif


//---------------------------------------------------------------------
// Section 1: Utility functions

static codepoint_t read_utf8_character(const char** bytes) {
    const unsigned char* p = (const unsigned char*)*bytes;
    codepoint_t c;
    if (*p == 0) {
        return 0;
    }
    if ((*p & 0x80) == 0) {
        c = *p++;
    } else if ((*p & 0xE0) == 0xC0) {
        c = (*p++ & 0x1F) << 6;
        if ((*p & 0xC0) != 0x80) {
            return INVALID_CHARACTER;
        }
        c |= (*p++ & 0x3F);
        if (c < 0x80) {
            return INVALID_CHARACTER;
        }
    } else if ((*p & 0xF0) == 0xE0) {
        c = (*p++ & 0x0F) << 12;
        if ((*p & 0xC0) != 0x80) {
            return INVALID_CHARACTER;
        }
        c |= (*p++ & 0x3F) << 6;
        if ((*p & 0xC0) != 0x80) {
            return INVALID_CHARACTER;
        }
        c |= (*p++ & 0x3F);
        if (c < 0x800 || (c >= 0xD800 && c <= 0xDFFF)) {
            return INVALID_CHARACTER;
        }
    } else if ((*p & 0xF8) == 0xF0) {
        c = (*p++ & 0x07) << 18;
        if ((*p & 0xC0) != 0x80) {
            return INVALID_CHARACTER;
        }
        c |= (*p++ & 0x3F) << 12;
        if ((*p & 0xC0) != 0x80) {
            return INVALID_CHARACTER;
        }
        c |= (*p++ & 0x3F) << 6;
        if ((*p & 0xC0) != 0x80) {
            return INVALID_CHARACTER;
        }
        c |= (*p++ & 0x3F);
        if (c < 0x10000) {
            return INVALID_CHARACTER;
        }
    } else {
        return INVALID_CHARACTER;
    }
    *bytes = (const char*)p;
    return c;
}


static int is_word_break(codepoint_t c) {
    switch (c) {
    case '\t':
    case ' ':
    case 0x2000:
    case 0x2001:
    case 0x2002:
    case 0x2003:
    case 0x2004:
    case 0x2005:
    case 0x2006:
    case 0x2008:
    case 0x2009:
    case 0x200B:
    case 0x200C:
    case 0x200D:
        return 1;
    }
    return 0;
}


static int is_collapsable_whitespace(codepoint_t c) {
    switch (c) {
    case '\t':
    case '\n':
    case '\r':
    case ' ':
        return 1;
    }
    return 0;
}


static int is_line_break(codepoint_t c) {
    switch (c) {
    case '\n':
    case 0x2028:
    case 0x2029:
        return 1;
    }
    return 0;
}


static int get_hex02_value(const char* hex) {
    int i, rv;
    rv = 0;
    for (i = 0; ; i++) {
        if (*hex >= '0' && *hex <= '9') {
            rv += (*hex - '0');
        } else if (*hex >= 'A' && *hex <= 'F') {
            rv += (*hex - 'A' + 10);
        } else if (*hex >= 'a' && *hex <= 'f') {
            rv += (*hex - 'a' + 10);
        } else {
            return -1;
        }
        if (i == 1) {
            return rv;
        }
        hex++;
        rv *= 16;
    }
}


//---------------------------------------------------------------------
// Section 2: Font management functions

// I am ashamed of using a linked-list for storing registered fonts, but when you
// have about ten fonts and they're heavily front-loaded, there's no reason to
// make this any more complex.  (Just make sure to register the most common fonts
// first.)

static FontRecord* font_list;
static FontRecord* font_last;

int parlay_register_font(const char* font_name, const char* normal_filename, const char* italic_filename,
        const char* bold_filename, const char* bold_italic_filename) {
    size_t alloc_size;
    FontRecord* font_rec;
    char* p;
    int status = 9999;
    alloc_size = 5 + sizeof(FontRecord) + strlen(font_name);
    if (normal_filename != NULL) {
        alloc_size += strlen(normal_filename);
    }
    if (italic_filename != NULL) {
        alloc_size += strlen(italic_filename);
    }
    if (bold_filename != NULL) {
        alloc_size += strlen(bold_filename);
    }
    if (bold_italic_filename != NULL) {
        alloc_size += strlen(bold_italic_filename);
    }
    font_rec = malloc(alloc_size);
    if (font_rec == NULL) {
        status = 301;
        goto error;
    }
    p = (char*)&font_rec[1];
    font_rec->name = p;
    strcpy(p,font_name);
    p += strlen(p)+1;
    if (normal_filename) {
        font_rec->normal_filename = p;
        strcpy(p,normal_filename);
        p += strlen(p)+1;
    } else {
        font_rec->normal_filename = NULL;
    }
    if (italic_filename) {
        font_rec->italic_filename = p;
        strcpy(p,italic_filename);
        p += strlen(p)+1;
    } else {
        font_rec->italic_filename = NULL;
    }
    if (bold_filename) {
        font_rec->bold_filename = p;
        strcpy(p,bold_filename);
        p += strlen(p)+1;
    } else {
        font_rec->bold_filename = NULL;
    }
    if (bold_italic_filename) {
        font_rec->bold_italic_filename = p;
        strcpy(p,bold_italic_filename);
    } else {
        font_rec->bold_italic_filename = NULL;
    }
    font_rec->next = NULL;
    if (font_list == NULL) {
        font_list = font_rec;
    } else {
        font_last->next = font_rec;
    }
    font_last = font_rec;
    status = 0;
error:
    return status;
}


static FTC_FaceID lookup_face_id(const char* font_name, int font_style) {
    FontRecord* font_rec;
    for (font_rec = font_list; font_rec != NULL; font_rec = font_rec->next) {
        if (!strcmp(font_name,font_rec->name)) {
            switch (font_style) {
            case PARLAY_STYLE_NORMAL:
                return (FTC_FaceID)font_rec->normal_filename;
            case PARLAY_STYLE_ITALIC:
                return (FTC_FaceID)font_rec->italic_filename;
            case PARLAY_STYLE_BOLD:
                return (FTC_FaceID)font_rec->bold_filename;
            case PARLAY_STYLE_BOLD_ITALIC:
                return (FTC_FaceID)font_rec->bold_italic_filename;
            default:
                return NULL;
            }
        }
    }
    return NULL;
}


static FT_Error load_face_callback(FTC_FaceID face_id, FT_Library library, FT_Pointer request_data,
        FT_Face* rface) {
    return FT_New_Face(library,(char*)face_id,0,rface);
}


//---------------------------------------------------------------------
// Section 3: Layout functions

static FT_Library library;
static FTC_Manager manager;
static FTC_CMapCache cmap_cache;
static FTC_SBitCache sbit_cache;
static FTC_ImageCache image_cache;


static int new_layout(size_t n_glyphs_cap, ParlayLayout** rlayout) {
    ParlayLayout* layout = NULL;
    ParlayGlyphPlan* glyph_plans = NULL;
    int status = 9999;

    layout = (ParlayLayout*)malloc(sizeof(ParlayLayout));
    if (layout == NULL) {
        status = 1001;
        goto error;
    }

    glyph_plans = (ParlayGlyphPlan*)malloc(sizeof(ParlayGlyphPlan)*n_glyphs_cap);
    if (glyph_plans == NULL) {
        status = 1002;
        goto error;
    }

    layout->glyph_plans = glyph_plans;
    layout->n_glyphs_cap = n_glyphs_cap;
    layout->n_glyphs = 0;
    layout->first_glyph_of_current_word = 0;
    layout->first_glyph_of_current_line = 0;
    layout->glyph_x = 0;
    layout->line_y_top = 0;
    layout->height = -1;
    layout->width = -1;
    layout->x_image_offset = -9999;
    layout->y_image_offset = -9999;
    layout->any_borders = 0;
    layout->any_highlights = 0;

    *rlayout = layout;
    layout = NULL;
    glyph_plans = NULL;

    status = 0;

error:
    if (glyph_plans != NULL) {
        free(glyph_plans);
    }
    if (layout != NULL) {
        free(layout);
    }
    return status;
}


static int increase_layout_glyph_capacity(ParlayLayout* layout) {
    ParlayGlyphPlan* glyph_plans = NULL;
    size_t n_glyphs_cap;
    int status = 9999;

    n_glyphs_cap = layout->n_glyphs_cap*2;
    if (n_glyphs_cap < 10) {
        n_glyphs_cap = 10;
    }

    glyph_plans = (ParlayGlyphPlan*)malloc(sizeof(ParlayGlyphPlan)*n_glyphs_cap);
    if (glyph_plans == NULL) {
        status = 1101;
        goto error;
    }

    if (layout->glyph_plans) {
        memcpy(glyph_plans,layout->glyph_plans,layout->n_glyphs*sizeof(ParlayGlyphPlan));
        free(layout->glyph_plans);
    }

    layout->glyph_plans = glyph_plans;
    layout->n_glyphs_cap = n_glyphs_cap;

    glyph_plans = NULL;

    status = 0;

error:
    if (glyph_plans == NULL) {
        free(glyph_plans);
    }
    return status;
}


static void lay_out_line(ParlayLayout* layout, int empty_line_height, int empty_line_ascender) {
    size_t i;
    int this_line_y, this_line_ascender, this_line_descender, this_line_height;
    this_line_ascender = empty_line_ascender;
    this_line_descender = empty_line_height - empty_line_ascender;
    for (i = layout->first_glyph_of_current_line; i < layout->n_glyphs; i++) {
        int glyph_ascender = layout->glyph_plans[i].ascender;
        int glyph_descender = layout->glyph_plans[i].line_height - glyph_ascender;
        this_line_ascender = MAX(glyph_ascender,this_line_ascender);
        this_line_descender = MAX(glyph_descender,this_line_descender);
    }
    this_line_height = this_line_ascender + this_line_descender;
    this_line_y = layout->line_y_top - this_line_ascender;
    for (i = layout->first_glyph_of_current_line; i < layout->n_glyphs; i++) {
        layout->glyph_plans[i].y = this_line_y;
        layout->glyph_plans[i].line_height = this_line_height;
        layout->glyph_plans[i].ascender = this_line_ascender;
    }
    layout->first_glyph_of_current_line = layout->n_glyphs;
    layout->first_glyph_of_current_word = layout->n_glyphs;
    layout->line_y_top = this_line_y - this_line_descender;
    layout->glyph_x = 0;
}


static void lay_out_most_of_line(ParlayLayout* layout) {
    size_t i;
    int this_line_y, this_line_ascender, this_line_descender, this_line_height, this_line_width;
    if (layout->first_glyph_of_current_line == layout->first_glyph_of_current_word) {
        return;
    }
    this_line_ascender = 0;
    this_line_descender = 0;
    for (i = layout->first_glyph_of_current_line; i < layout->first_glyph_of_current_word; i++) {
        int glyph_ascender = layout->glyph_plans[i].ascender;
        int glyph_descender = layout->glyph_plans[i].line_height - glyph_ascender;
        this_line_ascender = MAX(glyph_ascender,this_line_ascender);
        this_line_descender = MAX(glyph_descender,this_line_descender);
    }
    this_line_height = this_line_ascender + this_line_descender;
    this_line_y = layout->line_y_top - this_line_ascender;
    for (i = layout->first_glyph_of_current_line; i < layout->first_glyph_of_current_word; i++) {
        layout->glyph_plans[i].y = this_line_y;
        layout->glyph_plans[i].line_height = this_line_height;
        layout->glyph_plans[i].ascender = this_line_ascender;
    }
    this_line_width = layout->glyph_plans[layout->first_glyph_of_current_word].x;
    for (i = layout->first_glyph_of_current_word; i < layout->n_glyphs; i++) {
        layout->glyph_plans[i].x -= this_line_width;
    }
    layout->first_glyph_of_current_line = layout->first_glyph_of_current_word;
    layout->line_y_top = this_line_y - this_line_descender;
    layout->glyph_x -= this_line_width;
}


static int add_text_to_layout(ParlayLayout* layout, const char** text_handle,
        const ParlayStyle* style, int wrap_width, int collapse_whitespace, size_t max_characters) {

    FTC_FaceID face_id;
    FT_Face face;
    FTC_ScalerRec face_size_info;
    FT_Size size;
    int font_px, line_height, ascender;
    int prev_was_whitespace;
    int c_is_sbit, c_xadvance, c_width, c_height, c_left, c_top;
    codepoint_t c;
    FTC_SBit sbit;
    FT_BitmapGlyph glyph;
    FT_UInt glyph_index;
    //FT_UInt prev_glyph_index;
    //FT_Vector kerning;
    ParlayGlyphPlan* gp;
    int status = 9999;
    size_t ichr;

    face_id = lookup_face_id(style->font_name,style->font_style);
    if (face_id == NULL) {
        status = 1201;
        goto error;
    }

    status = FTC_Manager_LookupFace(manager,face_id,&face);
    if (status) {
        status = 1202;
        goto error;
    }

    font_px = (int)ceil(style->font_size * style->font_scaler);

    face_size_info.face_id = face_id;
    face_size_info.width = font_px;
    face_size_info.height = font_px;
    face_size_info.pixel = 1;
    face_size_info.x_res = 0;
    face_size_info.y_res = 0;

    status = FTC_Manager_LookupSize(manager,&face_size_info,&size);
    if (status) {
        status = 1203;
        goto error;
    }

    line_height = (int)((float)face->height * size->metrics.x_ppem / face->units_per_EM + 0.5);
    ascender = (int)((float)face->ascender * size->metrics.x_ppem / face->units_per_EM + 0.5);

    //prev_glyph_index = 0;
    prev_was_whitespace = 0;

    for (ichr = 0; ichr < max_characters; ichr++) {
        c = read_utf8_character(text_handle);
        if (c == INVALID_CHARACTER) {
            status = 1205;
            goto error;
        }
        if (c == 0) {
            break;
        }
        if (collapse_whitespace) {
            if (is_collapsable_whitespace(c)) {
                if (prev_was_whitespace || layout->glyph_x == 0) {
                    continue;
                }
                c = ' ';
                prev_was_whitespace = 1;
            } else {
                prev_was_whitespace = 0;
            }
        } else if (is_line_break(c)) {
            lay_out_line(layout,line_height,ascender);
            //prev_glyph_index = 0;
            continue;
        }
        glyph_index = FTC_CMapCache_Lookup(cmap_cache,face_id,0,c);
        if (glyph_index == 0) {
            glyph_index = FTC_CMapCache_Lookup(cmap_cache,face_id,0,'?');
        }
        status = FTC_SBitCache_LookupScaler(sbit_cache,&face_size_info,FT_LOAD_RENDER,glyph_index,&sbit,NULL);
        if (status) {
            status = 1207;
            goto error;
        }
        if (sbit->xadvance != 0 || sbit->height != 0) {
            c_is_sbit = 1;
            c_xadvance = sbit->xadvance;
            c_width = sbit->width;
            c_height = sbit->height;
            c_left = sbit->left;
            c_top = sbit->top;
        } else {
            status = FTC_ImageCache_LookupScaler(image_cache,&face_size_info,FT_LOAD_RENDER,glyph_index,(FT_Glyph*)&glyph,NULL);
            if (status) {
                status = 1208;
                goto error;
            }
            c_is_sbit = 0;
            c_xadvance = glyph->root.advance.x >> 16;
            c_width = glyph->bitmap.width;
            c_height = glyph->bitmap.rows;
            c_left = glyph->left;
            c_top = glyph->top;
        }
        if (layout->n_glyphs >= layout->n_glyphs_cap) {
            status = increase_layout_glyph_capacity(layout);
            if (status) {
                status = 1209;
                goto error;
            }
        }
        gp = &layout->glyph_plans[layout->n_glyphs];
        gp->is_sbit = c_is_sbit;
        gp->line_height = line_height;
        gp->x = layout->glyph_x;
        gp->y = 0;
        gp->ascender = ascender;
        gp->advance = c_xadvance;
        gp->highlight = style->highlight;
        if (gp->highlight) {
            layout->any_highlights = 1;
            memcpy(gp->highlight_color,style->highlight_color,4*sizeof(float));
        }
        if (c_height != 0) {
            // if (prev_glyph_index != 0) {
            //    status = FT_Get_Kerning(face,glyph_index,prev_glyph_index,FT_KERNING_DEFAULT,&kerning);
            //    if (status) {
            //        status = 1208;
            //        goto error;
            //    }
            //    layout->next_glyph_x += kerning.x>>6;
            // }
            gp->face_id = face_id;
            gp->font_px = font_px;
            gp->glyph_index = glyph_index;
            gp->left = c_left;
            gp->width = c_width;
            gp->top = c_top;
            gp->height = c_height;
            memcpy(gp->text_color,style->text_color,4*sizeof(float));
            gp->border_thickness = style->border_thickness;
            if (style->border_thickness) {
                memcpy(gp->border_color,style->border_color,4*sizeof(float));
                layout->any_borders = 1;
            }
        } else {
            gp->face_id = NULL;
            gp->glyph_index = 0;
            // The rest shouldn't be needed, here as failsafe
            gp->left = 0;
            gp->width = 0;
            gp->top = 0;
            gp->height = 0;
            gp->border_thickness = 0;
        }
        layout->n_glyphs++;
        layout->glyph_x += gp->advance;
        if (is_word_break(c)) {
            layout->first_glyph_of_current_word = layout->n_glyphs;
        } else {
            if (wrap_width > 0 && gp->x + gp->left + gp->width + gp->border_thickness > wrap_width) {
                lay_out_most_of_line(layout);
            }
        }
        //prev_glyph_index = glyph_index;
    }
    status = 0;

error:
    return status;
}


static void get_x_glyph_bounds(ParlayLayout* layout, int* pleft, int* pright) {
    int i, left, right;
    if (layout->n_glyphs == 0) {
        *pleft = *pright = 0;
        return;
    }
    left = INT_MAX;
    right = INT_MIN;
    for (i = 0; i < layout->n_glyphs; i++) {
        ParlayGlyphPlan* gp = &layout->glyph_plans[i];
        if (gp->glyph_index != 0) {
            left = MIN(left,gp->x+gp->left-gp->border_thickness);
            right = MAX(right,gp->x+gp->left+gp->width+gp->border_thickness);
        }
    }
    *pleft = left;
    *pright = right;
}


static void get_y_glyph_bounds(ParlayLayout* layout, int* pbottom, int* ptop) {
    int i, bottom, top;
    if (layout->n_glyphs == 0) {
        *pbottom = *ptop = 0;
        return;
    }
    bottom = INT_MAX;
    top = INT_MIN;
    for (i = 0; i < layout->n_glyphs; i++) {
        ParlayGlyphPlan* gp = &layout->glyph_plans[i];
        if (gp->glyph_index != 0) {
            top = MAX(top,gp->y+gp->top+gp->border_thickness);
            bottom = MIN(bottom,gp->y+gp->top+gp->height-gp->border_thickness);
        }
    }
    *pbottom = bottom;
    *ptop = top;
}


static int finalize_layout(ParlayLayout* layout, int cropping_strategy, int fixed_width) {
    int top, bottom, left, right;
    ParlayGlyphPlan* gp;
    size_t k;

    if (layout->first_glyph_of_current_line != layout->n_glyphs) {
        lay_out_line(layout,0,0);
    }

    switch (cropping_strategy & PARLAY_CROP_X_MASK) {
    case PARLAY_CROP_X_NATURAL:
        left = 0;
        right = 0;
        for (k = 0; k < layout->n_glyphs; k++) {
            gp = &layout->glyph_plans[k];
            right = MAX(right,gp->x+gp->advance);
        }
        break;

    case PARLAY_CROP_X_TIGHT:
        get_x_glyph_bounds(layout,&left,&right);
        break;

    case PARLAY_CROP_X_WIDTH:
        if (fixed_width == 0) {
            return 1403;
        }
        get_x_glyph_bounds(layout,&left,&right);
        right = left + fixed_width;
        break;

    case PARLAY_CROP_X_FAILSAFE:
        get_x_glyph_bounds(layout,&left,&right);
        left = MIN(left,0);
        for (k = 0; k < layout->n_glyphs; k++) {
            gp = &layout->glyph_plans[k];
            right = MAX(right,gp->x+gp->advance);
        }
        break;

    default:
        return 1401;
    }

    switch (cropping_strategy & PARLAY_CROP_Y_MASK) {
    case PARLAY_CROP_Y_NATURAL:
        top = 0;
        if (layout->n_glyphs > 0) {
            gp = &layout->glyph_plans[layout->n_glyphs-1];
            bottom = gp->y+gp->ascender-gp->line_height;
        } else {
            bottom = 0;
        }
        break;

    case PARLAY_CROP_Y_TIGHT:
        get_y_glyph_bounds(layout,&bottom,&top);
        break;

    case PARLAY_CROP_Y_HEIGHT:
        return 1404;

    case PARLAY_CROP_Y_FAILSAFE:
        get_y_glyph_bounds(layout,&bottom,&top);
        top = MAX(top,0);
        if (layout->n_glyphs > 0) {
            gp = &layout->glyph_plans[layout->n_glyphs-1];
            bottom = MIN(bottom,gp->y+gp->ascender-gp->line_height);
        }
        break;

    default:
        return 1402;
    }

    layout->height = top - bottom;
    layout->y_image_offset = top;
    layout->width = right - left;
    layout->x_image_offset = left;

    layout->first_glyph_of_current_word = (size_t)(-1);
    layout->first_glyph_of_current_line = (size_t)(-1);
    layout->glyph_x = -1;
    layout->line_y_top = -1;

    return 0;
}


static void delete_layout(ParlayLayout* layout) {
    if (layout != NULL) {
        if (layout->glyph_plans != NULL) {
            free(layout->glyph_plans);
        }
        free(layout);
    }
}


static int realign(ParlayLayout* layout, int text_alignment) {
    size_t i, j, first_glyph, last_glyph;
    int line_y, shift;

    switch (text_alignment) {
    case PARLAY_ALIGN_LEFT:
        // do nothing
        break;

    case PARLAY_ALIGN_CENTER:
    case PARLAY_ALIGN_RIGHT:
        i = 0;
        while (i < layout->n_glyphs) {
            first_glyph = i;
            line_y = layout->glyph_plans[i].y;
            last_glyph = i;
            i++;
            while (i < layout->n_glyphs && layout->glyph_plans[i].y == line_y) {
                if (layout->glyph_plans[i].face_id != NULL) {
                    last_glyph = i;
                }
                i++;
            }
            shift = layout->width - layout->glyph_plans[last_glyph].x - layout->glyph_plans[last_glyph].width;
            if (text_alignment == PARLAY_ALIGN_CENTER) {
                shift /= 2;
            }
            if (shift > 0) {
                for (j = first_glyph; j <= last_glyph; j++) {
                    layout->glyph_plans[j].x += shift;
                }
            }
        }
        break;

    default:
        return 1301;
    }

    return 0;
}


static void transfer_rect(ParlayLayout* layout, int x, int y, int width, int height, float rgb[3], float alpha, float* work) {
    int i, j, imax, jmax;
    size_t iw;
    float source_alpha, rem_alpha, total_alpha;

    imax = (int)MAX(0,MIN(width,layout->width-x));
    jmax = (int)MAX(0,MIN(height,layout->height-y));
    for (i = MAX(0,-x); i < imax; i++) {
        for (j = MAX(0,-y); j < jmax; j++) {
            iw = ((y+j) * layout->width + (x+i)) * 4;
            source_alpha = alpha;
            rem_alpha = (1-source_alpha)*work[iw+3];
            total_alpha = source_alpha + rem_alpha;
            source_alpha /= total_alpha;
            rem_alpha /= total_alpha;
            work[iw+0] = rgb[0]*source_alpha + work[iw+0]*rem_alpha;
            work[iw+1] = rgb[1]*source_alpha + work[iw+1]*rem_alpha;
            work[iw+2] = rgb[2]*source_alpha + work[iw+2]*rem_alpha;
            work[iw+3] = total_alpha;
        }
    }
}


static void transfer_buffer(ParlayLayout* layout, unsigned char* buffer, int x, int y, int width, int height, float rgb[3], float alpha, float* work) {
    int i, j, imax, jmax;
    size_t ig, iw;
    float source_alpha, rem_alpha, total_alpha;

    imax = (int)MAX(0,MIN(width,layout->width-x));
    jmax = (int)MAX(0,MIN(height,layout->height-y));
    for (i = MAX(0,-x); i < imax; i++) {
        for (j = MAX(0,-y); j < jmax; j++) {
            ig = j * width + i;
            if (buffer[ig] != 0) {
                iw = ((y+j) * layout->width + (x+i)) * 4;
                source_alpha = (buffer[ig]/255.0f) * alpha;
                rem_alpha = (1-source_alpha)*work[iw+3];
                total_alpha = source_alpha + rem_alpha;
                source_alpha /= total_alpha;
                rem_alpha /= total_alpha;
                work[iw+0] = rgb[0]*source_alpha + work[iw+0]*rem_alpha;
                work[iw+1] = rgb[1]*source_alpha + work[iw+1]*rem_alpha;
                work[iw+2] = rgb[2]*source_alpha + work[iw+2]*rem_alpha;
                work[iw+3] = total_alpha;
            }
        }
    }
}


static void smear_buffer(ParlayLayout* layout, unsigned char* buffer, int x, int y, int width, int height, float rgb[3], float alpha, int bt, float* work) {
    int i, j;
    float r;
    if (bt == 0) {
        return;
    }
    for (i = -bt; i <= bt; i++) {
        for (j = -bt; j <= bt; j++) {
            r = sqrt(i*i+j*j);
            if (r <= bt) {
                transfer_buffer(layout,buffer,x+i,y+j,width,height,rgb,alpha,work);
            } else if (r <= bt+1) {
                transfer_buffer(layout,buffer,x+i,y+j,width,height,rgb,alpha*(bt+1-r),work);
            }
        }
    }
}


static int rasterize(ParlayLayout* layout, const float background_color[4], ParlayRGBARawImage* image) {
    unsigned char* data = NULL;
    float* work = NULL;
    int x, y;
    size_t k, m;
    ParlayGlyphPlan* gp;
    FTC_ScalerRec face_size_info;
    FTC_SBit sbit;
    FT_BitmapGlyph glyph;
    unsigned char* c_buffer;
    int status = 9999;

    face_size_info.pixel = 1;
    face_size_info.x_res = 0;
    face_size_info.y_res = 0;

    data = malloc(layout->height*layout->width*4);
    if (data == NULL) {
        status = 1901;
        goto error;
    }

    work = malloc(layout->height * layout->width * 4 * sizeof(float));
    if (work == NULL) {
        status = 1902;
        goto error;
    }

    for (k = 0; k < (size_t)(layout->width * layout->height * 4); k++) {
        work[k] = background_color[k%4];
    }

    if (layout->any_highlights) {
        for (k = 0; k < layout->n_glyphs; k++) {
            gp = &layout->glyph_plans[k];
            if (!gp->highlight) {
                continue;
            }
            y = layout->y_image_offset - (gp->y + gp->ascender);
            x = gp->x - layout->x_image_offset;
            transfer_rect(layout,x,y,gp->advance,gp->line_height,gp->highlight_color,gp->highlight_color[3],work);
        }
    }

    for (m = layout->any_borders ? 0 : 1; m < 2; m++) {
        for (k = 0; k < layout->n_glyphs; k++) {
            gp = &layout->glyph_plans[k];
            if (gp->face_id == NULL) {
                continue;
            }
            face_size_info.face_id = gp->face_id;
            face_size_info.width = gp->font_px;
            face_size_info.height = gp->font_px;
            if (gp->is_sbit) {
                status = FTC_SBitCache_LookupScaler(sbit_cache,&face_size_info,FT_LOAD_RENDER,gp->glyph_index,&sbit,NULL);
                if (status) {
                    status = 1903;
                    goto error;
                }
                c_buffer = sbit->buffer;
            } else {
                status = FTC_ImageCache_LookupScaler(image_cache,&face_size_info,FT_LOAD_RENDER,gp->glyph_index,(FT_Glyph*)&glyph,NULL);
                if (status) {
                    status = 1903;
                    goto error;
                }
                c_buffer = glyph->bitmap.buffer;
            }
            y = layout->y_image_offset - (gp->y + gp->top);
            x = (gp->x + gp->left) - layout->x_image_offset;
            if (m == 0) {
                smear_buffer(layout,c_buffer,x,y,gp->width,gp->height,gp->border_color,gp->border_color[3],gp->border_thickness,work);
            } else {
                transfer_buffer(layout,c_buffer,x,y,gp->width,gp->height,gp->text_color,gp->text_color[3],work);
            }
        }
    }

    for (k = 0; k < (size_t)(layout->width * layout->height * 4); k++) {
        data[k] = (unsigned char)(work[k] * 255.0);
    }

    image->data = data;
    image->height = layout->height;
    image->width = layout->width;
    image->x0 = layout->x_image_offset;
    image->y0 = layout->y_image_offset;

    data = NULL;

    status = 0;

error:
    if (work != NULL) {
        free(work);
    }
    if (data != NULL) {
        free(data);
    }
    return status;
}



static int final_offset(ParlayLayout* layout, ParlayRGBARawImage* image, int fixed_width, int text_alignment) {
    if (fixed_width != 0) {
        if (text_alignment == PARLAY_ALIGN_CENTER) {
            image->x0 += (fixed_width - layout->width) / 2;
        } else if (text_alignment == PARLAY_ALIGN_RIGHT) {
            image->x0 += fixed_width - layout->width;
        }
    }
    return 0;
}




//---------------------------------------------------------------------
// Paragraph functions

int parlay_init(void) {
    int status;
    if (library == NULL) {
        status = FT_Init_FreeType(&library);
        if (status) {
            return 1;
        }
    }
    if (manager == NULL) {
        status = FTC_Manager_New(library,0,0,0,load_face_callback,NULL,&manager);
        if (status) {
            return 2;
        }
    }
    if (cmap_cache == NULL) {
        status = FTC_CMapCache_New(manager,&cmap_cache);
        if (status) {
            return 3;
        }
    }
    if (sbit_cache == NULL) {
        status = FTC_SBitCache_New(manager,&sbit_cache);
        if (status) {
            return 4;
        }
    }
    if (image_cache == NULL) {
        status = FTC_ImageCache_New(manager,&image_cache);
        if (status) {
            return 5;
        }
    }
    return 0;
}


int parlay_finalize(void) {
    int status;
    if (library != NULL) {
        status = FT_Done_FreeType(library);
        if (status) {
            return 9;
        }
        image_cache = NULL;
        sbit_cache = NULL;
        cmap_cache = NULL;
        manager = NULL;
        library = NULL;
    }
    return 0;
}


int parlay_plain_text(const char* text, const ParlayStyle* style, const ParlayControl* ctl, ParlayRGBARawImage* image) {
    ParlayLayout* layout = NULL;
    int status = 9999;

    status = new_layout(strlen(text),&layout);
    if (status) {
        goto error;
    }

    status = add_text_to_layout(layout,&text,style,ctl->width,ctl->collapse_whitespace,SIZE_MAX);
    if (status) {
        goto error;
    }

    status = finalize_layout(layout,ctl->cropping_strategy,ctl->width);
    if (status) {
        goto error;
    }

    status = realign(layout,ctl->text_alignment);
    if (status) {
        goto error;
    }

    status = rasterize(layout,ctl->background_color,image);
    if (status) {
        goto error;
    }

    status = final_offset(layout,image,ctl->width,ctl->text_alignment);
    if (status) {
        goto error;
    }
    status = 0;

error:
    if (layout != NULL) {
        delete_layout(layout);
    }

    return status;
}


#if PARLAY_USE_MINIXML

static int lay_out_element(ParlayLayout* layout, mxml_node_t* node,
        const ParlayStyle* parent_style, unsigned wrap_width, int is_top_element, int collapse_whitespace) {
    ParlayStyle style;
    const char* tag;
    const char* w;
    int i, c;
    float x;
    mxml_node_t* subnode;
    mxml_type_t subtype;
    int parse_style_attributes;
    int status = 9999;

    memcpy(&style,parent_style,sizeof(ParlayStyle));

    tag = mxmlGetElement(node);
    if (is_top_element) {
        if (strcmp(tag,"p")) {
            status = 301;
            goto error;
        }
        parse_style_attributes = 1;
    } else if (!strcmp(tag,"span")) {
        parse_style_attributes = 1;
    } else if (!strcmp(tag,"b")) {
        style.font_style |= PARLAY_STYLE_BOLD;
        parse_style_attributes = 0;
    } else if (!strcmp(tag,"i")) {
        style.font_style |= PARLAY_STYLE_ITALIC;
        parse_style_attributes = 0;
    } else {
        status = 302;
        goto error;
    }

    if (parse_style_attributes) {
        w = mxmlElementGetAttr(node,"font");
        if (w != NULL) {
            style.font_name = w;
        }
        w = mxmlElementGetAttr(node,"style");
        if (w != NULL) {
            if (!strcmp(w,"normal")) {
                style.font_style = PARLAY_STYLE_NORMAL;
            } else if (!strcmp(w,"italic")) {
                style.font_style = PARLAY_STYLE_ITALIC;
            } else if (!strcmp(w,"bold")) {
                style.font_style = PARLAY_STYLE_BOLD;
            } else if (!strcmp(w,"bold italic")) {
                style.font_style = PARLAY_STYLE_BOLD_ITALIC;
            } else {
                status = 303;
                goto error;
            }
        }
        w = mxmlElementGetAttr(node,"size");
        if (w != NULL) {
            x = atof(w);
            if (x <= 0) {
                status = 304;
                goto error;
            }
            style.font_size = x;
        }
        w = mxmlElementGetAttr(node,"color");
        if (w != NULL) {
            if (strlen(w) != 7 || w[0] != '#') {
                status = 305;
                goto error;
            }
            for (i = 0; i < 3; i++) {
                c = get_hex02_value(&w[1+2*i]);
                if (c == -1) {
                    status = 306;
                    goto error;
                }
                style.text_color[i] = c / 255.0;
            }
        }
        w = mxmlElementGetAttr(node,"border");
        if (w != NULL) {
            i = atoi(w);
            if (i < 0) {
                status = 307;
                goto error;
            }
            style.border_thickness = i;
        }
        w = mxmlElementGetAttr(node,"border_color");
        if (w != NULL) {
            if (strlen(w) != 7 || w[0] != '#') {
                status = 308;
                goto error;
            }
            for (i = 0; i < 3; i++) {
                c = get_hex02_value(&w[1+2*i]);
                if (c == -1) {
                    status = 309;
                    goto error;
                }
                style.border_color[i] = c / 255.0;
            }
        }
        w = mxmlElementGetAttr(node,"highlight_color");
        if (w != NULL) {
            if (strlen(w) != 7 || w[0] != '#') {
                status = 308;
                goto error;
            }
            for (i = 0; i < 3; i++) {
                c = get_hex02_value(&w[1+2*i]);
                if (c == -1) {
                    status = 309;
                    goto error;
                }
                style.highlight_color[i] = c / 255.0;
            }
            style.highlight = 1;
        }
        w = mxmlElementGetAttr(node,"visibility");
        if (w != NULL) {
            x = atof(w);
            if (x < 0 || x > 1) {
                status = 310;
                goto error;
            }
            style.text_color[3] = x;
            style.border_color[3] = x;
        }
    }

    subnode = mxmlGetFirstChild(node);
    while (subnode != NULL) {
        subtype = mxmlGetType(subnode);
        switch (subtype) {

        case MXML_ELEMENT:
            tag = mxmlGetElement(subnode);
            if (!strcmp(tag,"br")) {
                w = "\n";
                status = add_text_to_layout(layout,&w,&style,wrap_width,0,1);
            } else {
                status = lay_out_element(layout,subnode,&style,wrap_width,0,collapse_whitespace);
            }
            if (status) {
                goto error;
            }
            break;

        case MXML_OPAQUE:
            w = mxmlGetOpaque(subnode);
            status = add_text_to_layout(layout,&w,&style,wrap_width,collapse_whitespace,SIZE_MAX);
            if (status) {
                goto error;
            }
            break;

        default:
            status = 311;
            goto error;
        }

        subnode = mxmlGetNextSibling(subnode);
    }

    status = 0;

error:
    return status;
}


int parlay_markup_text(const char* xml, const ParlayStyle* style, const ParlayControl* ctl, ParlayRGBARawImage* image) {
    ParlayLayout* layout = NULL;
    mxml_node_t* top_node = NULL;
    const char* w;
    int text_alignment;
    int status = 9999;

    top_node = mxmlLoadString(NULL,xml,MXML_OPAQUE_CALLBACK);
    if (top_node == NULL) {
        status = 201;
        goto error;
    }

    status = new_layout(strlen(xml)*3/4,&layout);
    if (status) {
        goto error;
    }

    status = lay_out_element(layout,top_node,style,ctl->width,1,ctl->collapse_whitespace);
    if (status) {
        goto error;
    }

    status = finalize_layout(layout,ctl->cropping_strategy,ctl->width);
    if (status) {
        goto error;
    }

    w = mxmlElementGetAttr(top_node,"align");
    if (w == NULL) {
        text_alignment = ctl->text_alignment;
    } else if (!strcmp(w,"left")) {
        text_alignment = PARLAY_ALIGN_LEFT;
    } else if (!strcmp(w,"center")) {
        text_alignment = PARLAY_ALIGN_CENTER;
    } else if (!strcmp(w,"right")) {
        text_alignment = PARLAY_ALIGN_RIGHT;
    } else {
        status = 202;
        goto error;
    }

    status = realign(layout,text_alignment);
    if (status) {
        goto error;
    }

    status = rasterize(layout,ctl->background_color,image);
    if (status) {
        goto error;
    }

    status = final_offset(layout,image,ctl->width,text_alignment);
    if (status) {
        goto error;
    }
    status = 0;

error:
    if (layout != NULL) {
        delete_layout(layout);
    }
    if (top_node != NULL) {
        mxmlDelete(top_node);
    }

    return status;
}

#endif


int parlay_free_image_data(ParlayRGBARawImage* image) {
    if (image->data != NULL) {
        free(image->data);
        image->data = NULL;
    }
    image->width = 0;
    image->height = 0;
    image->x0 = 0;
    image->y0 = 0;
    return 0;
}
