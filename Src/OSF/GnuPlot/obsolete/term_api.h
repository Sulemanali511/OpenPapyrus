/*
 * $Id: term_api.h,v 1.148 2016/04/01 16:19:41 sfeam Exp $
 */

/* GNUPLOT - term_api.h */

/*[
 * Copyright 1999, 2004   Thomas Williams, Colin Kelley
 *
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.
 *
 * Permission to modify the software is granted, but not the right to
 * distribute the complete modified source code.  Modifications are to
 * be distributed as patches to the released version.  Permission to
 * distribute binaries produced by compiling modified sources is granted,
 * provided you
 *   1. distribute the corresponding source modifications from the
 *    released version in the form of a patch file along with the binaries,
 *   2. add special version identification to distinguish your version
 *    in addition to the base release version number,
 *   3. provide your name and address as the primary contact for the
 *    support of your modified version, and
 *   4. retain our contact information in regard to use of the base
 *    software.
 * Permission to distribute the released version of the source code along
 * with corresponding source modifications in the form of a patch file is
 * granted with same provisions 2 through 4 for binary distributions.
 *
 * This software is provided "as is" without express or implied warranty
 * to the extent permitted by applicable law.
]*/

#ifndef GNUPLOT_TERM_API_H
# define GNUPLOT_TERM_API_H

/* #if... / #include / #define collection: */

//#include "syscfg.h"
#include "stdfn.h"
//#include "gp_types.h"
//#include "color.h"
#include "tables.h"
#ifdef OS2
# include "mousecmn.h"
#endif
//
// Constants that are interpreted by terminal driver routines
//
// Default line type is LT_BLACK; reset to this after changing colors
//
#define LT_AXIS       (-1)
#define LT_BLACK      (-2)		/* Base line type */
#define LT_SOLID      (-2)		/* Synonym for base line type */
#define LT_NODRAW     (-3)
#define LT_BACKGROUND (-4)
#define LT_UNDEFINED  (-5)
#define LT_COLORFROMCOLUMN  (-6)	/* Used by hidden3d code */
#define LT_DEFAULT    (-7)

/* Pre-defined dash types */
#define DASHTYPE_CUSTOM (-3)
#define DASHTYPE_AXIS   (-2)
#define DASHTYPE_SOLID  (-1)
/* more...? */

/* magic point type that indicates a character rather than a predefined symbol */
#define PT_CHARACTER  (-9)
/* magic point type that indicates true point type comes from a data column */
#define PT_VARIABLE   (-8)

/* Constant value passed to (term->text_angle)(ang) to generate vertical
 * text corresponding to old keyword "rotate", which produced the equivalent
 * of "rotate by 90 right-justified".
 */
#define TEXT_VERTICAL (-270)
//
// Type definitions 
//
// this order means we can use  x-(just*strlen(text)*t->h_char)/2 if term cannot justify
//
typedef enum JUSTIFY {
    LEFT,
    CENTRE,
    RIGHT
} JUSTIFY;
//
// we use a similar trick for vertical justification of multi-line labels
//
enum VERT_JUSTIFY {
    JUST_TOP,
    JUST_CENTRE,
    JUST_BOT
};

enum t_linecap {
    BUTT = 0,
    ROUNDED,
    SQUARE
};

/* custom dash pattern definition modeled after SVG terminal,
 * but string specifications like "--.. " are also allowed and stored */
#define DASHPATTERN_LENGTH 8

struct t_dashtype {
	float  pattern[DASHPATTERN_LENGTH];
	char   dstring[8];
};

#define DEFAULT_DASHPATTERN {{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, {0,0,0,0,0,0,0,0} }

