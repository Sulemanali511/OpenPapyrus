// \file       crc32_table.c
// \brief      Precalculated CRC32 table with correct endianness
//  Author:     Lasse Collin
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
#include "common.h"
#pragma hdrstop

extern const uint32_t lzma_crc32_table[8][256]; // Having the declaration here silences clang -Wmissing-variable-declarations.
extern const uint64_t lzma_crc64_table[4][256]; // Having the declaration here silences clang -Wmissing-variable-declarations.

#ifdef WORDS_BIGENDIAN
	#include "crc32_table_be.h"
	#include "crc64_table_be.h"
#else
	#include "crc32_table_le.h"
	#include "crc64_table_le.h"
#endif
