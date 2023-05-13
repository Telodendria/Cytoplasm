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
#ifndef CYTOPLASM_RUNTIME_H
#define CYTOPLASM_RUNTIME_H

/***
 * @Nm Runtime
 * @Nd Supporting functions for the Cytoplasm runtime.
 * @Dd May 23 2023
 * @Xr Memory
 *
 * .Nm
 * provides supporting functions for the Cytoplasm runtime. These
 * functions are not intended to be called directly by programs,
 * but are used internally. They're exposed via a header because
 * the runtime stub needs to know their definitions.
 */

#include <Array.h>

/**
 * Write a memory report to a file in the current directory, using
 * the provided string as the name of the program currently being
 * executed. This function is to be called after all memory is
 * supposed to have been freed. It iterates over all remaining
 * memory and generates a text file containing all of the
 * recorded information about each block, including a hex dump of
 * the data stored in them.
 */
extern void GenerateMemoryReport(const char *);

#endif /* CYTOPLASM_RUNTIME_H */
