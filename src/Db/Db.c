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

    ret = db->create(db, args);
    ArrayFree(args);

    return ret;
}

bool
DbDelete(Db * db, size_t nArgs,...)
{
    (void) nArgs;
    (void) db;
    /* TODO */
    /*va_list ap;
    Array *args;
    char *file;
    char *hash;
    bool ret = true;
    DbRef *ref;

    if (!db)
    {
        return false;
    }

    va_start(ap, nArgs);
    args = ArrayFromVarArgs(nArgs, ap);
    va_end(ap);

    pthread_mutex_lock(&db->lock);

    hash = DbHashKey(args);
    file = DbFileName(db, args);

    ref = HashMapGet(db->cache, hash);
    if (ref)
    {
        HashMapDelete(db->cache, hash);
        JsonFree(ref->json);
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

        if (!db->leastRecent)
        {
            db->leastRecent = db->mostRecent;
        }

        Free(ref);
    }

    Free(hash);

    if (UtilLastModified(file))
    {
        ret = (remove(file) == 0);
    }

    pthread_mutex_unlock(&db->lock);

    ArrayFree(args);
    Free(file);
    return ret;*/
    return false;
}

DbRef *
DbLock(Db * db, size_t nArgs,...)
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

    ret = db->lockFunc(db, args);

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
DbExists(Db * db, size_t nArgs,...)
{
    (void) nArgs;
    (void) db;
    return false;
    /*va_list ap;
    Array *args;
    char *file;
    bool ret;

    va_start(ap, nArgs);
    args = ArrayFromVarArgs(nArgs, ap);
    va_end(ap);

    if (!args)
    {
        return false;
    }

    pthread_mutex_lock(&db->lock);

    file = DbFileName(db, args);
    ret = (UtilLastModified(file) != 0);

    pthread_mutex_unlock(&db->lock);

    Free(file);
    ArrayFree(args);

    return ret;*/
}

Array *
DbList(Db * db, size_t nArgs,...)
{
    (void) db;
    (void) nArgs;
    /* TODO */
    /*Array *result;
    Array *path;
    DIR *files;
    struct dirent *file;
    char *dir;
    va_list ap;

    if (!db || !nArgs)
    {
        return NULL;
    }

    result = ArrayCreate();
    if (!result)
    {
        return NULL;
    }

    va_start(ap, nArgs);
    path = ArrayFromVarArgs(nArgs, ap);
    dir = DbDirName(db, path, 0);

    pthread_mutex_lock(&db->lock);

    files = opendir(dir);
    if (!files)
    {
        ArrayFree(path);
        ArrayFree(result);
        Free(dir);
        pthread_mutex_unlock(&db->lock);
        return NULL;
    }
    while ((file = readdir(files)))
    {
        size_t namlen = strlen(file->d_name);

        if (namlen > 5)
        {
            int nameOffset = namlen - 5;

            if (StrEquals(file->d_name + nameOffset, ".json"))
            {
                file->d_name[nameOffset] = '\0';
                ArrayAdd(result, StrDuplicate(file->d_name));
            }
        }
    }
    closedir(files);

    ArrayFree(path);
    Free(dir);
    pthread_mutex_unlock(&db->lock);

    return result;*/
    return NULL;
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

    if (db->mostRecent)
    {
        db->mostRecent->next = ref;
    }
    if (!db->leastRecent)
    {
        db->leastRecent = ref;
    }
    db->mostRecent = ref;
}
