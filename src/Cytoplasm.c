/*
 * Copyright (C) 2022-2023 Jordan Bancino <@jordan:bancino.net>
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

#include <Cytoplasm.h>

int
CytoplasmGetVersion(void)
{
    return CYTOPLASM_VERSION;
}

const char *
CytoplasmGetVersionStr(void)
{
    return "v" STRINGIFY(CYTOPLASM_VERSION_MAJOR)
           "." STRINGIFY(CYTOPLASM_VERSION_MINOR)
           "." STRINGIFY(CYTOPLASM_VERSION_PATCH)
#if CYTOPLASM_VERSION_ALPHA
    "-alpha" STRINGIFY(CYTOPLASM_VERSION_ALPHA)
#elif CYTOPLASM_VERSION_BETA
    "-beta" STRINGIFY(CYTOPLASM_VERSION_BETA)
#endif
    ;
}
