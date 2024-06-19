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
#ifndef CYTOPLASM_CYTOPLASM_H
#define CYTOPLASM_CYTOPLASM_H

#define CYTOPLASM_VERSION_MAJOR 0
#define CYTOPLASM_VERSION_MINOR 4
#define CYTOPLASM_VERSION_PATCH 1
#define CYTOPLASM_VERSION ((CYTOPLASM_VERSION_MAJOR * 10000) + (CYTOPLASM_VERSION_MINOR * 100) + (CYTOPLASM_VERSION_PATCH))

#define CYTOPLASM_VERSION_ALPHA 1
#define CYTOPLASM_VERSION_BETA 0
#define CYTOPLASM_VERSION_STABLE (!CYTOPLASM_VERSION_ALPHA && !CYTOPLASM_VERSION_BETA)

#define STRINGIFY(x) #x

/***
 * @Nm Cytoplasm
 * @Nd A simple API that provides metadata on the library itself.
 * @Dd September 30 2022
 * @Xr Sha2
 *
 * This API simply provides name and versioning information for the
 * currently loaded library.
 */

/** */
extern int CytoplasmGetVersion(void);

/**
 * Get the library version. This will be useful mostly for printing
 * to log files, but it may also be used by a program to verify that
 * the version is new enough.
 * 
 * This function returns a string, which should usually be able to be
 * parsed using sscanf() if absolutely necessary.
 */
extern const char * CytoplasmGetVersionStr(void);

#endif /* CYTOPLASM_CYTOPLASM_H */
