/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

#ifndef _JBIG2_TEXT_H
#define _JBIG2_TEXT_H

/**
 * Headers for Text region handling
 **/

typedef enum {
    JBIG2_CORNER_BOTTOMLEFT = 0,
    JBIG2_CORNER_TOPLEFT = 1,
    JBIG2_CORNER_BOTTOMRIGHT = 2,
    JBIG2_CORNER_TOPRIGHT = 3
} Jbig2RefCorner;

typedef struct {
    boolint SBHUFF;
    boolint SBREFINE;
    boolint SBDEFPIXEL;
    Jbig2ComposeOp SBCOMBOP;
    boolint TRANSPOSED;
    Jbig2RefCorner REFCORNER;
    int SBDSOFFSET;
    /* int SBW; */
    /* int SBH; */
    uint32_t SBNUMINSTANCES;
    int LOGSBSTRIPS;
    int SBSTRIPS;
    /* int SBNUMSYMS; */
    /* SBSYMCODES */
    /* SBSYMCODELEN */
    /* SBSYMS */
    Jbig2HuffmanTable *SBHUFFFS;
    Jbig2HuffmanTable *SBHUFFDS;
    Jbig2HuffmanTable *SBHUFFDT;
    Jbig2HuffmanTable *SBHUFFRDW;
    Jbig2HuffmanTable *SBHUFFRDH;
    Jbig2HuffmanTable *SBHUFFRDX;
    Jbig2HuffmanTable *SBHUFFRDY;
    Jbig2HuffmanTable *SBHUFFRSIZE;
    Jbig2ArithIntCtx *IADT;
    Jbig2ArithIntCtx *IAFS;
    Jbig2ArithIntCtx *IADS;
    Jbig2ArithIntCtx *IAIT;
    Jbig2ArithIaidCtx *IAID;
    Jbig2ArithIntCtx *IARI;
    Jbig2ArithIntCtx *IARDW;
    Jbig2ArithIntCtx *IARDH;
    Jbig2ArithIntCtx *IARDX;
    Jbig2ArithIntCtx *IARDY;
    boolint SBRTEMPLATE;
    int8_t sbrat[4];
} Jbig2TextRegionParams;

int
jbig2_decode_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                         const Jbig2TextRegionParams *params,
                         const Jbig2SymbolDict *const *dicts, const uint32_t n_dicts,
                         Jbig2Image *image, const byte *data, const size_t size, Jbig2ArithCx *GR_stats, Jbig2ArithState *as, Jbig2WordStream *ws);

int jbig2_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);

#endif /* _JBIG2_TEXT_H */
