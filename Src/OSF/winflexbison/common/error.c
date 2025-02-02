/* Error handler for noninteractive utilities
   Copyright (C) 1990-1998, 2000-2007, 2009-2011 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by David MacKenzie <djm@gnu.ai.mit.edu>.  */

#include <flexbison_common.h>
#pragma hdrstop
#if !_LIBC && ENABLE_NLS
	#define _(msgid) gettext(msgid)
#endif
#ifdef _LIBC
	#include <libintl.h>
	#define mbsrtowcs __mbsrtowcs
#endif
#if USE_UNLOCKED_IO
	#include "unlocked-io.h"
#endif
#ifndef _
	#define _(String) String
#endif
/* If NULL, error will flush stdout, then print on stderr the program
   name, a colon and a space.  Otherwise, error will call this
   function without parameters instead.  */
void (* error_print_progname)(void);

uint error_message_count; /* This variable is incremented each time `error' is called.  */

#ifdef _LIBC
/* In the GNU C library, there is a predefined variable for this.  */

#define program_name program_invocation_name
#include <libio/libioP.h>

/* In GNU libc we want do not want to use the common name `error' directly.
   Instead make it a weak alias.  */
extern void __error(int status, int errnum, const char * message, ...) __attribute__ ((__format__(__printf__, 3, 4)));
extern void __error_at_line(int status, int errnum, const char * file_name, uint line_number, const char * message, ...) __attribute__ ((__format__(__printf__, 5, 6)));
#define error __error
#define error_at_line __error_at_line

#include <libio/iolibio.h>
#define fflush(s) INTUSE(_IO_fflush) (s)
#undef putc
#define putc(c, fp) INTUSE(_IO_putc) (c, fp)

#include <bits/libc-lock.h>

#else /* not _LIBC */

#if (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
	/* Get declarations of the Win32 API functions.  */
	//#define WIN32_LEAN_AND_MEAN
	//#include <windows.h>
#endif
/* The gnulib override of fcntl is not needed in this file.  */
#undef fcntl
#if !HAVE_DECL_STRERROR_R && STRERROR_R_CHAR_P
	#ifndef HAVE_DECL_STRERROR_R
		"this configure-time declaration test was not run"
	#endif
	char * strerror_r();
#endif

// The calling program should define program_name and set it to the name of the executing program. 
//extern const char *program_name;
const char * get_program_name();

#if HAVE_STRERROR_R || defined strerror_r
	#define __strerror_r strerror_r
#endif /* HAVE_STRERROR_R || defined strerror_r */
#endif  /* not _LIBC */
#if !_LIBC
#include <io.h>
// 
// Return non-zero if FD is open
// 
static int is_open(int fd)
{
#if (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
	/* On Win32: The initial state of unassigned standard file descriptors is
	   that they are open but point to an INVALID_HANDLE_VALUE.  There is no
	   fcntl, and the gnulib replacement fcntl does not support F_GETFL.  */
	return (HANDLE)_get_osfhandle(fd) != INVALID_HANDLE_VALUE;
#else
	#ifndef F_GETFL
		#error Please port fcntl to your platform
	#endif
	return 0 <= fcntl(fd, F_GETFL);
#endif
}
#endif

static void flush_stdout(void)
{
#if !_LIBC
	int stdout_fd;
#if GNULIB_FREOPEN_SAFER
	/* Use of gnulib's freopen-safer module normally ensures that fileno (stdout) == 1 whenever stdout is open.  */
	stdout_fd = STDOUT_FILENO;
#else
	/* POSIX states that fileno (stdout) after fclose is unspecified.  But in
	   practice it is not a problem, because stdout is statically allocated and
	   the fd of a FILE stream is stored as a field in its allocated memory.  */
	stdout_fd = _fileno(stdout);
#endif
	/* POSIX states that fflush (stdout) after fclose is unspecified; it
	   is safe in glibc, but not on all other platforms.  fflush (NULL)
	   is always defined, but too draconian.  */
	if(0 <= stdout_fd && is_open(stdout_fd))
#endif
	fflush(stdout);
}

static void print_errno_message(int errnum)
{
	char const * s;
#if defined HAVE_STRERROR_R || _LIBC
	char errbuf[1024];
#if STRERROR_R_CHAR_P || _LIBC
	s = __strerror_r(errnum, errbuf, sizeof errbuf);
#else
	if(__strerror_r(errnum, errbuf, sizeof errbuf) == 0)
		s = errbuf;
	else
		s = 0;
#endif
#else
	s = strerror(errnum);
#endif
#if !_LIBC
	SETIFZ(s, _("Unknown system error"));
#endif
#if _LIBC
	__fxprintf(NULL, ": %s", s);
#else
	fprintf(stderr, ": %s", s);
#endif
}

