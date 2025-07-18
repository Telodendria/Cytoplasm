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

#ifndef CYTOPLASM_UTF8_H
#define CYTOPLASM_UTF8_H

#include <Str.h>

// TODO: Unicode UTF-8 stuff

extern size_t Utf8CodepointLen(Str *);
extern size_t Utf8GraphemeLen(Str *);

extern Str *Utf8CodepointAt(Str *str, size_t i);
extern Str *Utf8GraphemeAt(Str *str, size_t i);

extern Str *Utf8CodepointSubStr(Str *, size_t start, size_t end);
extern Str *Utf8GraphemeSubStr(Str *, size_t start, size_t end);

extern Str *Utf8Encode(uint32_t);
extern uint32_t Utf8Decode(Str *);

#endif // CYTOPLASM_UTF8_H
