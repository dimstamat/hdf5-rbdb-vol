#ifndef __H5RBDB_GROUP_H__
#define __H5RBDB_GROUP_H__


#define PRINT_LATENCIES_GROUP 0
#if PRINT_LATENCIES_GROUP == 1
    #define INIT_COUNTING_GROUP \
            unsigned long long latency_us;  \
            struct timeval begin, end;
#else
    #define INIT_COUNTING_GROUP 
#endif

#if PRINT_LATENCIES_GROUP == 1
    #define START_COUNTING_GROUP gettimeofday(&begin, NULL);
#else 
    #define START_COUNTING_GROUP 
#endif

#if PRINT_LATENCIES_GROUP == 1
    #define STOP_COUNTING_GROUP(...) gettimeofday(&end, NULL); \
        latency_us = 1000000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec); \
        printf(__VA_ARGS__); \
        printf(": %llu\n", latency_us);
#else
    #define STOP_COUNTING_GROUP(...)
#endif


typedef struct group_child_t {
    const char* name;
    struct group_child_t* next;
} group_child_t;

typedef struct group_children_list_t {
    group_child_t* head;
    unsigned size;
} group_children_list_t;



typedef struct H5RBDB_group_t {
    const char *name;
    const char *absolute_name;
    const char *db_name;
    const char *parent_name;
    const char *db_parent_name;
    hbool_t readonly;
    hbool_t creation_order;
    group_children_list_t* children_groups;
    DB *dbp;
    H5RBDB_file_t* file_obj;
    H5VL_loc_params_t *loc_params;
    hid_t group_loc_id;
    hid_t gcpl_id;
    hid_t gapl_id;
    hid_t dxpl_id;
} H5RBDB_group_t;


H5RBDB_group_t * H5RBDB_group_create(H5RBDB_file_t * file_obj, H5VL_loc_params_t loc_params, const char* group_name, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id);
H5RBDB_group_t * H5RBDB_group_open(void *obj, H5I_type_t type, const char *group_name, hid_t gapl_id, hid_t dxpl_id);
herr_t H5RBDB_group_close(H5RBDB_group_t* group);
H5RBDB_group_t* H5RBDB_group_initialize(const char* group_name, const char* file_name);
int H5RBDB_add_child_group(DB** parent_dbp, DB_TXN** tid, const char* absolute_name, hbool_t creation_order);
group_children_list_t* H5RBDB_get_children_groups(DB** g_dbp, DB_TXN** f_tid);

#endif // __H5RBDB_GROUP_H__