lp_style_type {	/* contains all Line and Point properties */
	void    SetDefault()
	{
		// {0, LT_BACKGROUND, 0, DASHTYPE_SOLID, 0, 1.0, 0.0, DEFAULT_P_CHAR, BACKGROUND_COLORSPEC, DEFAULT_DASHPATTERN},
		flags = 0;
		l_type = LT_BACKGROUND;
		p_type = 0;
		d_type = DASHTYPE_SOLID;
		p_interval = 0;
		l_width = 1.0;
		p_size = 0.0;
		MEMSZERO(p_char);
		pm3d_color = BACKGROUND_COLORSPEC;
		custom_dash_pattern = DEFAULT_DASHPATTERN;
	}
    int     flags;		/* e.g. LP_SHOW_POINTS */
    int     l_type;
    int	    p_type;
    int     d_type;		/* Dashtype */
    int     p_interval;		/* Every Nth point in style LINESPOINTS */
    double  l_width;
    double  p_size;
    char    p_char[8];		/* string holding UTF-8 char used if p_type = PT_CHARACTER */
    t_colorspec pm3d_color;
    t_dashtype custom_dash_pattern;	/* per-line, user defined dashtype */
    /* ... more to come ? */
};

#define DEFAULT_P_CHAR {0,0,0,0,0,0,0,0}
#define DEFAULT_LP_STYLE_TYPE {0, LT_BLACK, 0, DASHTYPE_SOLID, 0, 1.0, PTSZ_DEFAULT, DEFAULT_P_CHAR, DEFAULT_COLORSPEC, DEFAULT_DASHPATTERN}

/* Bit definitions for lp_style_type.flags */
#define LP_SHOW_POINTS     (0x1) /* if not set, ignore the point properties of this line style */
#define LP_NOT_INITIALIZED (0x2) /* internal flag used in set.c:parse_label_options */
#define LP_EXPLICIT_COLOR  (0x4) /* set by lp_parse if the user provided a color spec */
#define LP_ERRORBAR_SET    (0x8) /* set by "set errorbars <lineprops> */

#define DEFAULT_COLOR_SEQUENCE { 0x9400d3, 0x009e73, 0x56b4e9, 0xe69f00, 0xf0e442, 0x0072b2, 0xe51e10, 0x000000 }
#define PODO_COLOR_SEQUENCE { 0x000000, 0xe69f00, 0x56b4e9, 0x009e73, 0xf0e442, 0x0072b2, 0xd55e00, 0xcc79a7 }

#define DEFAULT_MONO_LINETYPES { \
	{0, LT_BLACK, 0, DASHTYPE_SOLID, 0, 1.0 /*linewidth*/, PTSZ_DEFAULT, DEFAULT_P_CHAR, BLACK_COLORSPEC, DEFAULT_DASHPATTERN}, \
	{0, LT_BLACK, 0, 1 /* dt 2 */, 0, 1.0 /*linewidth*/, PTSZ_DEFAULT, DEFAULT_P_CHAR, BLACK_COLORSPEC, DEFAULT_DASHPATTERN}, \
	{0, LT_BLACK, 0, 2 /* dt 3 */, 0, 1.0 /*linewidth*/, PTSZ_DEFAULT, DEFAULT_P_CHAR, BLACK_COLORSPEC, DEFAULT_DASHPATTERN}, \
	{0, LT_BLACK, 0, 3 /* dt 4 */, 0, 1.0 /*linewidth*/, PTSZ_DEFAULT, DEFAULT_P_CHAR, BLACK_COLORSPEC, DEFAULT_DASHPATTERN}, \
	{0, LT_BLACK, 0, 0 /* dt 1 */, 0, 2.0 /*linewidth*/, PTSZ_DEFAULT, DEFAULT_P_CHAR, BLACK_COLORSPEC, DEFAULT_DASHPATTERN}, \
	{0, LT_BLACK, 0, DASHTYPE_CUSTOM, 0, 1.2 /*linewidth*/, PTSZ_DEFAULT, DEFAULT_P_CHAR, BLACK_COLORSPEC, {{16.,8.,2.,5.,2.,5.,2.,8.},{0,0,0,0,0,0,0,0}}} \
}

typedef enum e_arrow_head {
	NOHEAD = 0,
	END_HEAD = 1,
	BACKHEAD = 2,
	BOTH_HEADS = 3
} t_arrow_head;

