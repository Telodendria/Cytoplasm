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
#include <Util.h>

#include <Memory.h>
#include <Platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

#ifndef SSIZE_MAX
#define SSIZE_MAX LONG_MAX
#endif

uint64_t
UtilTsMillis(void)
{
    struct timeval tv;

    uint64_t ts;
    uint64_t sec;
    uint64_t usec;

    gettimeofday(&tv, NULL);

    /*
     * POSIX defines struct timeval to be:
     *
     * struct timeval {
     *   time_t tv_sec;
     *   suseconds_t tv_usec;
     * };
     *
     * Note: Although the C standard does not require that
     * time_t be an integer type (it may be floating point),
     * according to POSIX, time_t shall be an integer type.
     * As we are targeting POSIX, not ANSI/ISO C, we can
     * rely on this.
     *
     * The same goes for suseconds_t.
     */

    // Two separate steps because time_t might be 32-bit. In that
    // case, we want the multiplication to happen after the promotion
    // to uint64_t.
    sec = tv.tv_sec;
    sec *= 1000;

    usec = tv.tv_usec / 1000;

    ts = sec + usec;

    return ts;
}

#ifdef PLATFORM_DARWIN
#define st_mtim st_mtimespec
#endif

uint64_t
UtilLastModified(char *path)
{
    struct stat st;
    uint64_t ts = 0;

    if (stat(path, &st) == 0)
    {
        ts = st.st_mtim.tv_sec;
        ts *= 1000;
        ts += st.st_mtim.tv_nsec / 1000000;
    }

    return ts;
}

#ifdef PLATFORM_DARWIN
#undef st_mtim
#endif

int
UtilMkdir(const char *dir, const mode_t mode)
{
    char tmp[PATH_MAX];
    char *p = NULL;

    struct stat st;

    size_t len;

    len = strnlen(dir, PATH_MAX);
    if (!len || len == PATH_MAX)
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(tmp, dir, len);
    tmp[len] = '\0';

    if (tmp[len - 1] == '/')
    {
        tmp[len - 1] = '\0';
    }

    if (stat(tmp, &st) == 0 && S_ISDIR(st.st_mode))
    {
        return 0;
    }

    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;

            if (stat(tmp, &st) != 0)
            {
                if (mkdir(tmp, mode) < 0)
                {
                    /* errno already set by mkdir() */
                    return -1;
                }
            }
            else if (!S_ISDIR(st.st_mode))
            {
                errno = ENOTDIR;
                return -1;
            }

            *p = '/';
        }
    }

    if (stat(tmp, &st) != 0)
    {
        if (mkdir(tmp, mode) < 0)
        {
            /* errno already set by mkdir() */
            return -1;
        }
    }
    else if (!S_ISDIR(st.st_mode))
    {
        errno = ENOTDIR;
        return -1;
    }

    return 0;
}

int
UtilSleepMillis(uint64_t ms)
{
    struct timespec ts;
    int res;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;

    res = nanosleep(&ts, &ts);

    return res;
}

ssize_t
UtilGetDelim(char **linePtr, size_t * n, int delim, Stream * stream)
{
    char *curPos, *newLinePtr;
    size_t newLinePtrLen;
    int c;

    if (!linePtr || !n || !stream)
    {
        errno = EINVAL;
        return -1;
    }

    if (*linePtr == NULL)
    {
        *n = 128;

        if (!(*linePtr = Malloc(*n)))
        {
            errno = ENOMEM;
            return -1;
        }
    }

    curPos = *linePtr;

    while (1)
    {
        c = StreamGetc(stream);

        if (StreamError(stream) || (c == EOF && curPos == *linePtr))
        {
            return -1;
        }

        if (c == EOF)
        {
            break;
        }

        if ((*linePtr + *n - curPos) < 2)
        {
            if (SSIZE_MAX / 2 < *n)
            {
#ifdef EOVERFLOW
                errno = EOVERFLOW;
#else
                errno = ERANGE;
#endif
                return -1;
            }

            newLinePtrLen = *n * 2;

            if (!(newLinePtr = Realloc(*linePtr, newLinePtrLen)))
            {
                errno = ENOMEM;
                return -1;
            }

            curPos = newLinePtr + (curPos - *linePtr);
            *linePtr = newLinePtr;
            *n = newLinePtrLen;
        }

        *curPos++ = (char) c;

        if (c == delim)
        {
            break;
        }
    }

    *curPos = '\0';
    return (ssize_t) (curPos - *linePtr);
}

ssize_t
UtilGetLine(char **linePtr, size_t * n, Stream * stream)
{
    return UtilGetDelim(linePtr, n, '\n', stream);
}

static void
ThreadNoDestructor(void *p)
{
    free(p);
}

uint32_t
UtilThreadNo(void)
{
    static pthread_key_t key;
    static int createdKey = 0;
    static unsigned long count = 0;

    uint32_t *no;

    if (!createdKey)
    {
        pthread_key_create(&key, ThreadNoDestructor);
        createdKey = 1;
    }

    no = pthread_getspecific(key);
    if (!no)
    {
        no = malloc(sizeof(uint32_t));
        *no = count++;
        pthread_setspecific(key, no);
    }

    return *no;
}
