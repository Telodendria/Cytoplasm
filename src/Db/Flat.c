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

typedef struct FlatDb {
    Db base;
    char *dir;
    /* Theres not much to do here. */
} FlatDb;
typedef struct FlatDbRef {
    DbRef base;

    Stream *stream;
    int fd;
} FlatDbRef;

static char
DbSanitiseChar(char input)
{
    switch (input)
    {
        case '/':
            return '_';
        case '.':
            return '-';
    }
    return input;
}

static char *
DbDirName(FlatDb * db, Array * args, size_t strip)
{
    size_t i, j;
    char *str = StrConcat(2, db->dir, "/");

    for (i = 0; i < ArraySize(args) - strip; i++)
    {
        char *tmp;
        char *sanitise = StrDuplicate(ArrayGet(args, i));
        for (j = 0; j < strlen(sanitise); j++)
        {
            sanitise[j] = DbSanitiseChar(sanitise[j]);
        }

        tmp = StrConcat(3, str, sanitise, "/");

        Free(str);
        Free(sanitise);

        str = tmp;
    }

    return str;
}
static char *
DbFileName(FlatDb * db, Array * args)
{
    size_t i;
    char *str = StrConcat(2, db->dir, "/");

    for (i = 0; i < ArraySize(args); i++)
    {
        char *tmp;
        char *arg = StrDuplicate(ArrayGet(args, i));
        size_t j = 0;

        /* Sanitize name to prevent directory traversal attacks */
        while (arg[j])
        {
            arg[j] = DbSanitiseChar(arg[j]);
            j++;
        }

        tmp = StrConcat(3, str, arg,
                        (i < ArraySize(args) - 1) ? "/" : ".json");

        Free(arg);
        Free(str);

        str = tmp;
    }

    return str;
}

static DbRef *
FlatLock(Db *d, DbHint hint, Array *dir)
{
    FlatDb *db = (FlatDb *) d;
    FlatDbRef *ref = NULL;
    size_t i;
    char *path = NULL;
    if (!d || !dir)
    {
        return NULL;
    }

    pthread_mutex_lock(&d->lock);
    path = DbFileName(db, dir);
    /* TODO: Caching */
    {
        int fd = open(path, O_RDWR);
        Stream *stream;
        struct flock lock;
        if (fd == -1)
        {
            ref = NULL;
            goto end;
        }

        stream = StreamFd(fd);
        if (!stream)
        {
            ref = NULL;
            goto end;
        }
        
        lock.l_start = 0;
        lock.l_len = 0;
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;

        if (fcntl(fd, F_SETLK, &lock) < 0)
        {
            StreamClose(stream);
            ref = NULL;
            goto end;
        }

        ref = Malloc(sizeof(*ref));
        DbRefInit(d, (DbRef *) ref);
        /* TODO: Hints */
        ref->base.hint = hint;
        ref->base.ts = UtilLastModified(path);
        ref->base.json = JsonDecode(stream);
        ref->stream = stream;
        ref->fd = fd;
        if (!ref->base.json)
        {
            Free(ref);
            StreamClose(stream);
            ref = NULL;
            goto end;
        }
        
        
        ref->base.name = ArrayCreate();
        for (i = 0; i < ArraySize(dir); i++)
        {
            StringArrayAppend(ref->base.name, ArrayGet(dir, i));
        }
    }
end:
    Free(path);
    if (!ref)
    {
        pthread_mutex_unlock(&d->lock);
    }
    return (DbRef *) ref;
}