static void error_tail(int status, int errnum, const char * message, va_list args)
{
#if _LIBC
	if(_IO_fwide(stderr, 0) > 0) {
#define ALLOCA_LIMIT 2000
		size_t len = strlen(message) + 1;
		wchar_t * wmessage = NULL;
		mbstate_t st;
		size_t res;
		const char * tmp;
		bool use_malloc = false;
		while(1) {
			if(__libc_use_alloca(len * sizeof(wchar_t)))
				wmessage = (wchar_t*)alloca(len * sizeof(wchar_t));
			else {
				if(!use_malloc)
					wmessage = NULL;
				wchar_t * p = (wchar_t*)SAlloc::R(wmessage, len * sizeof(wchar_t));
				if(p == NULL) {
					SAlloc::F(wmessage);
					fputws_unlocked(L"out of memory\n", stderr);
					return;
				}
				wmessage = p;
				use_malloc = true;
			}
			memzero(&st, sizeof(st));
			tmp = message;
			res = mbsrtowcs(wmessage, &tmp, len, &st);
			if(res != len)
				break;
			if(__builtin_expect(len >= SIZE_MAX / 2, 0)) {
				/* This really should not happen if everything is fine.  */
				res = (size_t)-1;
				break;
			}
			len *= 2;
		}
		if(res == (size_t)-1) {
			/* The string cannot be converted.  */
			if(use_malloc) {
				SAlloc::F(wmessage);
				use_malloc = false;
			}
			wmessage = (wchar_t*)L"???";
		}
		__vfwprintf(stderr, wmessage, args);
		if(use_malloc)
			SAlloc::F(wmessage);
	}
	else
#endif
	vfprintf(stderr, message, args);
	va_end(args);
	++error_message_count;
	if(errnum)
		print_errno_message(errnum);
#if _LIBC
	__fxprintf(NULL, "\n");
#else
	putc('\n', stderr);
#endif
	fflush(stderr);
	if(status)
		exit(status);
}
// 
// Print the program name and error message MESSAGE, which is a printf-style
// format string with optional args.
// If ERRNUM is nonzero, print its corresponding system error message.
// Exit with status STATUS if it is nonzero.
// 
void error(int status, int errnum, const char * message, ...)
{
	va_list args;
#if defined _LIBC && defined __libc_ptf_call
	// We do not want this call to be cut short by a thread cancellation.  Therefore disable cancellation for now.
	int state = PTHREAD_CANCEL_ENABLE;
	__libc_ptf_call(pthread_setcancelstate, (PTHREAD_CANCEL_DISABLE, &state), 0);
#endif
	flush_stdout();
#ifdef _LIBC
	_IO_flockfile(stderr);
#endif
	if(error_print_progname)
		(*error_print_progname)();
	else {
#if _LIBC
		__fxprintf(NULL, "%s: ", get_program_name());
#else
		fprintf(stderr, "%s: ", get_program_name());
#endif
	}
	va_start(args, message);
	error_tail(status, errnum, message, args);
#ifdef _LIBC
	_IO_funlockfile(stderr);
#ifdef __libc_ptf_call
	__libc_ptf_call(pthread_setcancelstate, (state, NULL), 0);
#endif
#endif
}

// Sometimes we want to have at most one error per line.  This variable controls whether this mode is selected or not.
int error_one_per_line;

void error_at_line(int status, int errnum, const char * file_name, uint line_number, const char * message, ...)
{
	va_list args;
	if(error_one_per_line) {
		static const char * old_file_name;
		static uint old_line_number;
		if(old_line_number == line_number && (file_name == old_file_name || sstreq(old_file_name, file_name)))
			return; // Simply return and print nothing.  
		old_file_name = file_name;
		old_line_number = line_number;
	}
#if defined _LIBC && defined __libc_ptf_call
	// We do not want this call to be cut short by a thread cancellation.  Therefore disable cancellation for now.
	int state = PTHREAD_CANCEL_ENABLE;
	__libc_ptf_call(pthread_setcancelstate, (PTHREAD_CANCEL_DISABLE, &state), 0);
#endif
	flush_stdout();
#ifdef _LIBC
	_IO_flockfile(stderr);
#endif
	if(error_print_progname)
		(*error_print_progname)();
	else {
#if _LIBC
		__fxprintf(NULL, "%s:", get_program_name());
#else
		fprintf(stderr, "%s:", get_program_name());
#endif
	}

#if _LIBC
	__fxprintf(NULL, file_name != NULL ? "%s:%d: " : " ", file_name, line_number);
#else
	fprintf(stderr, file_name != NULL ? "%s:%d: " : " ", file_name, line_number);
#endif
	va_start(args, message);
	error_tail(status, errnum, message, args);
#ifdef _LIBC
	_IO_funlockfile(stderr);
# ifdef __libc_ptf_call
	__libc_ptf_call(pthread_setcancelstate, (state, NULL), 0);
#endif
#endif
}

#ifdef _LIBC
	// Make the weak alias
	#undef error
	#undef error_at_line
	weak_alias(__error, error)
	weak_alias(__error_at_line, error_at_line)
#endif
