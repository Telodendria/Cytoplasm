#include <Memory.h>
#include <Json.h>
#include <Log.h>
#include <Db.h>

#include "Db/Internal.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef EDB_IMPL

#include <lmdb.h>

typedef struct LMDB {
    Db base; /* The base implementation required to pass */

    MDB_env *environ;
} LMDB;
typedef struct LMDBRef {
    DbRef base;

    /* TODO: LMDB-dependent stuff */
    MDB_txn *transaction;
    MDB_dbi dbi;
} LMDBRef;

/* Helper functions */
static MDB_val
LMDBTranslateKey(Array *key)
{
    MDB_val ret = { 0 };
    char *data = NULL;
    size_t length = 0;
    size_t i;
    if (!key || ArraySize(key) > 255)
    {
        return ret;
    }

    data = Realloc(data, ++length);
    data[0] = ArraySize(key);

    /* Now, let's push every item */
    for (i = 0; i < ArraySize(key); i++)
    {
        char *entry = ArrayGet(key, i);
        size_t offset = length;

        data = Realloc(data, (length += strlen(entry) + 1));
        memcpy(data + offset, entry, strlen(entry));
        data[length - 1] = '\0';
    }

    /* We now have every key */
    ret.mv_size = length;
    ret.mv_data = data;
    return ret;
}
static void
LMDBKillKey(MDB_val key)
{
    if (!key.mv_data || !key.mv_size)
    {
        return;
    }

    Free(key.mv_data);
}
static HashMap *
LMDBDecode(MDB_val val)
{
    FILE *fakefile;
    Stream *fakestream;
    HashMap *ret;
    if (!val.mv_data || !val.mv_size)
    {
        return NULL;
    }

    fakefile = fmemopen(val.mv_data, val.mv_size, "r");
    fakestream = StreamFile(fakefile);
    ret = JsonDecode(fakestream);
    StreamClose(fakestream);

    return ret;
}