extern const char *arrow_head_names[4];

typedef enum arrowheadfill {
	AS_NOFILL = 0,
	AS_EMPTY,
	AS_FILLED,
	AS_NOBORDER
} arrowheadfill;

arrow_style_type {    /* contains all Arrow properties */
    int tag;                         /* -1 (local), AS_VARIABLE, or style index */
    int layer;	                     /* 0 = back, 1 = front */
    lp_style_type lp_properties;
    /* head options */
    t_arrow_head head;               /* arrow head choice */
    /* t_position headsize; */  /* x = length, y = angle [deg] */
    double head_length;              /* length of head, 0 = default */
    int head_lengthunit;             /* unit (x1, x2, screen, graph) */
    double head_angle;               /* front angle / deg */
    double head_backangle;           /* back angle / deg */
    arrowheadfill headfill;	     /* AS_FILLED etc */
    bool head_fixedsize;         /* Adapt the head size for short arrow shafts? */
    /* ... more to come ? */
};

/* Operations used by the terminal entry point term->layer(). */
typedef enum termlayer {
	TERM_LAYER_RESET,
	TERM_LAYER_BACKTEXT,
	TERM_LAYER_FRONTTEXT,
	TERM_LAYER_BEGIN_BORDER,
	TERM_LAYER_END_BORDER,
	TERM_LAYER_BEGIN_GRID,
	TERM_LAYER_END_GRID,
	TERM_LAYER_END_TEXT,
	TERM_LAYER_BEFORE_PLOT,
	TERM_LAYER_AFTER_PLOT,
	TERM_LAYER_BEGIN_KEYSAMPLE,
	TERM_LAYER_END_KEYSAMPLE,
	TERM_LAYER_RESET_PLOTNO,
	TERM_LAYER_BEFORE_ZOOM,
	TERM_LAYER_BEGIN_PM3D_MAP,
	TERM_LAYER_END_PM3D_MAP,
	TERM_LAYER_BEGIN_IMAGE,
	TERM_LAYER_END_IMAGE,
	TERM_LAYER_3DPLOT
} t_termlayer;

/* Options used by the terminal entry point term->waitforinput(). */
#define TERM_ONLY_CHECK_MOUSING	1
#define TERM_EVENT_POLL_TIMEOUT 0	/* select() timeout in usec */

/* Options used by the terminal entry point term->hypertext(). */
#define TERM_HYPERTEXT_TOOLTIP 0
#define TERM_HYPERTEXT_TITLE   1

fill_style_type {
	fill_style_type()
	{
		SetDefault();
	}
	void   SetDefault()
	{
		fillstyle = FS_SOLID;
		filldensity = 100;
		fillpattern = 0;
		border_color = BLACK_COLORSPEC;
	}
    int    fillstyle;
    int    filldensity;
    int    fillpattern;
    t_colorspec border_color;
};

#ifdef EAM_BOXED_TEXT
/* Options used by the terminal entry point term->boxed_text() */
enum t_textbox_options {
	TEXTBOX_INIT = 0,
	TEXTBOX_OUTLINE,
	TEXTBOX_BACKGROUNDFILL,
	TEXTBOX_MARGINS,
	TEXTBOX_FINISH,
	TEXTBOX_GREY
};
#endif

enum t_fillstyle { 
	FS_EMPTY, 
	FS_SOLID, 
	FS_PATTERN, 
	FS_DEFAULT,
	FS_TRANSPARENT_SOLID, 
	FS_TRANSPARENT_PATTERN 
};

#define FS_OPAQUE (FS_SOLID + (100<<4))
//
// Color construction for an image, palette lookup or rgb components
//
enum t_imagecolor { 
	IC_PALETTE, 
	IC_RGB, 
	IC_RGBA 
};

/* Operations possible with term->modify_plots() */
#define MODPLOTS_SET_VISIBLE         (1<<0)
#define MODPLOTS_SET_INVISIBLE       (1<<1)
#define MODPLOTS_INVERT_VISIBILITIES (MODPLOTS_SET_VISIBLE|MODPLOTS_SET_INVISIBLE)

