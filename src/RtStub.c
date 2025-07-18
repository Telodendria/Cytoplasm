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
#include <Runtime.h>
#include <Array.h>
#include <HashMap.h>
#include <Stream.h>
#include <Log.h>
#include <Memory.h>
#include <Str.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define ERR_BUF_SIZE 128

/* Specified by POSIX to contain environment variables */
extern char **environ;

/* The linking program is expected to provide Main */
extern int Main(Array *, HashMap *);

/* Functions in the Memory API that are not exported via header */
extern int MemoryRuntimeInit(void);
extern int MemoryRuntimeDestroy(void);

typedef struct MainArgs
{
    Array *args;
    HashMap *env;
    int ret;
} MainArgs;

static void *
MainThread(void *argp)
{
    MainArgs *args = argp;

    args = argp;
    args->ret = Main(args->args, args->env);

    return NULL;
}

int
main(int argc, char **argv)
{
    pthread_t mainThread;
    size_t i;

    char *key;
    char *val;
    char **envp;

    MainArgs args;

    char errBuf[ERR_BUF_SIZE];
    int errLen;

    args.args = NULL;
    args.env = NULL;
    args.ret = EXIT_FAILURE;

    if (!MemoryRuntimeInit())
    {
        errLen = snprintf(errBuf, ERR_BUF_SIZE, "Fatal: Unable to initialize Memory runtime.\n");
        write(STDERR_FILENO, errBuf, errLen);
        goto finish;
    }

    args.args = ArrayCreate();

    if (!args.args)
    {
        errLen = snprintf(errBuf, ERR_BUF_SIZE, "Fatal: Unable to allocate heap memory for program arguments.\n");
        write(STDERR_FILENO, errBuf, errLen);
        goto finish;
    }

    args.env = HashMapCreate();
    if (!args.env)
    {
        errLen = snprintf(errBuf, ERR_BUF_SIZE, "Fatal: Unable to allocate heap memory for program environment.\n");
        write(STDERR_FILENO, errBuf, errLen);
        goto finish;
    }

    for (i = 0; i < (size_t) argc; i++)
    {
        char *arg = StrDuplicate(argv[i]);

        if (!arg || !ArrayAdd(args.args, arg))
        {
            errLen = snprintf(errBuf, ERR_BUF_SIZE, "Fatal: Unable to allocate heap memory for program argument.\n");
            write(STDERR_FILENO, errBuf, errLen);
            goto finish;
        }
    }

    envp = environ;
    while (*envp)
    {
        size_t valInd;

        /* It is unclear whether or not envp strings are writable, so
         * we make our own copy to manipulate it */
        key = StrDuplicate(*envp);

        if (!key)
        {
            errLen = snprintf(errBuf, ERR_BUF_SIZE, "Fatal: Unable to allocate heap memory for program environment variable.\n");
            write(STDERR_FILENO, errBuf, errLen);
            goto finish;
        }

        valInd = strcspn(key, "=");

        key[valInd] = '\0';
        val = key + valInd + 1;
        HashMapSet(args.env, key, StrDuplicate(val));
        Free(key);
        envp++;
    }

    if (pthread_create(&mainThread, NULL, MainThread, &args) != 0)
    {
        errLen = snprintf(errBuf, ERR_BUF_SIZE, "Fatal: Unable to create main thread.\n");
        write(STDERR_FILENO, errBuf, errLen);
        goto finish;
    }

    if (pthread_join(mainThread, NULL) != 0)
    {
        errLen = snprintf(errBuf, ERR_BUF_SIZE, "Fatal: Unable to join main thread.\n");
        write(STDERR_FILENO, errBuf, errLen);
        args.ret = EXIT_FAILURE;
        goto finish;
    }

finish:
    if (args.args)
    {
        for (i = 0; i < ArraySize(args.args); i++)
        {
            Free(ArrayGet(args.args, i));
        }
        ArrayFree(args.args);
    }

    if (args.env)
    {
        while (HashMapIterate(args.env, &key, (void **) &val))
        {
            Free(val);
        }
        HashMapFree(args.env);
    }

    LogConfigFree(LogConfigGlobal());

    StreamClose(StreamStdout());
    StreamClose(StreamStdin());
    StreamClose(StreamStderr());

    GenerateMemoryReport(argc, argv);

    MemoryRuntimeDestroy();

    return args.ret;
}
