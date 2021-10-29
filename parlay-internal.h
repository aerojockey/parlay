/* parlay-internal.h */

#ifndef PARLAY_INTENRAL_H
#define PARLAY_INTENRAL_H


/* -------- Section one: Constants -------- */

/* Invalid character constant */

#define INVALID_CHARACTER ((codepoint_t)-1)


/* -------- Section two: Types -------- */

/* A Unicode code point */

typedef unsigned codepoint_t;


/* A record of face filenames for a particular font */

typedef struct _FontRecord {
    const char* name;
    const char* normal_filename;
    const char* italic_filename;
    const char* bold_filename;
    const char* bold_italic_filename;
    struct _FontRecord* next;
} FontRecord;


/* Information about a layed-out glyph */

typedef struct {
    FTC_FaceID face_id;
    FT_UInt glyph_index;
    int is_sbit;
    int font_px;
    int line_height;
    int x;
    int y;
    int left;
    int top;
    int width;
    int height;
    int ascender;
    int advance;
    float text_color[4];
    int border_thickness;
    float border_color[4];
    int highlight;
    float highlight_color[4];
    int underline;
} ParlayGlyphPlan;


/* Information about a whole layout */

typedef struct {
    ParlayGlyphPlan* glyph_plans;
    size_t n_glyphs_cap;
    size_t n_glyphs;
    size_t first_glyph_of_current_line;
    size_t first_glyph_of_current_word;
    int glyph_x;
    int line_y_top;
    int height;
    int width;
    int x_image_offset;
    int y_image_offset;
    int any_borders;
    int any_highlights;
} ParlayLayout;


/* -------- Section three: Inline functions -------- */

static __inline int MAX(int a, int b) {
    if (a > b) {
        return a;
    }
    return b;
}

static __inline int MIN(int a, int b) {
    if (a < b) {
        return a;
    }
    return b;
}

#endif
