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
#include <Db.h>

#include <Memory.h>
#include <Json.h>
#include <Util.h>
#include <Str.h>
#include <Stream.h>
#include <Log.h>

#include <sys/types.h>
#include <dirent.h>

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>

#include "Db/Internal.h"

void
StringArrayFree(Array * arr)
{
    size_t i;

    for (i = 0; i < ArraySize(arr); i++)
    {
        Free(ArrayGet(arr, i));
    }

    ArrayFree(arr);
}

static ssize_t DbComputeSize(HashMap *);

static ssize_t
DbComputeSizeOfValue(JsonValue * val)
{
    MemoryInfo *a;
    ssize_t total = 0;

    size_t i;

    union
    {
        char *str;
        Array *arr;
    } u;

    if (!val)
    {
        return -1;
    }

    a = MemoryInfoGet(val);
    if (a)
    {
        total += MemoryInfoGetSize(a);
    }

    switch (JsonValueType(val))
    {
        case JSON_OBJECT:
            total += DbComputeSize(JsonValueAsObject(val));
            break;
        case JSON_ARRAY:
            u.arr = JsonValueAsArray(val);
            a = MemoryInfoGet(u.arr);

            if (a)
            {
                total += MemoryInfoGetSize(a);
            }

            for (i = 0; i < ArraySize(u.arr); i++)
            {
                total += DbComputeSizeOfValue(ArrayGet(u.arr, i));
            }
            break;
        case JSON_STRING:
            u.str = JsonValueAsString(val);
            a = MemoryInfoGet(u.str);
            if (a)
            {
                total += MemoryInfoGetSize(a);
            }
            break;
        case JSON_NULL:
        case JSON_INTEGER:
        case JSON_FLOAT:
        case JSON_BOOLEAN:
        default:
            /* These don't use any extra heap space */
            break;
    }
    return total;
}

static ssize_t
DbComputeSize(HashMap * json)
{
    char *key;
    JsonValue *val;
    MemoryInfo *a;
    size_t total;

    if (!json)
    {
        return -1;
    }

    total = 0;

    a = MemoryInfoGet(json);
    if (a)
    {
        total += MemoryInfoGetSize(a);
    }

    while (HashMapIterate(json, &key, (void **) &val))
    {
        a = MemoryInfoGet(key);
        if (a)
        {
            total += MemoryInfoGetSize(a);
        }

        total += DbComputeSizeOfValue(val);
    }

    return total;
}

static char *
DbHashKey(Array * args)
{
    size_t i;
    char *str = NULL;

    for (i = 0; i < ArraySize(args); i++)
    {
        char *tmp = StrConcat(2, str, ArrayGet(args, i));

        Free(str);
        str = tmp;
    }

    return str;
}

static void
DbCacheEvict(Db * db)
{
    DbRef *ref = db->leastRecent;
    DbRef *tmp;

    while (ref && db->cacheSize > db->maxCache)
    {
        char *hash;

        JsonFree(ref->json);

        hash = DbHashKey(ref->name);
        HashMapDelete(db->cache, hash);
        Free(hash);

        StringArrayFree(ref->name);

        db->cacheSize -= ref->size;

        if (ref->next)
        {
            ref->next->prev = ref->prev;
        }
        else
        {
            db->mostRecent = ref->prev;
        }

        if (ref->prev)
        {
            ref->prev->next = ref->next;
        }
        else
        {
            db->leastRecent = ref->next;
        }

        tmp = ref->next;
        Free(ref);

        ref = tmp;
    }
}

void
DbMaxCacheSet(Db * db, size_t cache)
{
    if (!db)
    {
        return;
    }

    pthread_mutex_lock(&db->lock);

    db->maxCache = cache;
    if (db->maxCache && !db->cache)
    {
        db->cache = HashMapCreate();
        db->cacheSize = 0;
    }

    DbCacheEvict(db);

    pthread_mutex_unlock(&db->lock);
}

void
DbClose(Db * db)
{
    if (!db)
    {
        return;
    }

    pthread_mutex_lock(&db->lock);
    if (db->close)
    {
        db->close(db);
    }
    DbMaxCacheSet(db, 0);
    DbCacheEvict(db);
    HashMapFree(db->cache);

    pthread_mutex_unlock(&db->lock);
    pthread_mutex_destroy(&db->lock);

    Free(db);
}

DbRef *
DbCreateArgs(Db * db, Array *args)
{
    if (!args || !db || !db->create)
    {
        return NULL;
    }

    return db->create(db, args);
}
DbRef *
DbCreate(Db * db, size_t nArgs,...)
{
    va_list ap;
    Array *args;
    DbRef *ret;

    if (!db || !db->create)
    {
        return NULL;
    }

    va_start(ap, nArgs);
    args = ArrayFromVarArgs(nArgs, ap);
    va_end(ap);

    if (!args)
    {
        return NULL;
    }

    ret = DbCreateArgs(db, args);
    ArrayFree(args);

    return ret;
}

