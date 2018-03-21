#ifndef __H5RBDB_LINK_H__
#define __H5RBDB_LINK_H__

#define PRINT_LATENCIES_LINK 0
#if PRINT_LATENCIES_LINK == 1
    #define INIT_COUNTING_LINK \
            unsigned long long latency_us;  \
            struct timeval begin, end;
#else
    #define INIT_COUNTING_LINK 
#endif

#if PRINT_LATENCIES_LINK == 1
    #define START_COUNTING_LINK gettimeofday(&begin, NULL);
#else 
    #define START_COUNTING_LINK 
#endif

#if PRINT_LATENCIES_LINK == 1
    #define STOP_COUNTING_LINK(...) gettimeofday(&end, NULL); \
        latency_us = 1000000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec); \
        printf(__VA_ARGS__); \
        printf(": %llu\n", latency_us);
#else
    #define STOP_COUNTING_LINK(...)
#endif

typedef struct H5RBDB_link_t {
    const char* name;
    const char* target;
}H5RBDB_link_t;


herr_t H5RBDB_link_create(H5RBDB_group_t* group, const char* link_name, const char* target_name, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id);
htri_t H5RBDB_link_exists(void* obj, const char* name, hid_t lapl, hid_t dxpl);
int H5RBDB_link_getval(H5RBDB_group_t* group, char* link_name, void* buf, size_t size, hid_t lapl_id, hid_t dxpl_id);
herr_t H5RBDB_link_iterate(H5RBDB_group_t *group_obj, hid_t file_id, hid_t group_id, hbool_t recursive, 
                   H5_index_t idx_type, H5_iter_order_t order, hsize_t *idx_p, 
                   H5L_iterate_t op, void *op_data, hid_t dxpl_id);

#endif // __H5RBDB_LINK_H__