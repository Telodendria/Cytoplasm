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
#ifndef CYTOPLASM_DB_INTERNAL_H
#define CYTOPLASM_DB_INTERNAL_H

#include <HashMap.h>
#include <Db.h>

#include <pthread.h>

/* TODO: Define the base structures to define a database internally. 
 * All "implementations" shall have them as a first element, so that 
 * basic, general functions can work on them properly. */

/* The base structure of a database */
struct Db
{
    pthread_mutex_t lock;

    size_t cacheSize;
    size_t maxCache;
    HashMap *cache;

    /*
     * The cache uses a double linked list (see DbRef
     * below) to know which objects are most and least
     * recently used. The following diagram helps me
     * know what way all the pointers go, because it
     * can get very confusing sometimes. For example,
     * there's nothing stopping "next" from pointing to
     * least recent, and "prev" from pointing to most
     * recent, so hopefully this clarifies the pointer
     * terminology used when dealing with the linked
     * list:
     *
     *         mostRecent            leastRecent
     *             |   prev       prev   |   prev
     *           +---+ ---> +---+ ---> +---+ ---> NULL
     *           |ref|      |ref|      |ref|
     * NULL <--- +---+ <--- +---+ <--- +---+
     *      next       next       next
     */
    DbRef *mostRecent;
    DbRef *leastRecent;

    /* Functions for implementation-specific operations
     * (opening a ref, closing a db, removing an entry, ...) */
    DbRef * (*lockFunc)(Db *, Array *);
    DbRef * (*create)(Db *, Array *);
    Array * (*list)(Db *, Array *);
    bool (*unlock)(Db *, DbRef *);
    bool (*delete)(Db *, Array *);
    bool (*exists)(Db *, Array *);
    void (*close)(Db *);

    /* Implementation-specific constructs */
};

struct DbRef
{
    HashMap *json;

    uint64_t ts;
    size_t size;

    Array *name;

    DbRef *prev;
    DbRef *next;

    /* TODO: Functions for implementation-specific operations */

    /* Implementation-specific constructs */
};

extern void DbInit(Db *);
extern void DbRefInit(Db *, DbRef *);
extern void StringArrayFree(Array *);
extern void StringArrayAppend(Array *, char *);

#endif
