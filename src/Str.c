/*
 * Copyright (C) 2022-2024 Jordan Bancino <@jordan:bancino.net>
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
#include <Str.h>

#include <Memory.h>
#include <Util.h>
#include <Rand.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

struct Str
{
    size_t len;
    size_t size;
    uint8_t *buf;
};

Str *StrCreate(uint8_t *initialBuf, size_t len)
{
    Str *str = Malloc(sizeof(Str));
    if (!str)
    {
        return NULL;
    }

    if (initialBuf)
    {
        str->buf = Malloc(len * sizeof(uint8_t));
        if (!str->buf)
        {
            Free(str);
            return NULL;
        }

        memcpy(str->buf, initialBuf, len);

        str->len = len;
        str->size = len;
    }
    else
    {
        str->buf = NULL;
        str->len = 0;
        str->size = 0;
    }

    return str;
}

void StrFree(Str *str)
{
    if (!str)
    {
        return;
    }

    Free(str->buf);
    Free(str);
}

size_t StrLen(Str *str)
{
    if (!str)
    {
        return 0;
    }

    return str->len;
}

size_t StrSize(Str *str)
{
    if (!str)
    {
        return 0;
    }

    // Size of the string metadata and allocated buffer
    // space.
    return sizeof(Str) + str->size;
}

char * StrToC(Str *str)
{
    if (!str)
    {
        return NULL;
    }

    char *cStr = Malloc((str->len + 1) * sizeof(char));
    if (!cStr)
    {
        return NULL;
    }

    memcpy(cStr, str->buf, str->len);
    cStr[str->len] = '\0';
    return cStr;

}

uint32_t
StrUtf16Decode(uint16_t high, uint16_t low)
{
    if (high <= 0xD7FF)
    {
        return high;
    }
    else if (high <= 0xDBFF)
    {
        unsigned short hS = (high - 0xD800) * 0x400;
        unsigned short lS = low - 0xDC00;

        return (lS | hS) + 0x10000;
    }
    else
    {
        return 0;
    }
}

Str *
StrUtf8Encode(uint32_t codepoint)
{
    uint8_t str[5];

    if (codepoint <= 0x7F && codepoint != 0)    /* Plain ASCII */
    {
        str[0] = (uint8_t) codepoint;
        str[1] = '\0';
    }
    else if (codepoint <= 0x07FF)  /* 2-byte */
    {
        str[0] = (uint8_t) (((codepoint >> 6) & 0x1F) | 0xC0);
        str[1] = (uint8_t) (((codepoint >> 0) & 0x3F) | 0x80);
        str[2] = '\0';
    }
    else if (codepoint <= 0xFFFF)  /* 3-byte */
    {
        str[0] = (uint8_t) (((codepoint >> 12) & 0x0F) | 0xE0);
        str[1] = (uint8_t) (((codepoint >> 6) & 0x3F) | 0x80);
        str[2] = (uint8_t) (((codepoint >> 0) & 0x3F) | 0x80);
        str[3] = '\0';
    }
    else if (codepoint <= 0x10FFFF)/* 4-byte */
    {
        str[0] = (uint8_t) (((codepoint >> 18) & 0x07) | 0xF0);
        str[1] = (uint8_t) (((codepoint >> 12) & 0x3F) | 0x80);
        str[2] = (uint8_t) (((codepoint >> 6) & 0x3F) | 0x80);
        str[3] = (uint8_t) (((codepoint >> 0) & 0x3F) | 0x80);
        str[4] = '\0';
    }
    else
    {
        /* Send replacement character */
        str[0] = (uint8_t) 0xEF;
        str[1] = (uint8_t) 0xBF;
        str[2] = (uint8_t) 0xBD;
        str[3] = '\0';
    }

    Str *ret = StrCreate(str, strlen((char *)str));
    return ret;
}

Str *
StrDuplicate(Str *inStr)
{
    if (!inStr)
    {
        return NULL;
    }

    Str *outStr = StrCreate(inStr->buf, inStr->len);
    return outStr;
}

char *
StrSubstr(const char *inStr, size_t start, size_t end)
{
    size_t len;
    size_t i;
    size_t j;

    char *outStr;

    if (!inStr)
    {
        return NULL;
    }

    if (start >= end)
    {
        return NULL;
    }

    len = end - start;
    outStr = Malloc(len + 1);
    if (!outStr)
    {
        return NULL;
    }

    j = 0;
    for (i = start; i < end; i++)
    {
        if (inStr[i] == '\0')
        {
            break;
        }

        outStr[j] = inStr[i];
        j++;
    }

    outStr[j] = '\0';

    return outStr;
}

char *
StrConcat(size_t nStr,...)
{
    va_list argp;
    char *str;
    char *strp;
    size_t strLen = 0;
    size_t i;

    va_start(argp, nStr);
    for (i = 0; i < nStr; i++)
    {
        char *argStr = va_arg(argp, char *);

        if (argStr)
        {
            strLen += strlen(argStr);
        }
    }
    va_end(argp);

    str = Malloc(strLen + 1);
    strp = str;

    va_start(argp, nStr);

    for (i = 0; i < nStr; i++)
    {
        /* Manually copy chars instead of using strcopy() so we don't
         * have to call strlen() on the strings again, and we aren't
         * writing useless null chars. */

        char *argStr = va_arg(argp, char *);

        if (argStr)
        {
            while (*argStr)
            {
                *strp = *argStr;
                strp++;
                argStr++;
            }
        }
    }

    va_end(argp);
    str[strLen] = '\0';
    return str;
}

bool
StrBlank(const char *str)
{
    bool blank = true;
    size_t i = 0;

    while (str[i])
    {
        blank &= isspace((unsigned char) str[i]);
        /* No need to continue if we don't have a blank */
        if (!blank)
        {
            break;
        }
        i++;
    }
    return blank;
}

char *
StrRandom(size_t len)
{
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    char *str;
    uint32_t *nums;
    size_t i;

    if (!len)
    {
        return NULL;
    }

    str = Malloc(len + 1);

    if (!str)
    {
        return NULL;
    }

    nums = Malloc(len * sizeof(uint32_t));
    if (!nums)
    {
        Free(str);
        return NULL;
    }

    /* TODO: This seems slow. */
    RandIntN(nums, len, sizeof(charset) - 1);
    for (i = 0; i < len; i++)
    {
        str[i] = charset[nums[i]];
    }

    Free(nums);
    str[len] = '\0';
    return str;
}

char *
StrInt(long i)
{
    char *str;
    int len;

    len = snprintf(NULL, 0, "%ld", i);
    str = Malloc(len + 1);
    if (!str)
    {
        return NULL;
    }

    snprintf(str, len + 1, "%ld", i);

    return str;
}

char *
StrLower(char *str)
{
    char *ret;

    size_t len;
    size_t i;

    if (!str)
    {
        return NULL;
    }

    len = strlen(str);
    ret = Malloc(len + 1);

    for (i = 0; i < len; i++)
    {
        ret[i] = tolower(str[i]);
    }

    ret[len] = '\0';

    return ret;
}

bool
StrEquals(const char *str1, const char *str2)
{
    /* Both strings are NULL, they're equal */
    if (!str1 && !str2)
    {
        return true;
    }

    /* One or the other is NULL, they're not equal */
    if (!str1 || !str2)
    {
        return false;
    }

    /* Neither are NULL, do a regular string comparison */
    return (strcmp(str1, str2) == 0);
}
