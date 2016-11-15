/*
 * KMP_GLT_IO.c -- RTL IO
 */


//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.txt for details.
//
//===----------------------------------------------------------------------===//


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#ifndef __ABSOFT_WIN
# include <sys/types.h>
#endif

#include "kmp_glt.h" // KMP_GTID_DNE, __kmp_debug_buf, etc

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

#define __kmp_stderr     (stderr)

void
__kmp_vprintf( enum kmp_io __kmp_io, char const * format, va_list ap )
{
    if ( __kmp_debug_buf && __kmp_debug_buffer != NULL ) {

        int dc = ( __kmp_debug_buf_atomic ?
                   KMP_TEST_THEN_INC32( & __kmp_debug_count) : __kmp_debug_count++ )
                   % __kmp_debug_buf_lines;
        char *db = & __kmp_debug_buffer[ dc * __kmp_debug_buf_chars ];
        int chars = 0;

        #ifdef KMP_DEBUG_PIDS
            chars = KMP_SNPRINTF( db, __kmp_debug_buf_chars, "pid=%d: ", (kmp_int32)getpid() );
        #endif
        chars += KMP_VSNPRINTF( db, __kmp_debug_buf_chars, format, ap );

        if ( chars + 1 > __kmp_debug_buf_chars ) {
            if ( chars + 1 > __kmp_debug_buf_warn_chars ) {
                fprintf( __kmp_stderr,
                     "OMP warning: Debugging buffer overflow; increase KMP_DEBUG_BUF_CHARS to %d\n",
                     chars + 1 );
                fflush( __kmp_stderr );
                __kmp_debug_buf_warn_chars = chars + 1;
            }
            /* terminate string if overflow occurred */
            db[ __kmp_debug_buf_chars - 2 ] = '\n';
            db[ __kmp_debug_buf_chars - 1 ] = '\0';
        }
    } else {
        #ifdef KMP_DEBUG_PIDS
            fprintf( __kmp_stderr, "pid=%d: ", (kmp_int32)getpid() );
        #endif
        vfprintf( __kmp_stderr, format, ap );
        fflush( __kmp_stderr );
    }
}

void
__kmp_printf( char const * format, ... )
{
    va_list ap;
    va_start( ap, format );

    __kmp_acquire_bootstrap_lock( &__kmp_global.stdio_lock );
    __kmp_vprintf( kmp_err, format, ap );
    __kmp_release_bootstrap_lock( &__kmp_global.stdio_lock );

    va_end( ap );
}

void
__kmp_printf_no_lock( char const * format, ... )
{
    va_list ap;
    va_start( ap, format );

    __kmp_vprintf( kmp_err, format, ap );

    va_end( ap );
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

