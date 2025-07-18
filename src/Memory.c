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

#define MEMORY_FILE_SIZE 256

#define MEM_BOUND_TYPE uint64_t
#define MEM_BOUND 0xDEADBEEFBEEFDEAD
#define MEM_MAGIC 0xDEADBEEFDEADBEEF

struct MemoryInfo
{
    uint64_t magic;

    size_t size;
    size_t unalignedSize;
    char file[MEMORY_FILE_SIZE];
    int line;
    void *pointer;

    MemoryInfo *prev;
    MemoryInfo *next;

    MEM_BOUND_TYPE leftBoundary;
};

#define MEM_SIZE_ACTUAL(x) (MemoryAlignBoundary((x) * sizeof(uint8_t)) + sizeof(MEM_BOUND_TYPE))
#define MEM_START_BOUNDARY(info) (info->leftBoundary)
#define MEM_END_BOUNDARY(info) (*(((MEM_BOUND_TYPE *) (((uint8_t *) info->pointer) + info->size)) - 1))


static pthread_mutex_t lock;
static void (*hook) (MemoryAction, MemoryInfo *, void *) = MemoryDefaultHook;
static void *hookArgs = NULL;

static size_t allocationsLen = 0;

static MemoryInfo *allocationTail = NULL;

/* Simple range of "plausible" boundaries for heap, serving as an extra 
 * check */
static void *heapStart, *heapEnd;

static size_t MemoryAlignBoundary(size_t size)
{
    size_t boundSize = sizeof(MEM_BOUND_TYPE);
    size_t remainder = size % boundSize;
    size_t closest   = size / boundSize + !!remainder;

    return closest * boundSize;
}

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
    heapStart = NULL;
    heapEnd = NULL;

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

static int
MemoryInsert(MemoryInfo * a)
{
    if (allocationTail)
    {
        allocationTail->next = a;
    }
    a->next = NULL;
    a->prev = allocationTail;
    a->magic = MEM_MAGIC;

    if (!heapStart || heapStart > (void *) a)
    {
        heapStart = a;
    }
    if (!heapEnd || heapEnd < (void *) a)
    {
        heapEnd = a;
    }

    allocationTail = a;
    allocationsLen++;

    return 1;
}

static void
MemoryDelete(MemoryInfo * a)
{
    MemoryInfo *aPrev = a->prev;
    MemoryInfo *aNext = a->next;

    if (aPrev)
    {
        aPrev->next = aNext;
    }
    if (aNext)
    {
        aNext->prev = aPrev;
    }

    if (a == allocationTail)
    {
        allocationTail = aPrev;
    }

    a->magic = ~MEM_MAGIC;
}

static int
MemoryCheck(MemoryInfo * a)
{
    if (MEM_START_BOUNDARY(a) != MEM_BOUND || 
        a->magic != MEM_MAGIC ||
        MEM_END_BOUNDARY(a) != MEM_BOUND)
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

    //MemoryIterate(NULL, NULL);

    pthread_mutex_lock(&lock);

    a = malloc(sizeof(MemoryInfo) + MEM_SIZE_ACTUAL(size));
    if (!a)
    {
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    p = a + 1;

    memset(p, 0, MEM_SIZE_ACTUAL(size));

    a->size = MEM_SIZE_ACTUAL(size);
    a->unalignedSize = size;
    strncpy(a->file, file, MEMORY_FILE_SIZE - 1);
    a->line = line;
    a->pointer = p;
    MEM_START_BOUNDARY(a) = MEM_BOUND;
    MEM_END_BOUNDARY(a) = MEM_BOUND;

    if (!MemoryInsert(a))
    {
        free(a);
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    if (hook)
    {
        hook(MEMORY_ALLOCATE, a, hookArgs);
    }

    pthread_mutex_unlock(&lock);
    return p;
}

void *
MemoryReallocate(void *p, size_t size, const char *file, int line)
{
    MemoryInfo *a;
    void *new = NULL;

    //MemoryIterate(NULL, NULL);

    if (!p)
    {
        return MemoryAllocate(size, file, line);
    }

    a = MemoryInfoGet(p);
    if (a)
    {
        pthread_mutex_lock(&lock);
        MemoryDelete(a);
        new = realloc(a, sizeof(MemoryInfo) + MEM_SIZE_ACTUAL(size));
        if (new)
        {
            a = new;
            a->unalignedSize = size;
            a->size = MEM_SIZE_ACTUAL(size);
            strncpy(a->file, file, MEMORY_FILE_SIZE - 1);
            a->line = line;
            a->magic = MEM_MAGIC;

            a->pointer = a + 1;
            MemoryInsert(a);

            MEM_START_BOUNDARY(a) = MEM_BOUND;
            MEM_END_BOUNDARY(a) = MEM_BOUND;

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
            strncpy(a->file, file, MEMORY_FILE_SIZE - 1);
            a->line = line;
            a->pointer = p;
            hook(MEMORY_BAD_POINTER, a, hookArgs);
            free(a);
        }
    }

    return a->pointer;
}

void
MemoryFree(void *p, const char *file, int line)
{
    MemoryInfo *a;

    //MemoryIterate(NULL, NULL);

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
            strncpy(a->file, file, MEMORY_FILE_SIZE - 1);
            a->line = line;
            hook(MEMORY_FREE, a, hookArgs);
        }
        MemoryDelete(a);
        free(a);

        pthread_mutex_unlock(&lock);
    }
    else if (hook)
    {
        a = malloc(sizeof(MemoryInfo));
        if (a)
        {
            strncpy(a->file, file, MEMORY_FILE_SIZE - 1);
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
    size_t total = 0;
    MemoryInfo *cur;

    pthread_mutex_lock(&lock);

    /* TODO */
    for (cur = allocationTail; cur; cur = cur->prev)
    {
        total += MemoryInfoGetSize(cur);
    }

    pthread_mutex_unlock(&lock);

    return total;
}

void
MemoryFreeAll(void)
{
    MemoryInfo *cur;
    MemoryInfo *prev;

    pthread_mutex_lock(&lock);

    /* TODO */
    for (cur = allocationTail; cur; cur = prev)
    {
        prev = cur->prev;
        free(cur);
    }

    allocationsLen = 0;

    pthread_mutex_unlock(&lock);
}

MemoryInfo *
MemoryInfoGet(void *po)
{
    void *p = po;

    pthread_mutex_lock(&lock);

    p = ((MemoryInfo *) p) - 1;
    if (p < heapStart || p > heapEnd)
    {
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    if (((MemoryInfo *)p)->magic != MEM_MAGIC)
    {
        p = NULL;
    }

    pthread_mutex_unlock(&lock);
    return p;
}

size_t
MemoryInfoGetSize(MemoryInfo * a)
{
    if (!a)
    {
        return 0;
    }

    return a->size ? a->unalignedSize : 0;
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

    return a->pointer;
}

void
MemoryIterate(void (*iterFunc) (MemoryInfo *, void *), void *args)
{
    MemoryInfo *cur;

    pthread_mutex_lock(&lock);

    for (cur = allocationTail; cur; cur = cur->prev)
    {
        MemoryCheck(cur);
        if (iterFunc)
        {
            iterFunc(cur, args);
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