static bool
FlatUnlock(Db *d, DbRef *r)
{
    FlatDbRef *ref = (FlatDbRef *) r;

    if (!d || !r)
    {
        return false;
    }

    lseek(ref->fd, 0L, SEEK_SET);
    if (ftruncate(ref->fd, 0) < 0)
    {
        pthread_mutex_unlock(&d->lock);
        Log(LOG_ERR, "Failed to truncate file on disk.");
        Log(LOG_ERR, "Error on fd %d: %s", ref->fd, strerror(errno));
        return false;
    }

    JsonEncode(ref->base.json, ref->stream, JSON_DEFAULT);
    StreamClose(ref->stream);

    JsonFree(ref->base.json);
    StringArrayFree(ref->base.name);
    Free(ref);

    pthread_mutex_unlock(&d->lock);
    return true;
}
static DbRef *
FlatCreate(Db *d, Array *dir)
{
    FlatDb *db = (FlatDb *) d;
    char *path, *dirPath;
    Stream *fp;
    DbRef *ret;

    if (!d || !dir)
    {
        return NULL;
    }
    pthread_mutex_lock(&d->lock);

    path = DbFileName(db, dir);
    if (UtilLastModified(path))
    {
        Free(path);
        pthread_mutex_unlock(&d->lock);
        return NULL;
    }

    dirPath = DbDirName(db, dir, 1);
    if (UtilMkdir(dirPath, 0750) < 0)
    {
        Free(path);
        Free(dirPath);
        pthread_mutex_unlock(&d->lock);
        return NULL;
    }
    Free(dirPath);

    fp = StreamOpen(path, "w");
    Free(path);
    if (!fp)
    {
        pthread_mutex_unlock(&d->lock);
        return NULL;
    }

    StreamPuts(fp, "{}");
    StreamClose(fp);

    /* FlatLock() will lock again for us */
    pthread_mutex_unlock(&d->lock);
    ret = FlatLock(d, DB_HINT_WRITE, dir);

    return ret;
}

static bool
FlatDelete(Db *d, Array *dir)
{
    bool ret = false;
    char *file;
    FlatDb *db = (FlatDb *) d;
    if (!d || !dir)
    {
        return false;
    }

    pthread_mutex_lock(&d->lock);
    file = DbFileName(db, dir);

    /* TODO: Unlink the entry from the linkedlist */
    if (UtilLastModified(file))
    {
        ret = remove(file) == 0;
    }

    Free(file);
    pthread_mutex_unlock(&d->lock);
    return ret;
}

static Array *
FlatList(Db *d, Array *dir)
{
    FlatDb *db = (FlatDb *) d;
    struct dirent *file;
    Array *ret;
    DIR *files;
    char *path;

    if (!d || !dir)
    {
        return NULL;
    }

    pthread_mutex_lock(&d->lock);

    path = DbDirName(db, dir, 0);
    files = opendir(path);
    if (!files)
    {
        Free(path);
        pthread_mutex_unlock(&d->lock);
        return NULL;
    }

    ret = ArrayCreate();
    while ((file = readdir(files)))
    {
        size_t namlen = strlen(file->d_name);

        if (namlen > 5)
        {
            int nameOffset = namlen - 5;

            if (StrEquals(file->d_name + nameOffset, ".json"))
            {
                file->d_name[nameOffset] = '\0';
                StringArrayAppend(ret, file->d_name);
            }
        }
    }
    closedir(files);
    Free(path);

    pthread_mutex_unlock(&d->lock);
    return ret;
}
static bool
FlatExists(Db *d, Array *dir)
{
    FlatDb *db = (FlatDb *) d;
    char *path;
    bool ret;
    if (!d || !dir)
    {
        return NULL;
    }

    pthread_mutex_lock(&d->lock);

    path = DbFileName(db, dir);
    ret = UtilLastModified(path) != 0;
    Free(path);

    pthread_mutex_unlock(&d->lock);

    return ret;
}

Db *
DbOpen(char *dir, size_t cache)
{
    FlatDb *db;
    if (!dir)
    {
        return NULL;
    }
    db = Malloc(sizeof(*db));
    DbInit((Db *) db);
    db->dir = dir;
    db->base.cacheSize = cache;

    db->base.lockFunc = FlatLock;
    db->base.unlock = FlatUnlock;
    db->base.create = FlatCreate;
    db->base.delete = FlatDelete;
    db->base.exists = FlatExists;
    db->base.list   = FlatList;
    db->base.close = NULL;

    return (Db *) db;
}