/* Values for the flags field of GpTermEntry
 */
#define TERM_CAN_MULTIPLOT    (1<<0)	/* tested if stdout not redirected */
#define TERM_CANNOT_MULTIPLOT (1<<1)	/* tested if stdout is redirected  */
#define TERM_BINARY           (1<<2)	/* open output file with "b"       */
#define TERM_INIT_ON_REPLOT   (1<<3)	/* call term->init() on replot     */
#define TERM_IS_POSTSCRIPT    (1<<4)	/* post, next, pslatex, etc        */
#define TERM_ENHANCED_TEXT    (1<<5)	/* enhanced text mode is enabled   */
#define TERM_NO_OUTPUTFILE    (1<<6)	/* terminal doesnt write to a file */
#define TERM_CAN_CLIP         (1<<7)	/* terminal does its own clipping  */
#define TERM_CAN_DASH         (1<<8)	/* terminal supports dashed lines  */
#define TERM_ALPHA_CHANNEL    (1<<9)	/* alpha channel transparency      */
#define TERM_MONOCHROME      (1<<10)	/* term is running in mono mode    */
#define TERM_LINEWIDTH       (1<<11)	/* support for set term linewidth  */
#define TERM_FONTSCALE       (1<<12)	/* terminal supports fontscale     */
#define TERM_IS_LATEX        (1<<13)	/* text uses TeX markup            */
#define TERM_EXTENDED_COLOR  (1<<14)	/* uses EXTENDED_COLOR_SPECS       */
#define TERM_NULL_SET_COLOR  (1<<15)	/* no support for RGB color        */
#define TERM_POLYGON_PIXELS  (1<<16)	/* filledpolygon rather than fillbox */

/* The terminal interface structure --- heart of the terminal layer.
 *
 * It should go without saying that additional entries may be made
 * only at the end of this structure. Any fields added must be
 * optional - a value of 0 (or NULL pointer) means an older driver
 * does not support that feature - gnuplot must still be able to
 * function without that terminal feature
 */

