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
#include <HttpRouter.h>

#include <Http.h>
#include <Memory.h>
#include <HashMap.h>
#include <Str.h>

#include <regex.h>
#include <string.h>

#define REG_FLAGS (REG_EXTENDED)
#define REG_MAX_SUB 8

typedef struct RouteNode
{
    HttpRouteFunc *exec;
    HashMap *children;

    regex_t regex;
} RouteNode;

struct HttpRouter
{
    RouteNode *root;
};

static RouteNode *
RouteNodeCreate(char *regex, HttpRouteFunc * exec)
{
    RouteNode *node;

    if (!regex)
    {
        return NULL;
    }

    node = Malloc(sizeof(RouteNode));

    if (!node)
    {
        return NULL;
    }

    node->children = HashMapCreate();
    if (!node->children)
    {
        Free(node);
        return NULL;
    }

    /* Force the regex to match the entire path part exactly. */
    regex = StrConcat(3, "^", regex, "$");
    if (!regex)
    {
        Free(node);
        return NULL;
    }

    if (regcomp(&node->regex, regex, REG_FLAGS) != 0)
    {
        HashMapFree(node->children);
        Free(node);
        Free(regex);
        return NULL;
    }

    node->exec = exec;

    Free(regex);
    return node;
}

static void
RouteNodeFree(RouteNode * node)
{
    char *key;
    RouteNode *val;

    if (!node)
    {
        return;
    }

    while (HashMapIterate(node->children, &key, (void **) &val))
    {
        RouteNodeFree(val);
    }

    HashMapFree(node->children);

    regfree(&node->regex);

    Free(node);
}

HttpRouter *
HttpRouterCreate(void)
{
    HttpRouter *router = Malloc(sizeof(HttpRouter));

    if (!router)
    {
        return NULL;
    }

    router->root = RouteNodeCreate("/", NULL);

    return router;
}

void
HttpRouterFree(HttpRouter * router)
{
    if (!router)
    {
        return;
    }

    RouteNodeFree(router->root);
    Free(router);
}

bool
HttpRouterAdd(HttpRouter * router, char *regPath, HttpRouteFunc * exec)
{
    RouteNode *node;
    char *pathPart;
    char *tmp;

    if (!router || !regPath || !exec)
    {
        return false;
    }

    if (StrEquals(regPath, "/"))
    {
        router->root->exec = exec;
        return true;
    }

    regPath = StrDuplicate(regPath);
    if (!regPath)
    {
        return false;
    }

    tmp = regPath;
    node = router->root;

    while ((pathPart = strtok_r(tmp, "/", &tmp)))
    {
        RouteNode *tNode = HashMapGet(node->children, pathPart);

        if (!tNode)
        {
            tNode = RouteNodeCreate(pathPart, NULL);
            RouteNodeFree(HashMapSet(node->children, pathPart, tNode));
        }

        node = tNode;
    }

    node->exec = exec;

    Free(regPath);

    return true;
}

bool
HttpRouterRoute(HttpRouter * router, char *path, void *args, void **ret)
{
    RouteNode *node;
    char *pathPart;
    char *tmp;
    HttpRouteFunc *exec = NULL;
    Array *matches = NULL;
    size_t i;
    bool retval;

    if (!router || !path)
    {
        return false;
    }

    matches = ArrayCreate();
    if (!matches)
    {
        return false;
    }

    node = router->root;

    if (StrEquals(path, "/"))
    {
        exec = node->exec;
    }
    else
    {
        path = StrDuplicate(path);
        tmp = path;
        while ((pathPart = strtok_r(tmp, "/", &tmp)))
        {
            char *key;
            RouteNode *val = NULL;

            regmatch_t pmatch[REG_MAX_SUB];

            pathPart = HttpUrlDecode(pathPart);

            i = 0;

            while (HashMapIterateReentrant(node->children, &key, (void **) &val, &i))
            {
                if (regexec(&val->regex, pathPart, REG_MAX_SUB, pmatch, 0) == 0)
                {
                    break;
                }

                val = NULL;
            }

            if (!val)
            {
                exec = NULL;
                Free(pathPart);
                break;
            }

            node = val;
            exec = node->exec;

            /* If we want to pass an arg, the match must be in parens */
            if (val->regex.re_nsub)
            {
                /* pmatch[0] is the whole string, not the first
                 * subexpression */
                char *substr;
                regmatch_t cpmatch;

                for (i = 1; i < REG_MAX_SUB; i++)
                {
                    cpmatch = pmatch[i];
                    substr = StrSubstr(pathPart, cpmatch.rm_so, cpmatch.rm_eo);
                    if (pmatch[i].rm_so == -1)
                    {
                        break;
                    }

                    ArrayAdd(matches, substr);
                }
            }
            Free(pathPart);
        }
        Free(path);
    }

    if (!exec)
    {
        retval = false;
        goto finish;
    }

    if (ret)
    {
        *ret = exec(matches, args);
    }
    else
    {
        exec(matches, args);
    }

    retval = true;

finish:
    for (i = 0; i < ArraySize(matches); i++)
    {
        Free(ArrayGet(matches, i));
    }
    ArrayFree(matches);

    return retval;
}
