------
PARLAY
------

Parlay is a C library for laying out out paragraphs to an in-memory
buffer.

This README is for version 0.3.


Summary
-------

Parlay does one thing.  It take some text (it could be plain text or a
simple markup language), lays that text out in a paragraph, and creates
an RGBA image of that paragraph in memory.

That's it.  That's all it does.  You give it paragraph text, you get
back an image buffer.

It's not something that exists only as part of some weird rendering
pipeline.  It is not built upon a massive foundational library.  It
doesn't have multiple backends.  It doesn't have plugins.  It's not
scriptable.  It's not part of a framework.  It stands alone.

The only input to Parlay is the text you want to render, some style
information, including the font to use, and a few layout options.  The
only output is an RGBA image buffer.

Its only dependencies are FreeType and, optionally, MiniXML.


Features
--------

* Lays out and renders text to an in-memory buffer
* Supports any True Type or Type 1 font that FreeType library supports
* Allows user to register font files (it doesn't rely on system fonts)
* Renders text in different styles like italic and bold, different font
  sizes, and different colors
* Supports outlines on characters
* Supports highlighting characters (i.e., as with a highlighting pen)
* Has basic layout control like maximum width and paragraph alignment
* Supports Unicode and the UTF-8 encoding
* Implements a simple XML-based markup language for specifying styles
* Does not have a lot of dependencies: just FreeType and optionally 
  MiniXML


Limitations
-----------

* Currently only supports left-to-right text
* Probably does not support combining characters, though I've never
  actually tried it
* Requires user to register font files (it doesn't use system fonts at all)
* Supports ONLY the UTF-8 encoding
* Does not yet support some basic styles like strikeout, double underline,
  superscript or subscript
* Does not yet support grayscale buffers, though of course you can render
  gray characters to an RGB buffer
* Does not currently support kerning


License
-------

Parlay is free to use and distribute, subject to a BSD-Style license.
See the file LICENSE.txt for details.

For binary distributions (i.e., if you distribute software that uses
Parlay) I consider a note saying that the software uses Parlay, with
a clickable link back to the official home page of Parlay, to satisfy
the second bullet point of license.

http://blog.aerojockey.com/post/parlay

Source distributions should include the license file and not rely on
a link.


Downloading
-----------

You can download Parlay from GitHub.

https://github.com/aerojockey/parlay


Building
--------

Parlay is written in portable C.  I think the only compatibility drama
might come from a pair of inline functions.  I don't expect you'll have
trouble building it on any system that FreeType supports.

Unless you're building libparlay.so for some kind of Linux distro, I
recommend you just add the source files to your project and compile it
in.  You need to build it with the FreeType library, version 2.  If you
want the simple markup language, you'll also need to build with MiniXML.

There is only one configuration option, PARLAY_USE_MINIXML, which
specifies whether to build the function parlay_markup_text, which
requires MiniXML.  You can modify this option at the top of parlay.h, or
define it on your compiler's command line.

I make no guarantees about thread safety at this point.  The functions
parlay_register_font and parlay_init are certainly not thread safe, but
you should probably be calling those during initialization anyway.  I
suspect that if you linked Parlay with a thread-safe version of
Freetype, then the functions parlay_plain_text and parlay_markup_text
would be thread-safe.  But it wasn't a design goal of mine to make them
thread-safe, so no guarantees.


Usage
-----

The API is very simple at this point: after initializating and
registering fonts, you call one function to get one rendered buffer.

(The underlying implementation allows for more exciting possibilities
like streaming text into a ParlayParagraphBuilder object, but the public
API currently doesn't offer this.)

Start by calling parlay_init.  This simply initializes some FreeType
objects.

Then, register some fonts with parlay_register_font.  Each font is given
a name and up to four font files (for regular, italic, bold, and
bold-italic).  The fonts can be TrueType or Type 1.

Once initialized, to lay out some text, you have to initialize two
structures and pass them to parlay_plain_text along with the text you
want to render.  The first structure is PaylayStyle, which contains
style information.  The second is ParlayControl, which has some options
for layout control.  You'll also create a structure, ParlayRGBARawImage,
to receive the image buffer output.

If you build it with MiniXML, you could call parlay_markup_text instead.


Example
-------

#include "parlay.h"

int parlay_hello(char* font_filename) {
    ParlayRGBARawImage image;
    ParlayStyle style;
    ParlayControl ctl;

    /* "Hello, world" isn't really long enough to show paragraph layout so we
       use a slightly longer message */

    const char* message = "Hello, my name is Inigo Montoya. You killed my father. Prepare to die.";

    /* Initialize Parlay */

    if (parlay_init() != 0) {
        /* error if parlay_init() returns nonzero */
        return -1;
    }

    /* Register the given font with the name "hello" */
    /* Arguments 3, 4, and 5 to parlay_register_font allow one to
       specify filenames for the italic, bold, and bold-italic
       styles, but to keep the example simple we'll not bother. */

    if (parlay_register_font("hello",font_filename,NULL,NULL,NULL) != 0) {
        /* error if parlay_register_font() returns nonzero */
        return -1;
    }

    /* Set up the style structure */

    style.font_name = "hello";  /* same name we registered the font as */
    style.font_size = 20;       /* size is roughly the number of pixels wide for lower-case m */
    style.font_scaler = 1.0;    /* a convenience to allow common scaling of fonts; just set to 1 */
    style.font_style = PARLAY_STYLE_NORMAL;  /* don't use bold or italic */
    style.text_color[0] = 0.0;  /* red component of text color, in range from 0 to 1 */
    style.text_color[1] = 0.0;  /* green component of text color */
    style.text_color[2] = 0.0;  /* blue component of text color */
    style.text_color[3] = 1.0;  /* alpha component of text color, 1 = fully opaque */
    style.border_thickness = 0; /* no border around the glyphs */
    style.highlight = 0;        /* no highlighting */
    style.underline = 0;        /* no underlining */

    /* Set up the control structure */

    ctl.width = 300;            /* number of pixels wide to wrap the paragraph--use 0 for no wrapping */
    ctl.text_alignment = PARLAY_ALIGN_CENTER; /* paragraph alignment */
    ctl.collapse_whitespace = 0; /* don't collapse whitespace--this option is mainly for markup */
    ctl.cropping_strategy = PARLAY_CROP_FAILSAFE; /* retain all rendered pixels */

    /* It's unnecessary but good practice to clear the image structure when not in use */

    image.data = NULL;    /* will be set to point at the malloc'ed image buffer */
    image.width = 0;      /* will be set to the width of the image buffer */
    image.height = 0;     /* will be set to the height of the image buffer */
    image.x0 = 0;         /* will be set to the recommended x-location to draw the buffer */
    image.y0 = 0;         /* will be set to the recommended y-location to draw the buffer */

    /* Now call parlay_plain_text */

    if (parlay_plain_text(message,&style,&ctl,&image) != 0) {
        /* error if parlay_register_font() returns nonzero */
        /* notice a pattern? */
        return -1;
    }

    /* The ParlayRGBARawImage structure has been filled with the image data */
    /* For this example just print out the width and height of it */
    /* A better example would show how the image buffer is used somehow */

    printf("parlay_plain_text produced an image of size %d x %d\n",image.width,image.height);

    /* We're done with image, free the image data. */

    parlay_free_image_data(&image);

    return 0;
}


What To Do With Your Shiny New Image Buffer
-------------------------------------------

I mean, it's kind of out of the scope of this README, but for those
skimming this file wondering, "Can Parlay do this?", here are some
examples of fun and cool things you can do with the image buffer output
of Parlay.

* You can use it to create OpenGL textures by passing it to
  glTexImage2D. (I assume you can do something similar with Direct X and
  Vulkan but I've never used them.)

* You can use an image format library, such as libpng or libjpeg, to save
  the buffer in a standard image format.

* You can use the image buffer to initialize a Pixmap object of some
  sort from a GUI Framework such as Qt or GTK, though if we're being
  fair, in most cases you'd just want to use the text capabilities built
  into the GUI Framework for that.

* You can do image operations (like compose, overlay, convolve, etc.)
  with other in-memory image buffers.

* You can load a cute picture of a kitten or a blobfish from the Internet
  into memory, and then use Parlay to add a funny or sarcastic caption
  to it, save that combined image to a file, and upload it to a
  social media website such as Twitter or Linkedin.

Some of these might need a little manipulation of the image data.  The
data format of the buffer is the obvious one.  Each pixel is four
unsigned chars, in order RGBA.  Each row is an array of pixels from left
to right. The whole image is a an array of rows from top to bottom.
Width and height are returned in the image structure.


History
-------

I wrote Parlay for a game I was working on, *The Ditty of Carmeana*.

Early on in development, I realized my crude way of rendering individual
letters from an alphabet texture didn't scale up well, and looked
terrible.  (Even by *Ditty of Carmeana* standards, which, if you decide
to look into the game, you will see are very, very low.)  I decided my
next step was to use real font rendering, and because I'm a modern,
savvy programmer who mindlessly heeds the woke advice to reuse code and
not reinvent the wheel, I went looking for a freeware library that could
do this for me.

Perhaps I didn't search the Internet well enough, but I could not find a
library that could simply lay out a paragraph of text into a memory
buffer.  The only applicable library I found was Pango.  Pango could do
what I wanted, it just could not do what I wanted *simply*.

The less I say about Pango the better.  (Put it this way: the two things
in the world I hate most are Pango and Adolf Hitler... in that order.)
I will only mention that the last straw was when it turned out to have a
complete inability to specify a application font.  All font selection
had to go through Font-Config, which manages system fonts, and this was
so baked into Pango it was like altering a law of nature to get it to
use an app font.

So I reinvented the wheel.

That's what this is.  When *The Ditty of Carmeana* was Greenlit on
Steam (so this is a while back), I decided it was worth my while to
write my own paragraph layout engine, that could do what I needed it to
do, lay out paragraphs to an in-memory image... *simply*.


Note on Workflow
----------------

I'm using GitHub mainly for distribution and visibility.  I'm not using
the Git workflow at all.

Currently, the official repository of Parlay is my private Subversion
repository for *The Ditty of Carmeana* (that's right, Subversion,
because that's how I roll, fight me), and I make my updates there. Then,
when I care to share my infrequent changes with the widespread community
of Parlay users, I manually copy the changes into my local cloned Git
repo using the Linux cp command, commit it and push it to master, with a
helpful log message like, "Pushed upstream changes from The Ditty of
Carmeana".

For now, if you want to contribute, the workflow is, "send me a patch
and/or verbal description of changes by email, and if I approve I'll
make the changes to my own repository use the above method to deploy".

Should this library grow and more people want to collaborate on it, we
will switch to the woke Git workflow, with detached heads and all that
useful stuff.  But for now I have no desire to implement a full-blown
workflow for this tiny project with three source files.


--Carl Banks