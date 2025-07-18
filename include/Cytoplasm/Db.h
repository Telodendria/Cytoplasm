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
#ifndef CYTOPLASM_DB_H
#define CYTOPLASM_DB_H

/***
 * @Nm Db
 * @Nd A minimal flat-file database with mutex locking and cache.
 * @Dd April 27 2023
 * @Xr Json
 *
 * Cytoplasm operates on a flat-file database instead of a
 * traditional relational database. This greatly simplifies the
 * persistent storage code, and creates a relatively basic API,
 * described here.
 */

#include <stddef.h>
#include <stdbool.h>

#include "HashMap.h"
#include "Array.h"

/**
 * All functions in this API operate on a database structure that is
 * opaque to the caller.
 */
typedef struct Db Db;

/**
 * Some "hints" for the database backend for operations. 
 * Hints are a way for the program to declare what to except of it
 * (and the program MUST adhere to these hints, but the backend 
 * MAY adhere).
 */
typedef enum DbHint {
    DB_HINT_READONLY,   /* The database reference is treated as read-only */
    DB_HINT_WRITE       /* The database reference is treated as read/write */
} DbHint;

/**
 * When an object is locked, a reference is returned. This reference
 * is owned by the current thread, and the database is inaccessible to
 * other threads until all references have been returned to the
 * database.
 * .Pp
 * This reference is opaque, but can be manipulated by the functions
 * defined here.
 */
typedef struct DbRef DbRef;

/**
 * Open a data directory. This function takes a path to open, and a
 * cache size in bytes. If the cache size is 0, then caching is
 * disabled and objects are loaded off the disk every time they are
 * locked. Otherwise, objects are stored in the cache, and they are
 * evicted in a least-recently-used manner.
 */
extern Db * DbOpen(char *, size_t);

/**
 * Open a LMDB data directory. This function is similar to 
 * .Fn DbOpen ,
 * but works with a LMDB-based backend, with the second argument 
 * being the maximum filesize. This function fails with NULL if ever 
 * called without LMDB enabled at compiletime.
 */
extern Db * DbOpenLMDB(char *, size_t);

/**
 * Close the database. This function will flush anything in the cache
 * to the disk, and then close the data directory. It assumes that
 * all references have been unlocked. If a reference has not been
 * unlocked, undefined behavior results.
 */
extern void DbClose(Db *);

/**
 * Set the maximum cache size allowed before
 * .Nm
 * starts evicting old objects. If this is set to 0, everything in the
 * cache is immediately evicted and caching is disabled. If the
 * database was opened with a cache size of 0, setting this will
 * initialize the cache, and subsequent calls to
 * .Fn DbLock
 * will begin caching objects.
 */
extern void DbMaxCacheSet(Db *, size_t);

/**
 * Create a new object in the database with the specified name. This
 * function will fail if the object already exists in the database. It
 * takes a variable number of C strings, with the exact number being
 * specified by the second parameter. These C strings are used to
 * generate a filesystem path at which to store the object. These paths
 * ensure each object is uniquely identifiable, and provides semantic
 * meaning to an object.
 */
extern DbRef * DbCreate(Db *, size_t,...);

/** 
 * Behaves like 
 * .Fn DbCreate ,
 * but with an array of strings instead of variadic arguments. 
 */
extern DbRef * DbCreateArgs(Db *, Array *);

/**
 * Lock an existing object in the database. This function will fail
 * if the object does not exist. It takes a variable number of C
 * strings, with the exact number being specified by the second
 * parameter. These C strings are used to generate the filesystem path
 * at which to load the object. These paths ensure each object is
 * uniquely identifiable, and provides semantic meaning to an object.
 */
extern DbRef * DbLock(Db *, size_t,...);

/** 
 * Behaves like 
 * .Fn DbLock ,
 * but with an array of strings instead of variadic arguments. 
 */
extern DbRef * DbLockArgs(Db *, Array *);

/**
 * Behaves like
 * .Fn DbLock ,
 * but with hints on the reference itself, as 
 * .Fn DbLock 
 * itself is read/write.
 */
extern DbRef * DbLockIntent(Db *, DbHint, size_t,...);

/** 
 * Behaves like 
 * .Fn DbLockIntent ,
 * but with an array of strings instead of variadic arguments.
 */
extern DbRef * DbLockIntentArgs(Db *, DbHint, Array *);

/**
 * Immediately and permanently remove an object from the database.
 * This function assumes the object is not locked, otherwise undefined
 * behavior will result.
 */
extern bool DbDelete(Db *, size_t,...);

/** 
 * Behaves like 
 * .Fn DbDelete ,
 * but with an array of strings instead of variadic arguments. 
 */
extern bool DbDeleteArgs(Db *, Array *);

/**
 * Unlock an object and return it back to the database. This function
 * immediately syncs the object to the filesystem. The cache is a
 * read cache; writes are always immediate to ensure data integrity in
 * the event of a system failure.
 */
extern bool DbUnlock(Db *, DbRef *);

/**
 * Check the existence of the given database object in a more efficient
 * manner than attempting to lock it with
 * .Fn DbLock .
 * This function does not lock the object, nor does it load it into
 * memory if it exists.
 */
extern bool DbExists(Db *, size_t,...);

/** 
 * Behaves like 
 * .Fn DbExists ,
 * but with an array of strings instead of variadic arguments.
 */
extern bool DbExistsArgs(Db *, Array *);

/**
 * List all of the objects at a given path. Unlike the other varargs
 * functions, this one does not take a path to a specific object; it
 * takes a directory to be iterated, where each path part is its own
 * C string. Note that the resulting list only contains the objects
 * in the specified directory, it does not list any subdirectories.
 * .Pp
 * The array returned is an array of C strings containing the object
 * name.
 */
extern Array * DbList(Db *, size_t,...);

/** 
 * Behaves like 
 * .Fn DbList ,
 * but with an array of strings instead of variadic arguments.
 */
extern Array * DbListArgs(Db *, Array *);

/**
 * Free the list returned by 
 * .Fn DbListFree .
 */
extern void DbListFree(Array *);

/**
 * Convert a database reference into JSON that can be manipulated.
 * At this time, the database actually stores objects as JSON on the
 * disk, so this function just returns an internal pointer, but in the
 * future it may have to be generated by decompressing a binary blob,
 * or something of that nature.
 */
extern HashMap * DbJson(DbRef *);

/**
 * Free the existing JSON associated with the given reference, and
 * replace it with new JSON. This is more efficient than duplicating
 * a separate object into the database reference.
 */
extern bool DbJsonSet(DbRef *, HashMap *);

#endif
