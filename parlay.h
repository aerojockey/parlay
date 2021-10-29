/* parlay.h */

#ifndef PARLAY_H
#define PARLAY_H


/* -------- Section one: Configuration -------- */

#ifndef PARLAY_USE_MINIXML
#define PARLAY_USE_MINIXML 1
#endif


/* -------- Section two: Constants -------- */

/* Font styles */

#define PARLAY_STYLE_NORMAL 0
#define PARLAY_STYLE_ITALIC 1
#define PARLAY_STYLE_BOLD 2
#define PARLAY_STYLE_BOLD_ITALIC 3

/* Paragraph alignment */

#define PARLAY_ALIGN_LEFT 0
#define PARLAY_ALIGN_CENTER 1
#define PARLAY_ALIGN_RIGHT 2

/* Cropping */

#define PARLAY_CROP_Y_MASK 255
#define PARLAY_CROP_Y_NATURAL 0
#define PARLAY_CROP_Y_TIGHT 1
#define PARLAY_CROP_Y_HEIGHT 2
#define PARLAY_CROP_Y_FAILSAFE 3

#define PARLAY_CROP_X_MASK (255<<8)
#define PARLAY_CROP_X_NATURAL (0<<8)
#define PARLAY_CROP_X_TIGHT (1<<8)
#define PARLAY_CROP_X_WIDTH (2<<8)
#define PARLAY_CROP_X_FAILSAFE (3<<8)

#define PARLAY_CROP_NATURAL (PARLAY_CROP_Y_NATURAL|PARLAY_CROP_X_NATURAL)
#define PARLAY_CROP_TIGHT (PARLAY_CROP_Y_TIGHT|PARLAY_CROP_X_TIGHT)
#define PARLAY_CROP_BOUNDS (PARLAY_CROP_Y_HEIGHT|PARLAY_CROP_X_WIDTH)
#define PARLAY_CROP_FAILSAFE (PARLAY_CROP_Y_FAILSAFE|PARLAY_CROP_X_FAILSAFE)

/* -------- Section three: Types -------- */

/* Text style information */

typedef struct {
    const char* font_name;
    int font_style;
    float font_size;
    float text_color[4];
    int border_thickness;
    float border_color[4];
    int highlight;
    float highlight_color[4];
    int underline;
    float font_scaler;
    
    /* etc */
    /* other means of selecting color, incl. procedural */
    /* different meaning of font_size: advance, ascender, etc. */

} ParlayStyle;


/* Layout and image directives */

typedef struct {
    int width;
    int text_alignment;
    float background_color[4];
    int collapse_whitespace;
    int cropping_strategy;

    /* line_spacing (single, double, etc.) */
    /* padding */
    /* height */
    /* vertical alignment */
    /* whether to extend image to a power of 2 */
    /* other means of selecting color, incl. procedural */
    /* existing background image + possible tiling */

} ParlayControl;


/* RGBA raw image type */

typedef struct {
    unsigned char* data;
    size_t width;
    size_t height;
    int x0;
    int y0;
} ParlayRGBARawImage;


/* -------- Section four: Function prototypes -------- */

int parlay_init(void);

int parlay_finalize(void);

int parlay_register_font(const char* font_name, const char* normal_filename, const char* italic_filename,
        const char* bold_filename, const char* bold_italic_filename);

int parlay_plain_text(const char* text, const ParlayStyle* style, const ParlayControl* ctl, ParlayRGBARawImage* image);

#if PARLAY_USE_MINIXML
int parlay_markup_text(const char* xml, const ParlayStyle* style, const ParlayControl* ctl, ParlayRGBARawImage* image);
#endif

int parlay_free_image_data(ParlayRGBARawImage* image);

#endif
