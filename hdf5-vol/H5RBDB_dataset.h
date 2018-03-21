#ifndef __H5RBDB_DATASET_H__
#define __H5RBDB_DATASET_H__


#define PRINT_LATENCIES_DATASET 0

#if PRINT_LATENCIES_DATASET == 1
    #define INIT_COUNTING_DATASET \
            unsigned long long latency_us;  \
            struct timeval begin, end;
#else
    #define INIT_COUNTING_DATASET 
#endif

#if PRINT_LATENCIES_DATASET == 1
    #define START_COUNTING_DATASET gettimeofday(&begin, NULL);
#else 
    #define START_COUNTING_DATASET 
#endif

#if PRINT_LATENCIES_DATASET == 1
    #define STOP_COUNTING_DATASET(...) gettimeofday(&end, NULL); \
        latency_us = 1000000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec); \
        printf(__VA_ARGS__); \
        printf(": %llu\n", latency_us);
#else
    #define STOP_COUNTING_DATASET(...)
#endif


typedef struct H5RBDB_dataspace_t {
    hsize_t nelem;              /* Number of elements in extent */
    unsigned rank;              /* Number of dimensions */
    hsize_t *size;              /* Current size of the dimensions */
    hsize_t *max;               /* Maximum size of the dimensions */
}H5RBDB_dataspace_t;

typedef struct H5RBDB_dataspace_nums_t {
    hsize_t nelem;              /* Number of elements in extent */
    unsigned rank;              /* Number of dimensions */
} H5RBDB_dataspace_nums_t ;

typedef struct H5RBDB_dataset_metadata_t {
    H5RBDB_dataspace_nums_t dspace_nums;
    hsize_t *size;              /* Current size of the dimensions */
    hsize_t *max;               /* Maximum size of the dimensions */
    hid_t type_id;
    hid_t dcpl_id;
}H5RBDB_dataset_metadata_t;

typedef struct H5RBDB_dataset_data_t {
    unsigned size;
    void* buf;
}H5RBDB_dataset_data_t;

typedef struct H5RBDB_dataset_t {
    const char *name;
    const char *absolute_name;
    const char *metadata_key;
    const char *data_key;
    const char *dataspace_key;
    const char *parent_name;
    const char *parent_db_name;
    DB *dbp;
    H5RBDB_file_t* file_obj;
    H5VL_loc_params_t *loc_params;
    hid_t dataset_loc_id;
    H5RBDB_dataspace_t dataspace;
    hid_t type_id;
    hid_t dcpl_id;
    hid_t dapl_id;
    hid_t dxpl_id;
} H5RBDB_dataset_t;


H5RBDB_dataset_t * H5RBDB_dataset_create(H5RBDB_file_t* file_obj, const char* name, hid_t space_id, hid_t type_id, hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id);
H5RBDB_dataset_t * H5RBDB_dataset_open(H5RBDB_file_t* file, const char* name, hid_t dapl_id, hid_t dxpl_id);
herr_t H5RBDB_dataset_write(H5RBDB_dataset_t* dataset, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t dxpl_id, const void* buf);
herr_t H5RBDB_dataset_read(H5RBDB_dataset_t* dataset, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t plist_id, void* buf);
H5RBDB_dataset_t* H5RBDB_dataset_initialize(const char* dataset_name, const char* file_name);
void H5RBDB_put_dataspace(DB** dbp, DB_TXN** tid, dspace_field field, const char* key, void* value);
void H5RBDB_put_dataspace_nums(DB** dbp, DB_TXN** tid, const char* key, H5RBDB_dataspace_nums_t* value);
void H5RBDB_put_dataspace_multiple(DB** dbp, DB_TXN** tid, dspace_field field, const char* key, void* value, unsigned rank);
void H5RBDB_get_dataspace(DB** dbp, DB_TXN** tid, dspace_field field, const char* key, void* value);
void H5RBDB_get_dataspace_nums(DB** dbp, DB_TXN** tid, const char* key, void* value);
void* H5RBDB_get_dataspace_multiple(DB** dbp, DB_TXN** tid, dspace_field field, const char* key, unsigned rank);

#endif // __H5RBDB_DATASET_H__