static DbRef *
LMDBLock(Db *d, Array *k)
{
    LMDB *db = (LMDB *) d;
    MDB_txn *transaction;
    LMDBRef *ret = NULL;
    MDB_val key, empty_json;
    MDB_dbi dbi;
    int code;
    if (!d || !k)
    {
        return NULL;
    }

    pthread_mutex_lock(&d->lock);
    key = LMDBTranslateKey(k);

    /* create a txn */
    if ((code = mdb_txn_begin(db->environ, NULL, 0, &transaction)) != 0)
    {
        /* Very bad! */
        Log(LOG_ERR,
            "%s: could not begin transaction: %s",
            __func__, mdb_strerror(code)
        );
        pthread_mutex_unlock(&d->lock);
        goto end;
    }
    /* apparently you need to give it a dbi */
    if ((code = mdb_dbi_open(transaction, "db", MDB_CREATE, &dbi)) != 0)
    {
        Log(LOG_ERR,
            "%s: could not get transaction dbi: %s",
            __func__, mdb_strerror(code)
        );
        pthread_mutex_unlock(&d->lock);
        goto end;

    }

    empty_json.mv_size = 0;
    empty_json.mv_data = NULL;
    /* get data from it */
    code = mdb_get(transaction, dbi, &key, &empty_json);
    if (code == MDB_NOTFOUND)
    {
        Log(LOG_ERR, 
            "%s: mdb_get failure: %s", 
            __func__, mdb_strerror(code)
        );
        mdb_txn_abort(transaction);
        mdb_dbi_close(db->environ, dbi);
        goto end;
    }
    else if (code != 0)
    {
        Log(LOG_ERR, 
            "%s: mdb_get failure: %s", 
            __func__, mdb_strerror(code)
        );
        mdb_txn_abort(transaction);
        mdb_dbi_close(db->environ, dbi);
        goto end;
    }

    ret = Malloc(sizeof(*ret));
    DbRefInit(d, (DbRef *) ret);
    /* TODO: Timestamp */
    {
        size_t i;
        ret->base.name = ArrayCreate();
        for (i = 0; i < ArraySize(k); i++)
        {
            char *ent = ArrayGet(k, i);
            StringArrayAppend(ret->base.name, ent);
        }
    }
    ret->base.json = LMDBDecode(empty_json);
    ret->transaction = transaction;
    ret->dbi = dbi;
end:
    LMDBKillKey(key);
    return (DbRef *) ret;
}
static bool
LMDBExists(Db *d, Array *k)
{
    MDB_val key, empty;
    LMDB *db = (LMDB *) d;
    MDB_txn *transaction;
    MDB_dbi dbi;
    int code;
    bool ret = false;
    if (!d || !k)
    {
        return false;
    }

    pthread_mutex_lock(&d->lock);
    key = LMDBTranslateKey(k);

    /* create a txn */
    if ((code = mdb_txn_begin(db->environ, NULL, 0, &transaction)) != 0)
    {
        /* Very bad! */
        Log(LOG_ERR,
            "%s: could not begin transaction: %s",
            __func__, mdb_strerror(code)
        );
        goto end;
    }
    /* apparently you need to give it a dbi */
    if ((code = mdb_dbi_open(transaction, "db", MDB_CREATE, &dbi)) != 0)
    {
        Log(LOG_ERR,
            "%s: could not get transaction dbi: %s",
            __func__, mdb_strerror(code)
        );
        goto end;

    }

    ret = mdb_get(transaction, dbi, &key, &empty) == 0;
    mdb_txn_abort(transaction);
    mdb_dbi_close(db->environ, dbi);

end:
    LMDBKillKey(key);
    pthread_mutex_unlock(&d->lock);
    return ret;
}
static bool
LMDBDelete(Db *d, Array *k)
{
    MDB_val key, empty;
    LMDB *db = (LMDB *) d;
    MDB_txn *transaction;
    MDB_dbi dbi;
    int code;
    bool ret = false;
    if (!d || !k)
    {
        return false;
    }

    pthread_mutex_lock(&d->lock);
    key = LMDBTranslateKey(k);

    /* create a txn */
    if ((code = mdb_txn_begin(db->environ, NULL, 0, &transaction)) != 0)
    {
        /* Very bad! */
        Log(LOG_ERR,
            "%s: could not begin transaction: %s",
            __func__, mdb_strerror(code)
        );
        goto end;
    }
    /* apparently you need to give it a dbi */
    if ((code = mdb_dbi_open(transaction, "db", MDB_CREATE, &dbi)) != 0)
    {
        Log(LOG_ERR,
            "%s: could not get transaction dbi: %s",
            __func__, mdb_strerror(code)
        );
        goto end;

    }

    ret = mdb_del(transaction, dbi, &key, &empty) == 0;
    mdb_txn_commit(transaction);
    mdb_dbi_close(db->environ, dbi);

end:
    LMDBKillKey(key);
    pthread_mutex_unlock(&d->lock);
    return ret;
}

