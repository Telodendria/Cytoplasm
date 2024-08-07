#include <Memory.h>

#include <Db.h>

#include "Db/Internal.h"

#ifdef EDB_LMDB

typedef struct LMDB {
    Db base; /* The base implementation required to pass */
} LMDB;
Db *
DbOpenLMDB(char *dir, size_t size)
{
    /* TODO */
    LMDB *db;
    if (!dir || !size)
    {
        return NULL;
    }

    db = Malloc(sizeof(*db));
    DbInit(db);

    (void) size;
    (void) dir;
    return db;
}

#else
Db *
DbOpenLMDB(char *dir, size_t size)
{
    /* Unimplemented function */
    (void) size;
    (void) dir;
    return NULL;
}
#endif
