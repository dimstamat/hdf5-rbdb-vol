#ifndef __H5RBDB_H__ // #define __H5RBDB_H__
#define __H5RBDB_H__

#include <db.h>
#include "db_config.h"
#include "db_int.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"
#include "dbinc/utils.h"
#include "dbinc/retro.h"


typedef struct H5RBDB_dbenv_t {
    const char *dbenv_home;
    const char *db_file;
    DB_ENV *dbenv;
    DB_TXN *tid;
    RETROSPECTION_DATA* bite;
    unsigned is_initialized;
    unsigned open_files;
    unsigned active_txn;
    unsigned in_retrospection;
} H5RBDB_dbenv_t;


typedef enum {
    DB_TYPE_FILE,
    DB_TYPE_GROUP
} H5RBDB_db_type;

typedef struct group_iterate_t{
  hid_t fapl_id; // the DB environment that the iteration is currently in
  hid_t file_id; // the file that the iteration is currently in
  uint level;
  char* linkval;
  unsigned char usetxn;
}group_iterate_t;

typedef enum {DSPACE_NELEM, DSPACE_RANK, DSPACE_SIZE, DSPACE_MAX} dspace_field;


#define DB_KEYS_DB_TYPE "db_type"
#define DB_KEYS_GROUP_CHILDREN "children"
#define DB_KEYS_GROUP_LINKS "links"
#define DB_KEYS_GROUP_MODIFIED "modified"
#define DB_KEYS_ATTR_DTYPE "datatype"
#define DB_KEYS_ATTR_DTYPE_SHARED "datatype_shared"
#define DB_KEYS_ATTR_STORAGE_SIZE "storage_size"
#define DB_KEYS_ATTR_METADATA "mdata_attr"


#define DB_TABLES_TABLE_CATALOG "__table_catalog"

#define FILENAME_DB_ENVIRONMENT "__db_environment"

#define FILE_NAME_SIZE 512
#define LINK_NAME_SIZE 512
#define LINK_VALUE_SIZE 512
#define ATTR_NAME_SIZE 512


#define PRINT_LATENCIES 1

#if PRINT_LATENCIES == 1
    #define INIT_COUNTING \
            long latency_us;  \
            struct timeval begin, end;
#else
    #define INIT_COUNTING 
#endif

#if PRINT_LATENCIES == 1
    #define START_COUNTING gettimeofday(&begin, NULL);
#else 
    #define START_COUNTING 
#endif

#if PRINT_LATENCIES == 1
    #define STOP_COUNTING(...) gettimeofday(&end, NULL); \
        latency_us = 1000000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec); \
        printf(__VA_ARGS__); \
        printf(": %ld\n", latency_us);
#else
    #define STOP_COUNTING(...)
#endif




int H5RBDB_environment_open(DB_ENV** dbenv, const char* dbenv_home);
int H5RBDB_database_open(DB_ENV** dbenv, DB** dbp , DB_TXN** tid, const char* file_name, const char* db_name, H5RBDB_db_type db_type, unsigned char create_db, unsigned char snapshottable);
int H5RBDB_db_handles_database_open(DB_ENV** dbenv, DB** dbp, const char* file_name, const char* db_name);
int H5RBDB_database_get(DB** dbp, DB_TXN** tid, char* key_str, void * value_buf, u_int32_t value_length);
int H5RBDB_database_put(DB** dbp, DB_TXN** tid, char* key_str, void* value_buf, unsigned value_size);
int H5RBDB_database_put_str(DB** dbp, DB_TXN** tid, char* key_str, char* value_str);
int H5RBDB_database_cursor_get_hsize(DB** dbp, DB_TXN** tid, char* key_str, void* value_buf, unsigned size, hsize_t nelements);
int H5RBDB_database_cursor_get(DB** dbp, DB_TXN** tid, char* key_str, void* value_buf, unsigned rec_size, u_int32_t total_size);
int H5RBDB_database_cursor_link_iter_apply(H5RBDB_dbenv_t* dbenv_info, hid_t obj_id, DB** dbp, DB_TXN** tid, char* prefix, 
    unsigned comp_len, H5L_iterate_t apply, H5L_info_t* link_info, char* file_name, void* op_data);
unsigned H5RBDB_database_exists(DB_ENV** dbenv, DB** dbp, DB_TXN** tid, RETROSPECTION_DATA* bite, const char* file_name, const char* db_name);
int H5RBDB_key_exists(DB** dbp, DB_TXN** tid, const char* key_str);
int H5RBDB_put_hid_t(DB* dbp, DB_TXN* tid, char* key_str, hid_t value_id);
void* H5RBDB_get_txn(hid_t loc_id);
int H5RBDB_txn_begin(hid_t loc_id);
int H5RBDB_txn_commit(hid_t loc_id);
u_int32_t H5RBDB_declare_snapshot(hid_t loc_id);
herr_t H5RBDB_as_of_snapshot(hid_t loc_id, u_int32_t snap_id);
herr_t H5RBDB_retro_free(hid_t loc_id);

#endif 
// #define __H5RBDB_H__