typedef struct GpTermEntry {
    const char *name;
    const char *description;
    uint xmax,ymax,v_char,h_char,v_tic,h_tic;

    void (*options)();
    void (*init)();
    void (*reset)();
    void (*text)();
    int (*scale) __PROTO((double, double));
    void (*graphics)();
    void (*move) __PROTO((uint, uint));
    void (*vector) __PROTO((uint, uint));
    void (*linetype)(int);
    void (*put_text) __PROTO((uint, uint, const char*));
    /* the following are optional. set term ensures they are not NULL */
    int (*text_angle)(int);
    int (*justify_text) __PROTO((enum JUSTIFY));
    void (*point) __PROTO((uint, uint, int));
    void (*arrow) __PROTO((uint, uint, uint, uint, int));
    int (*set_font) __PROTO((const char *font));
    void (*pointsize) __PROTO((double)); /* change pointsize */
    int flags;
    void (*suspend)(); /* called after one plot of multiplot */
    void (*resume) (); /* called before plots of multiplot */
    void (*fillbox) __PROTO((int, uint, uint, uint, uint)); /* clear in multiplot mode */
    void (*linewidth) __PROTO((double linewidth));
#ifdef USE_MOUSE
    int (*waitforinput)(int);     /* used for mouse and hotkey input */
    void (*put_tmptext) __PROTO((int, const char []));   /* draws temporary text; int determines where: 0=statusline, 1,2: at corners of zoom box, with \r separating text above and below the point */
    void (*set_ruler) __PROTO((int, int));    /* set ruler location; x<0 switches ruler off */
    void (*set_cursor) __PROTO((int, int, int));   /* set cursor style and corner of rubber band */
    void (*set_clipboard) __PROTO((const char[]));  /* write text into cut&paste buffer (clipboard) */
#endif
    int (*make_palette) __PROTO((t_sm_palette *palette));
    /* 1. if palette==NULL, then return nice/suitable
       maximal number of colours supported by this terminal.
       Returns 0 if it can make colours without palette (like
       postscript).
       2. if palette!=NULL, then allocate its own palette
       return value is undefined
       3. available: some negative values of max_colors for whatever
       can be useful
     */
    void (*previous_palette)();
    /* release the palette that the above routine allocated and get
       back the palette that was active before.
       Some terminals, like displays, may draw parts of the figure
       using their own palette. Those terminals that possess only
       one palette for the whole plot don't need this routine.
     */
    void (*set_color) __PROTO((t_colorspec *));
    /* EAM November 2004 - revised to take a pointer to struct rgb_color,
       so that a palette gray value is not the only option for
       specifying color.
     */
    void (*filled_polygon) __PROTO((int points, gpiPoint *corners));
    void (*image) __PROTO((uint, uint, coordval *, gpiPoint *, t_imagecolor));

/* Enhanced text mode driver call-backs */
    void (*enhanced_open) __PROTO((char * fontname, double fontsize,
		double base, bool widthflag, bool showflag,
		int overprint));
    void (*enhanced_flush)();
    void (*enhanced_writec) __PROTO((int c));

/* Driver-specific synchronization or other layering commands.
 * Introduced as an alternative to the ugly sight of
 * driver-specific code strewn about in the core routines.
 * As of this point (July 2005) used only by pslatex.trm
 */
    void (*layer) __PROTO((t_termlayer));

/* Begin/End path control.
 * Needed by PostScript-like devices in order to join the endpoints of
 * a polygon cleanly.
 */
    void (*path) __PROTO((int p));
/* Scale factor for converting terminal coordinates to output
 * pixel coordinates.  Used to provide data for external mousing code.
 */
    double tscale;
/* Pass hypertext for inclusion in the output plot */
    void (*hypertext) __PROTO((int type, const char *text));
#ifdef EAM_BOXED_TEXT
    void (*boxed_text) __PROTO((uint, uint, int));
#endif
    void (*modify_plots) __PROTO((uint operations, int plotno));
    void (*dashtype) __PROTO((int type, t_dashtype *custom_dash_pattern));
} GpTermEntry;

//#define termentry GpTermEntry

enum set_encoding_id {
   S_ENC_DEFAULT, S_ENC_ISO8859_1, S_ENC_ISO8859_2, S_ENC_ISO8859_9, S_ENC_ISO8859_15,
   S_ENC_CP437, S_ENC_CP850, S_ENC_CP852, S_ENC_CP950,
   S_ENC_CP1250, S_ENC_CP1251, S_ENC_CP1252, S_ENC_CP1254,
   S_ENC_KOI8_R, S_ENC_KOI8_U, S_ENC_SJIS,
   S_ENC_UTF8,
   S_ENC_INVALID
};

/* HBB 20020225: this stuff used to be in a separate header, ipc.h,
 * but I strongly disliked the way that was done */

/*
 * There are the following types of interprocess communication from
 * (gnupmdrv, gnuplot_x11) => gnuplot:
 *	OS2_IPC  ... the OS/2 shared memory + event semaphores approach
 *	PIPE_IPC ... communication by using bidirectional pipe
 */


/*
 * OS2_IPC: gnuplot's terminals communicate with it by shared memory + event
 * semaphores => the code in gpexecute.c is used, and nothing more from here.
 */


/* options handling */
enum { 
	UNSET = -1, 
	no = 0, 
	yes = 1 
};

/* Variables of term.c needed by other modules: */

/* the terminal info structure, being the heart of the whole module */
extern GpTermEntry *term;
/* Options string of the currently used terminal driver */
extern char term_options[];
/* access head length + angle without changing API */
extern int curr_arrow_headlength;
/* angle in degrees */
extern double curr_arrow_headangle;
extern double curr_arrow_headbackangle;
extern arrowheadfill curr_arrow_headfilled;
extern bool curr_arrow_headfixedsize;