static bool
LMDBUnlock(Db *d, DbRef *r)
{
    LMDBRef *ref = (LMDBRef *) r;
    LMDB *db = (LMDB *) d;
    FILE *fakestream;
    Stream *stream;
    MDB_val key, val;
    bool ret;

    if (!d || !r)
    {
        return false;
    }

    val.mv_data = NULL;
    val.mv_size = 0;
    key = LMDBTranslateKey(r->name);

    fakestream = open_memstream((char **) &val.mv_data, &val.mv_size);
    stream = StreamFile(fakestream);
    JsonEncode(r->json, stream, JSON_DEFAULT);
    StreamFlush(stream);
    StreamClose(stream);

    mdb_put(ref->transaction, ref->dbi, &key, &val, 0);

    mdb_txn_commit(ref->transaction);
    mdb_dbi_close(db->environ, ref->dbi);
    StringArrayFree(ref->base.name);
    JsonFree(ref->base.json);
    Free(ref);

    LMDBKillKey(key);
    if (val.mv_data)
    {
        free(val.mv_data);
    }
    pthread_mutex_unlock(&d->lock);
    return ret;
}
static DbRef *
LMDBCreate(Db *d, Array *k)
{
    LMDB *db = (LMDB *) d;
    MDB_txn *transaction;
    LMDBRef *ret = NULL;
    MDB_val key, empty_json;
    MDB_dbi dbi;
    int code;
    if (!d || !k)
    {
        return NULL;
    }

    pthread_mutex_lock(&d->lock);
    key = LMDBTranslateKey(k);

    /* create a txn */
    if ((code = mdb_txn_begin(db->environ, NULL, 0, &transaction)) != 0)
    {
        /* Very bad! */
        Log(LOG_ERR,
            "%s: could not begin transaction: %s",
            __func__, mdb_strerror(code)
        );
        pthread_mutex_unlock(&d->lock);
        goto end;
    }
    /* apparently you need to give it a dbi */
    if ((code = mdb_dbi_open(transaction, "db", MDB_CREATE, &dbi)) != 0)
    {
        Log(LOG_ERR,
            "%s: could not get transaction dbi: %s",
            __func__, mdb_strerror(code)
        );
        pthread_mutex_unlock(&d->lock);
        goto end;

    }

    empty_json.mv_size = 2;
    empty_json.mv_data = "{}";
    /* put data in it */
    code = mdb_put(transaction, dbi, &key, &empty_json, MDB_NOOVERWRITE);
    if (code == MDB_KEYEXIST)
    {
        mdb_dbi_close(db->environ, dbi);
        mdb_txn_abort(transaction);
        goto end;
    }
    else if (code == MDB_MAP_FULL)
    {
        Log(LOG_ERR, "%s: db is full", __func__);
        mdb_dbi_close(db->environ, dbi);
        mdb_txn_abort(transaction);
        goto end;
    }
    else if (code != 0)
    {
        Log(LOG_ERR, "%s: mdb_put failure: %s", __func__, mdb_strerror(code));
        mdb_dbi_close(db->environ, dbi);
        mdb_txn_abort(transaction);
        goto end;
    }

    ret = Malloc(sizeof(*ret));
    DbRefInit(d, (DbRef *) ret);
    /* TODO: Timestamp */
    {
        size_t i;
        ret->base.name = ArrayCreate();
        for (i = 0; i < ArraySize(k); i++)
        {
            char *ent = ArrayGet(k, i);
            StringArrayAppend(ret->base.name, ent);
        }
    }
    ret->base.json = HashMapCreate();
    ret->transaction = transaction;
    ret->dbi = dbi;
end:
    LMDBKillKey(key);
    return (DbRef *) ret;
}


/* Implementation functions */
static void
LMDBClose(Db *d)
{
    LMDB *db = (LMDB *) d;
    if (!d)
    {
        return;
    }

    mdb_env_close(db->environ);
}

Db *
DbOpenLMDB(char *dir, size_t size)
{
    /* TODO */
    MDB_env *env = NULL;
    int code;
    LMDB *db;
    if (!dir || !size)
    {
        return NULL;
    }

    /* Try initialising LMDB */
    if ((code = mdb_env_create(&env)) != 0)
    {
        Log(LOG_ERR, 
            "%s: could not create LMDB env: %s",
            __func__, mdb_strerror(code)
        );
        return NULL;
    }
    if ((code = mdb_env_set_mapsize(env, size) != 0))
    {
        Log(LOG_ERR, 
            "%s: could not set mapsize to %lu: %s",
            __func__, (unsigned long) size, 
            mdb_strerror(code)
        );
        mdb_env_close(env);
        return NULL;
    }
    mdb_env_set_maxdbs(env, 4);
    if ((code = mdb_env_open(env, dir, 0, 0644)) != 0)
    {
        Log(LOG_ERR, 
            "%s: could not open LMDB env: %s",
            __func__, mdb_strerror(code)
        );
        mdb_env_close(env);
        return NULL;
    }


    db = Malloc(sizeof(*db));
    DbInit((Db *) db);
    db->environ = env;

    db->base.lockFunc = LMDBLock;
    db->base.create = LMDBCreate;
    db->base.unlock = LMDBUnlock;
    db->base.delete = LMDBDelete;
    db->base.exists = LMDBExists;
    db->base.close = LMDBClose;
    db->base.list = NULL; /* TODO: This one is gonna be Fun. */
    
    return (Db *) db;
}

#else
Db *
DbOpenLMDB(char *dir, size_t size)
{
    /* Unimplemented function */
    Log(LOG_ERR, 
        "LMDB support is not enabled. Please compile with --use-lmdb"
    );
    (void) size;
    (void) dir;
    return NULL;
}
#endif
