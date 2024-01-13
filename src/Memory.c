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
#include <Memory.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>

#include <unistd.h>
#include <pthread.h>

#ifndef MEMORY_TABLE_CHUNK
#define MEMORY_TABLE_CHUNK 256
#endif

#ifndef MEMORY_HEXDUMP_WIDTH
#define MEMORY_HEXDUMP_WIDTH 16
#endif

struct MemoryInfo
{
    size_t size;
    const char *file;
    int line;
    void *pointer;
};

#define MEM_BOUND_TYPE uint32_t
#define MEM_BOUND 0xDEADBEEF

#define MEM_BOUND_LOWER(p) *((MEM_BOUND_TYPE *) p)
#define MEM_BOUND_UPPER(p, x) *(((MEM_BOUND_TYPE *) (((uint8_t *) p) + x)) + 1)
#define MEM_SIZE_ACTUAL(x) (((x) * sizeof(uint8_t)) + (2 * sizeof(MEM_BOUND_TYPE)))

static pthread_mutex_t lock;
static void (*hook) (MemoryAction, MemoryInfo *, void *) = MemoryDefaultHook;
static void *hookArgs = NULL;

static MemoryInfo **allocations = NULL;
static size_t allocationsSize = 0;
static size_t allocationsLen = 0;

int
MemoryRuntimeInit(void)
{
    pthread_mutexattr_t attr;
    int ret = 0;

    if (pthread_mutexattr_init(&attr) != 0)
    {
        goto finish;
    }

    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    ret = pthread_mutex_init(&lock, &attr);
    pthread_mutexattr_destroy(&attr);

    ret = (ret == 0);

finish:
    return ret;
}

int
MemoryRuntimeDestroy(void)
{
    MemoryFreeAll();
    return pthread_mutex_destroy(&lock) == 0;
}

static size_t
MemoryHash(void *p)
{
    return (((size_t) p) >> 2 * 7) % allocationsSize;
}

static int
MemoryInsert(MemoryInfo * a)
{
    size_t hash;

    if (!allocations)
    {
        allocationsSize = MEMORY_TABLE_CHUNK;
        allocations = calloc(allocationsSize, sizeof(void *));
        if (!allocations)
        {
            return 0;
        }
    }

    /* If the next insertion would cause the table to be at least 3/4
     * full, re-allocate and re-hash. */
    if ((allocationsLen + 1) >= ((allocationsSize * 3) >> 2))
    {
        size_t i;
        size_t tmpAllocationsSize = allocationsSize;
        MemoryInfo **tmpAllocations;

        allocationsSize += MEMORY_TABLE_CHUNK;
        tmpAllocations = calloc(allocationsSize, sizeof(void *));

        if (!tmpAllocations)
        {
            return 0;
        }

        for (i = 0; i < tmpAllocationsSize; i++)
        {
            if (allocations[i])
            {
                hash = MemoryHash(allocations[i]->pointer);

                while (tmpAllocations[hash])
                {
                    hash = (hash + 1) % allocationsSize;
                }

                tmpAllocations[hash] = allocations[i];
            }
        }

        free(allocations);
        allocations = tmpAllocations;
    }

    hash = MemoryHash(a->pointer);

    while (allocations[hash])
    {
        hash = (hash + 1) % allocationsSize;
    }

    allocations[hash] = a;
    allocationsLen++;

    return 1;
}

static void
MemoryDelete(MemoryInfo * a)
{
    size_t hash = MemoryHash(a->pointer);
    size_t count = 0;

    while (count <= allocationsSize)
    {
        if (allocations[hash] && allocations[hash] == a)
        {
            allocations[hash] = NULL;
            allocationsLen--;
            return;
        }
        else
        {
            hash = (hash + 1) % allocationsSize;
            count++;
        }
    }
}

static int
MemoryCheck(MemoryInfo * a)
{
    if (MEM_BOUND_LOWER(a->pointer) != MEM_BOUND ||
        MEM_BOUND_UPPER(a->pointer, a->size - (2 * sizeof(MEM_BOUND_TYPE))) != MEM_BOUND)
    {
        if (hook)
        {
            hook(MEMORY_CORRUPTED, a, hookArgs);
        }
        return 0;
    }
    return 1;
}

