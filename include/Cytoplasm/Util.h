/*
 * Copyright (C) 2022-2025 Jordan Bancino <@jordan:synapse.telodendria.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef CYTOPLASM_UTIL_H
#define CYTOPLASM_UTIL_H

/***
 * @Nm Util
 * @Nd Some misc. helper functions that don't need their own headers.
 * @Dd February 15 2023
 * 
 * This header holds a number of random functions related to strings,
 * time, the filesystem, and other simple tasks that don't require a
 * full separate API. For the most part, the functions here are
 * entirely standalone, depending only on POSIX functions, however
 * there are a few that depend explicitly on a few other APIs. Those
 * are noted.
 */

#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>

#include "Stream.h"

/**
 * Get the current timestamp in milliseconds since the Unix epoch. This
 * uses
 * .Xr gettimeofday 2 
 * and time_t, and converts it to a single number, which is then
 * returned to the caller.
 * .Pp
 * A note on the 2038 problem: as long as sizeof(time_t) >= 8, that is,
 * as long as the time_t type is 64 bits or more, then everything
 * should be fine. On most, if not, all, 64-bit systems, time_t is 64
 * bits. time_t is promoted to a 64-bit integer before it is converted
 * to milliseconds, so there is no risk of overflue due to the
 * multiplication by 1000. However, if time_t is only 32 bits, it will
 * overflow before it even gets to this function, which will cause this
 * function to produce unexpected results.
 */
extern uint64_t UtilTsMillis(void);

/**
 * Use
 * .Xr stat 2
 * to get the last modified time of the given file, or zero if there
 * was an error getting the last modified time of a file. This is
 * primarily useful for caching file data.
 */
extern uint64_t UtilLastModified(char *);

/**
 * This function behaves just like the system call
 * .Xr mkdir 2 ,
 * but it creates any intermediate directories as necessary, unlike
 * .Xr mkdir 2 .
 */
extern int UtilMkdir(const char *, const mode_t);

/**
 * Sleep the calling thread for the given number of milliseconds.
 * POSIX does not have a very friendly way to sleep, so this wraps
 * .Xr nanosleep 2
 * to make its usage much, much simpler.
 */
extern int UtilSleepMillis(uint64_t);

/**
 * This function works identically to the POSIX
 * .Xr getdelim 3 ,
 * except that it assumes pointers were allocated with the Memory API
 * and it reads from a Stream instead of a file pointer.
 */
extern ssize_t UtilGetDelim(char **, size_t *, int, Stream *);

/**
 * This function is just a special case of 
 * .Fn UtilGetDelim
 * that sets the delimiter to the newline character.
 */
extern ssize_t UtilGetLine(char **, size_t *, Stream *);

/**
 * Get a unique number associated with the current thread.
 * Numbers are assigned in the order that threads call this
 * function, but are guaranteed to be unique in identifying
 * the thread in a more human-readable way than just casting
 * the return value of
 * .Fn pthread_self
 * to a number.
 */
extern uint32_t UtilThreadNo(void);

#endif                             /* CYTOPLASM_UTIL_H */