/* Recycle count for user-defined linetypes */
extern int linetype_recycle_count;
extern int mono_recycle_count;

/* Current 'output' file: name and open filehandle */
extern char *outstr;
extern FILE *gpoutfile;

/* Output file where postscript terminal output goes to.
   In particular:
	gppsfile == gpoutfile
		for 'set term': postscript, pstex
	gppsfile == PSLATEX_auxfile
		for 'set term': pslatex, cairolatex
	gppsfile == 0
		for all other terminals
*/
extern FILE *gppsfile;
extern char *PS_psdir;

extern bool monochrome;
extern bool multiplot;

/* 'set encoding' support: index of current encoding ... */
extern enum set_encoding_id encoding;
/* ... in table of encoding names: */
extern const char *encoding_names[];
/* parsing table for encodings */
extern const GenTable set_encoding_tbl[];

/* mouse module needs this */
extern bool term_initialised;

/* The qt and wxt terminals cannot be used in the same session. */
/* Whichever one is used first to plot, this locks out the other. */
extern void *term_interlock;

/* Support for enhanced text mode. */
extern char  enhanced_text[];
extern char *enhanced_cur_text;
extern double enhanced_fontscale;
/* give array size to allow the use of sizeof */
extern char enhanced_escape_format[16];
extern bool ignore_enhanced_text;


/* Prototypes of functions exported by term.c */

void term_set_output __PROTO((char *));
void term_initialise();
void term_start_plot();
void term_end_plot();
void term_start_multiplot();
void term_end_multiplot();
/* void term_suspend(); */
void term_reset();
void term_apply_lp_properties __PROTO((lp_style_type *lp));
void term_check_multiplot_okay __PROTO((bool));
void init_monochrome();
GpTermEntry *change_term __PROTO((const char *name, int length));

void write_multiline __PROTO((uint, uint, char *, JUSTIFY, VERT_JUSTIFY, int, const char *));
int estimate_strlen __PROTO((char *));
char *estimate_plaintext __PROTO((char *));
void list_terms();
char* get_terminals_names();
GpTermEntry *set_term();
void init_terminal();
void test_term();

/* Support for enhanced text mode. */
const char *enhanced_recursion(const char *p, bool brace, char *fontname, double fontsize,
	double base, bool widthflag, bool showflag, int overprint);
void enh_err_check __PROTO((const char *str));
/* note: c is char, but must be declared int due to K&R compatibility. */
void do_enh_writec __PROTO((int c));
/* flag: don't use enhanced output methods --- for output of
 * filenames, which usually looks bad using subscripts */
void ignore_enhanced __PROTO((bool flag));

/* Simple-minded test that point is with drawable area */
bool on_page __PROTO((int x, int y));

/* Convert a fill style into a backwards compatible packed form */
int style_from_fill __PROTO((fill_style_type *));

#ifdef EAM_OBJECTS
/* Terminal-independent routine to draw a circle or arc */
void do_arc __PROTO(( uint cx, uint cy, double radius,
                      double arc_start, double arc_end,
		      int style, bool wedge));
#endif

#ifdef LINUXVGA
void LINUX_setup();
#endif

#ifdef PC
void PC_setup();
#endif

#ifdef VMS
void vms_reset();
#endif

#ifdef OS2
int PM_pause __PROTO((char *));
void PM_intc_cleanup();
# ifdef USE_MOUSE
void PM_update_menu_items();
void PM_set_gpPMmenu __PROTO((struct t_gpPMmenu * gpPMmenu));
# endif
#endif

int load_dashtype __PROTO((struct t_dashtype *dt, int tag));
void lp_use_properties __PROTO((lp_style_type *lp, int tag));
void load_linetype __PROTO((lp_style_type *lp, int tag));

/* Wrappers for term->path() */
void newpath();
void closepath();

/* Generic wrapper to check for mouse events or hotkeys during
 * non-interactive input (e.g. "load")
 */
void check_for_mouse_events();

#endif /* GNUPLOT_TERM_API_H */