void *
MemoryAllocate(size_t size, const char *file, int line)
{
    void *p;
    MemoryInfo *a;

    MemoryIterate(NULL, NULL);

    pthread_mutex_lock(&lock);

    a = malloc(sizeof(MemoryInfo));
    if (!a)
    {
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    p = malloc(MEM_SIZE_ACTUAL(size));
    if (!p)
    {
        free(a);
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    memset(p, 0, MEM_SIZE_ACTUAL(size));
    MEM_BOUND_LOWER(p) = MEM_BOUND;
    MEM_BOUND_UPPER(p, size) = MEM_BOUND;

    a->size = MEM_SIZE_ACTUAL(size);
    a->file = file;
    a->line = line;
    a->pointer = p;

    if (!MemoryInsert(a))
    {
        free(a);
        free(p);
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    if (hook)
    {
        hook(MEMORY_ALLOCATE, a, hookArgs);
    }

    pthread_mutex_unlock(&lock);
    return ((MEM_BOUND_TYPE *) p) + 1;
}

void *
MemoryReallocate(void *p, size_t size, const char *file, int line)
{
    MemoryInfo *a;
    void *new = NULL;

    MemoryIterate(NULL, NULL);

    if (!p)
    {
        return MemoryAllocate(size, file, line);
    }

    a = MemoryInfoGet(p);
    if (a)
    {
        pthread_mutex_lock(&lock);
        new = realloc(a->pointer, MEM_SIZE_ACTUAL(size));
        if (new)
        {
            MemoryDelete(a);
            a->size = MEM_SIZE_ACTUAL(size);
            a->file = file;
            a->line = line;

            a->pointer = new;
            MemoryInsert(a);

            MEM_BOUND_LOWER(a->pointer) = MEM_BOUND;
            MEM_BOUND_UPPER(a->pointer, size) = MEM_BOUND;

            if (hook)
            {
                hook(MEMORY_REALLOCATE, a, hookArgs);
            }

        }
        pthread_mutex_unlock(&lock);
    }
    else if (hook)
    {
        a = malloc(sizeof(MemoryInfo));
        if (a)
        {
            a->size = 0;
            a->file = file;
            a->line = line;
            a->pointer = p;
            hook(MEMORY_BAD_POINTER, a, hookArgs);
            free(a);
        }
    }

    return ((MEM_BOUND_TYPE *) new) + 1;
}

void
MemoryFree(void *p, const char *file, int line)
{
    MemoryInfo *a;

    MemoryIterate(NULL, NULL);

    if (!p)
    {
        return;
    }

    a = MemoryInfoGet(p);
    if (a)
    {
        pthread_mutex_lock(&lock);
        if (hook)
        {
            a->file = file;
            a->line = line;
            hook(MEMORY_FREE, a, hookArgs);
        }
        MemoryDelete(a);
        free(a->pointer);
        free(a);

        pthread_mutex_unlock(&lock);
    }
    else if (hook)
    {
        a = malloc(sizeof(MemoryInfo));
        if (a)
        {
            a->file = file;
            a->line = line;
            a->size = 0;
            a->pointer = p;
            hook(MEMORY_BAD_POINTER, a, hookArgs);
            free(a);
        }
    }
}

size_t
MemoryAllocated(void)
{
    size_t i;
    size_t total = 0;

    pthread_mutex_lock(&lock);

    for (i = 0; i < allocationsSize; i++)
    {
        if (allocations[i])
        {
            total += MemoryInfoGetSize(allocations[i]);
        }
    }

    pthread_mutex_unlock(&lock);

    return total;
}

void
MemoryFreeAll(void)
{
    size_t i;

    pthread_mutex_lock(&lock);

    for (i = 0; i < allocationsSize; i++)
    {
        if (allocations[i])
        {
            free(allocations[i]->pointer);
            free(allocations[i]);
        }
    }

    free(allocations);
    allocations = NULL;
    allocationsSize = 0;
    allocationsLen = 0;

    pthread_mutex_unlock(&lock);
}

MemoryInfo *
MemoryInfoGet(void *p)
{
    size_t hash, count;

    pthread_mutex_lock(&lock);

    p = ((MEM_BOUND_TYPE *) p) - 1;
    hash = MemoryHash(p);

    count = 0;
    while (count <= allocationsSize)
    {
        if (!allocations[hash] || allocations[hash]->pointer != p)
        {
            hash = (hash + 1) % allocationsSize;
            count++;
        }
        else
        {
            MemoryCheck(allocations[hash]);
            pthread_mutex_unlock(&lock);
            return allocations[hash];
        }
    }

    pthread_mutex_unlock(&lock);
    return NULL;
}

size_t
MemoryInfoGetSize(MemoryInfo * a)
{
    if (!a)
    {
        return 0;
    }

    return a->size ? a->size - (2 * sizeof(MEM_BOUND_TYPE)) : 0;
}

const char *
MemoryInfoGetFile(MemoryInfo * a)
{
    if (!a)
    {
        return NULL;
    }

    return a->file;
}

int
MemoryInfoGetLine(MemoryInfo * a)
{
    if (!a)
    {
        return -1;
    }

    return a->line;
}

void *
MemoryInfoGetPointer(MemoryInfo * a)
{
    if (!a)
    {
        return NULL;
    }

    return ((MEM_BOUND_TYPE *) a->pointer) + 1;
}

void
 MemoryIterate(void (*iterFunc) (MemoryInfo *, void *), void *args)
{
    size_t i;

    pthread_mutex_lock(&lock);

    for (i = 0; i < allocationsSize; i++)
    {
        if (allocations[i])
        {
            MemoryCheck(allocations[i]);
            if (iterFunc)
            {
                iterFunc(allocations[i], args);
            }
        }
    }

    pthread_mutex_unlock(&lock);
}

void
 MemoryHook(void (*memHook) (MemoryAction, MemoryInfo *, void *), void *args)
{
    pthread_mutex_lock(&lock);
    hook = memHook;
    hookArgs = args;
    pthread_mutex_unlock(&lock);
}

static size_t
HexPtr(unsigned long ptr, char *out, size_t len)
{
    size_t i = len - 1;
    size_t j = 0;

    do
    {
        out[i] = "0123456789abcdef"[ptr % 16];
        i--;
        ptr /= 16;
    } while (ptr > 0);

    while (++i < len)
    {
        out[j++] = out[i];
    }

    out[j] = '\0';

    return j;
}

void
MemoryDefaultHook(MemoryAction a, MemoryInfo * i, void *args)
{
    char buf[64];
    unsigned long ptr = (unsigned long) MemoryInfoGetPointer(i);

    size_t len = HexPtr(ptr, buf, sizeof(buf));

    (void) args;

    switch (a)
    {
        case MEMORY_BAD_POINTER:
            write(STDERR_FILENO, "Bad pointer: 0x", 15);
            break;
        case MEMORY_CORRUPTED:
            write(STDERR_FILENO, "Corrupted block: 0x", 19);
            break;
        default:
            return;
    }

    write(STDERR_FILENO, buf, len);
    write(STDERR_FILENO, " to 0x", 6);
    len = HexPtr(MemoryInfoGetSize(i), buf, sizeof(buf));
    write(STDERR_FILENO, buf, len);
    write(STDERR_FILENO, " bytes at ", 10);
    write(STDERR_FILENO, MemoryInfoGetFile(i), strlen(MemoryInfoGetFile(i)));
    write(STDERR_FILENO, ":0x", 3);
    len = HexPtr(MemoryInfoGetLine(i), buf, sizeof(buf));
    write(STDERR_FILENO, buf, len);
    write(STDERR_FILENO, "\n", 1);

    raise(SIGSEGV);
}

void
 MemoryHexDump(MemoryInfo * info, void (*printFunc) (size_t, char *, char *, void *), void *args)
{
    char hexBuf[(MEMORY_HEXDUMP_WIDTH * 2) + MEMORY_HEXDUMP_WIDTH + 1];
    char asciiBuf[MEMORY_HEXDUMP_WIDTH + 1];
    size_t pI = 0;
    size_t hI = 0;
    size_t aI = 0;
    const unsigned char *pc;

    if (!info || !printFunc)
    {
        return;
    }

    pc = MemoryInfoGetPointer(info);
    for (pI = 0; pI < MemoryInfoGetSize(info); pI++)
    {
        if (pI > 0 && pI % MEMORY_HEXDUMP_WIDTH == 0)
        {
            hexBuf[hI - 1] = '\0';
            asciiBuf[aI] = '\0';

            printFunc(pI - MEMORY_HEXDUMP_WIDTH, hexBuf, asciiBuf, args);

            snprintf(hexBuf, 4, "%02x ", pc[pI]);
            hI = 3;

            asciiBuf[0] = isprint(pc[pI]) ? pc[pI] : '.';
            asciiBuf[1] = '\0';
            aI = 1;
        }
        else
        {
            asciiBuf[aI] = isprint(pc[pI]) ? pc[pI] : '.';
            aI++;

            snprintf(hexBuf + hI, 4, "%02x ", pc[pI]);
            hI += 3;
        }
    }

    hexBuf[hI] = '\0';
    hI--;

    while (hI < sizeof(hexBuf) - 2)
    {
        hexBuf[hI] = ' ';
        hI++;
    }

    while (aI < sizeof(asciiBuf) - 1)
    {
        asciiBuf[aI] = ' ';
        aI++;
    }

    hexBuf[hI] = '\0';
    asciiBuf[aI] = '\0';

    printFunc(pI - ((pI % MEMORY_HEXDUMP_WIDTH) ?
                  (pI % MEMORY_HEXDUMP_WIDTH) : MEMORY_HEXDUMP_WIDTH),
              hexBuf, asciiBuf, args);
    printFunc(pI, NULL, NULL, args);
}
