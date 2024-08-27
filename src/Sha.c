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
#include <Sha.h>
#include <Memory.h>

#include <stdio.h>
#include <string.h>

char *
ShaToHex(unsigned char *bytes, HashType type)
{
    size_t i = 0, size;
    char *str;

    switch (type)
    {
        case HASH_SHA1:
            size = 20;
            break;
        case HASH_SHA256:
            size = 32;
            break;
        default:
            return NULL;
    }

    str = Malloc(((size * 2) + 1) * sizeof(char));
    if (!str)
    {
        return NULL;
    }

    for (i = 0; i < size; i++)
    {
        snprintf(str + (2 * i), 3, "%02x", bytes[i]);
    }

    return str;
}
unsigned char *
Sha256(char *str)
{
    return Sha256Raw((unsigned char *) str, str ? strlen(str) : 0);
}

unsigned char *
Sha1(char *str)
{
    return Sha1Raw((unsigned char *) str, str ? strlen(str) : 0);
}
