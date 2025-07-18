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
#ifndef CYTOPLASM_PLATFORM_H
#define CYTOPLASM_PLATFORM_H

/***
 * @Nm Platform
 * @Nd A simple macro header that determines what platform the application
 * is being built for.
 * @Dd September 21, 2024
 */

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #define PLATFORM_WINDOWS

   #ifdef _WIN64
      #define PLATFORM_WIN64
   #else
      #define PLATFORM_WIN32
   #endif
#elif __APPLE__
    #define PLATFORM_DARWIN

    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
        #define PLATFORM_IPHONE
    #elif TARGET_OS_MACCATALYST
        #define PLATFORM_CATALYST
    #elif TARGET_OS_IPHONE
        #define PLATFORM_IPHONE
    #elif TARGET_OS_MAC
        #define PLATFORM_MAC
    #else
    #   error "Unknown Apple platform"
    #endif
#elif __ANDROID__
    #define PLATFORM_ANDROID
#elif __linux__
    #define PLATFORM_LINUX
#elif __unix__
    #define PLATFORM_UNIX
#elif defined(_POSIX_VERSION)
    #define PLATFORM_POSIX
#else
#   error "Unknown compiler"
#endif

#endif /* CYTOPLASM_PLATFORM_H */