bool
DbDeleteArgs(Db * db, Array *args)
{
    if (!args || !db || !db->delete)
    {
        return false;
    }

    return db->delete(db, args);
}
bool
DbDelete(Db * db, size_t nArgs,...)
{
    va_list ap;
    Array *args;
    bool ret = true;

    if (!db)
    {
        return false;
    }

    va_start(ap, nArgs);
    args = ArrayFromVarArgs(nArgs, ap);
    va_end(ap);

    ret = DbDeleteArgs(db, args);

    ArrayFree(args);
    return ret;
}

DbRef *
DbLockArgs(Db * db, Array *args)
{
    if (!args || !db || !db->lockFunc)
    {
        return NULL;
    }

    return db->lockFunc(db, DB_HINT_WRITE, args);
}
DbRef *
DbLock(Db * db, size_t nArgs,...)
{
    va_list ap;
    Array *args;
    DbRef *ret;

    if (!db)
    {
        return NULL;
    }

    va_start(ap, nArgs);
    args = ArrayFromVarArgs(nArgs, ap);
    va_end(ap);

    if (!args || !db->lockFunc)
    {
        return NULL;
    }

    ret = DbLockArgs(db, args);

    ArrayFree(args);

    return ret;
}

DbRef *
DbLockIntentArgs(Db * db, DbHint hint, Array *args)
{
   if (!db || !args || !db->lockFunc)
   {
        return NULL;
   }
   return db->lockFunc(db, hint, args);
}
DbRef *
DbLockIntent(Db * db, DbHint hint, size_t nArgs,...)
{
    va_list ap;
    Array *args;
    DbRef *ret;

    va_start(ap, nArgs);
    args = ArrayFromVarArgs(nArgs, ap);
    va_end(ap);

    if (!args || !db->lockFunc)
    {
        return NULL;
    }

    ret = DbLockIntentArgs(db, hint, args);

    ArrayFree(args);

    return ret;
}

bool
DbUnlock(Db * db, DbRef * ref)
{
    if (!db || !ref || !db->unlock)
    {
        return false;
    }
    
    return db->unlock(db, ref);
}

bool
DbExistsArgs(Db * db, Array *args)
{
    if (!args || !db || !db->exists)
    {
        return false;
    }

    return db->exists(db, args);
}
bool
DbExists(Db * db, size_t nArgs,...)
{
    va_list ap;
    Array *args;
    bool ret;
    if (!db || !nArgs || !db->exists)
    {
        return false;
    }

    va_start(ap, nArgs);
    args = ArrayFromVarArgs(nArgs, ap);
    va_end(ap);

    if (!args)
    {
        return false;
    }

    ret = DbExistsArgs(db, args);
    ArrayFree(args);

    return ret;
}

Array * 
DbListArgs(Db *db, Array *args)
{
    if (!db || !args || !db->list)
    {
        return NULL;
    }
    return db->list(db, args);
}
Array *
DbList(Db * db, size_t nArgs,...)
{
    Array *result;
    Array *path;
    va_list ap;

    if (!db || !nArgs || !db->list)
    {
        return NULL;
    }

    va_start(ap, nArgs);
    path = ArrayFromVarArgs(nArgs, ap);
    va_end(ap);

    result = DbListArgs(db, path);
    
    ArrayFree(path);
    return result;
}

void
DbListFree(Array * arr)
{
    StringArrayFree(arr);
}

HashMap *
DbJson(DbRef * ref)
{
    return ref ? ref->json : NULL;
}

bool
DbJsonSet(DbRef * ref, HashMap * json)
{
    if (!ref || !json)
    {
        return false;
    }

    JsonFree(ref->json);
    ref->json = JsonDuplicate(json);
    return true;
}
void
DbInit(Db *db)
{
    pthread_mutexattr_t attr;
    if (!db)
    {
        return;
    }

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&db->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    db->mostRecent = NULL;
    db->leastRecent = NULL;
    db->cacheSize = 0;
    db->maxCache = 0;

    if (db->maxCache)
    {
        db->cache = HashMapCreate();
    }
    else
    {
        db->cache = NULL;
    }
}
void
DbRefInit(Db *db, DbRef *ref)
{
    if (!db || !ref)
    {
        return;
    }
    ref->prev = db->mostRecent;
    ref->next = NULL;
    ref->json = NULL;
    ref->name = NULL;

    /* As default values to be overwritten by impls */
    ref->ts = UtilTsMillis();
    ref->size = 0;

    /* TODO: Append the ref to the cache list.
     * I removed it because it broke everything and crashed all the time.
     * My bad! */
}
void
StringArrayAppend(Array *arr, char *str)
{
    if (!arr || !str)
    {
        return;
    }
    ArrayAdd(arr, StrDuplicate(str));
}
