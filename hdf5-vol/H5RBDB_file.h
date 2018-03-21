#ifndef __H5RBDB_FILE_H__
#define __H5RBDB_FILE_H__

#define PRINT_LATENCIES_FILE 1
#if PRINT_LATENCIES_FILE == 1
    #define INIT_COUNTING_FILE \
            unsigned long long latency_us;  \
            struct timeval begin, end;
#else
    #define INIT_COUNTING_FILE 
#endif

#if PRINT_LATENCIES_FILE == 1
    #define START_COUNTING_FILE gettimeofday(&begin, NULL);
#else 
    #define START_COUNTING_FILE 
#endif

#if PRINT_LATENCIES_FILE == 1
    #define STOP_COUNTING_FILE(...) gettimeofday(&end, NULL); \
        latency_us = 1000000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec); \
        printf(__VA_ARGS__); \
        printf(": %llu\n", latency_us);
#else
    #define STOP_COUNTING_FILE(...)
#endif


typedef struct H5RBDB_file_t {
    char *name;
    DB *dbp;
    DB *root_dbp; // keep the root db open!
    DB* db_handles_dbp; // the HASH DB storing the db handles of the opened DBs
    char *db_handles_dbname;
    H5RBDB_dbenv_t * dbenv_info;
    unsigned dbenv_info_initialized;
    unsigned flags;
    hid_t file_id;
    hid_t fcpl_id;
    hid_t fapl_id;
    hid_t dxpl_id;
} H5RBDB_file_t;

//deprecated
//H5RBDB_dbenv_file_t * H5RBDB_get_env_file(DB_ENV** dbenv, DB_TXN** tid, const char* env_name);
H5RBDB_file_t * H5RBDB_file_create(const char* name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id);
H5RBDB_file_t * H5RBDB_file_open(const char* name, unsigned flags, hid_t fapl_id, hid_t dxpl_id);
int H5RBDB_file_close(H5RBDB_file_t* file, hid_t dxpl_id);

#endif // __H5RBDB_FILE_H__
