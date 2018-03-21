#ifndef __H5RBDB_ATTR_H__
#define __H5RBDB_ATTR_H__

#define PRINT_LATENCIES_ATTR 0
#if PRINT_LATENCIES_ATTR == 1
    #define INIT_COUNTING_ATTR \
            unsigned long long latency_us;  \
            struct timeval begin, end;
#else
    #define INIT_COUNTING_ATTR 
#endif

#if PRINT_LATENCIES_ATTR == 1
    #define START_COUNTING_ATTR gettimeofday(&begin, NULL);
#else 
    #define START_COUNTING_ATTR 
#endif

#if PRINT_LATENCIES_ATTR == 1
    #define STOP_COUNTING_ATTR(...) gettimeofday(&end, NULL); \
        latency_us = 1000000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec); \
        printf(__VA_ARGS__); \
        printf(": %llu\n", latency_us);
#else
    #define STOP_COUNTING_ATTR(...)
#endif


typedef struct H5RBDB_attr_metadata_t {
	hsize_t nelem;              /* Number of elements in extent */
    unsigned rank;              /* Number of dimensions */
    H5T_t dtype;
    H5T_shared_t dtype_shared;
    hsize_t storage_size;
    union {
        H5T_atomic_t    atomic; /* an atomic datatype              */
        H5T_compnd_t    compnd; /* a compound datatype (struct)    */
        H5T_enum_t      enumer; /* an enumeration type (enum)       */
        H5T_vlen_t      vlen;   /* a variable-length datatype       */
        H5T_opaque_t    opaque; /* an opaque datatype              */
        H5T_array_t     array;  /* an array datatype                */
    } u;
} H5RBDB_attr_metadata_t;

typedef struct H5RBDB_attr_t {
    const char* name;
    const char* absolute_name;
    DB* dbp;
    H5RBDB_file_t* file_obj;
    H5I_type_t obj_type;    // type of object containing attribute
    void* container_obj;    // the object containing attribute
    H5T_t* type;            // the type of the attribute (string, int, etc.)
    H5RBDB_dataspace_t dataspace;
    const char* dataspace_key;
    hsize_t data_size;
    unsigned malloced_dtype; // we need to know whether we malloced the datatype, or it came from HDF5 lib.
    						 // if we malloced we have to free it when closing attr!
}H5RBDB_attr_t;

typedef struct attr_iterate_t {
	hid_t fapl_id; // the DB environment that the iteration is currently in
	hid_t group_id; // the group that the iteration is currently in
	uint level;
} attr_iterate_t;

#define ATTR_MDATA_BUF_SIZE 512

herr_t H5RBDB_put_attr_metadata(DB** dbp, DB_TXN** tid, H5RBDB_attr_t* attr, H5T_t* dtype);
herr_t H5RBDB_put_attr_metadata_multiple(DB** dbp, DB_TXN** tid, const char* attr_absolute_name, H5RBDB_dataspace_t* dspace, H5T_t* dtype, H5T_shared_t* dtype_shared, hsize_t storage_size);
herr_t H5RBDB_get_attr_metadata(DB** dbp, DB_TXN** tid, H5RBDB_attr_t* attr);
H5RBDB_attr_t* H5RBDB_attr_create(void* obj, H5I_type_t obj_type, const char* attr_name, H5T_t* type, H5S_t* space, hid_t acpl_id, hid_t dxpl_id);
H5RBDB_attr_t* H5RBDB_attr_open(void* obj, H5I_type_t obj_type, const char* attr_name, hid_t dxpl_id);
int H5RBDB_attr_read(H5RBDB_attr_t* attr,  H5T_t *mem_type, void* buf, hid_t dxpl_id);
int H5RBDB_attr_write(H5RBDB_attr_t* attr, H5T_t* mem_type, void* buf, hid_t dxpl_id);
htri_t H5RBDB_attr_exists(void* obj, H5I_type_t obj_type, char* attr_name, hid_t dxpl_id);
hid_t H5RBDB_attr_get_type(H5RBDB_attr_t* attr);
herr_t H5RBDB_attr_close(H5RBDB_attr_t* attr);
herr_t H5RBDB_attr_iterate(void* obj, hid_t obj_id, H5I_type_t obj_type, H5_index_t idx_type, H5_iter_order_t order,  hsize_t * idx, H5A_operator2_t op, void* op_data);
int H5RBDB_database_cursor_attr_iter_apply(H5RBDB_dbenv_t* dbenv_info, DB** dbp, DB_TXN** tid, hid_t obj_id, char* prefix, unsigned comp_len, H5A_operator2_t apply, char* file_name, void* op_data);

static H5RBDB_attr_t* H5RBDB_attr_initialize(void* container_obj, H5I_type_t obj_type, const char* attr_name);
static void H5RBDB_attr_set(H5RBDB_attr_t* attr, H5S_t* space, H5T_t* type);


#endif // __H5RBDB_ATTR_H__