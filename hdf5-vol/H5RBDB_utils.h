#ifndef __H5RBDB_UTILS_H__
#define __H5RBDB_UTILS_H__

#define PRINT_LATENCIES_UTILS 0

#if PRINT_LATENCIES_UTILS == 1
    #define INIT_COUNTING_UTILS \
            unsigned long long latency_us;  \
            struct timeval begin, end;
#else
    #define INIT_COUNTING_UTILS 
#endif

#if PRINT_LATENCIES_UTILS == 1
    #define START_COUNTING_UTILS gettimeofday(&begin, NULL);
#else 
    #define START_COUNTING_UTILS 
#endif

#if PRINT_LATENCIES_UTILS == 1
    #define STOP_COUNTING_UTILS(...) gettimeofday(&end, NULL); \
        latency_us = 1000000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec); \
        printf(__VA_ARGS__); \
        printf(": %llu\n", latency_us);
#else
    #define STOP_COUNTING_UTILS(...)
#endif


const char* H5RBDB_name_fix_slashes(const char* absolute_name);
int H5RBDB_get_name_start_pos(const char* absolute_name);
const char* H5RBDB_get_parent_name_from_absolute(const char* absolute_name);
const char* H5RBDB_get_name_from_absolute(const char* absolute_name);

char* H5RBDB_childname_add_prefix(const char* group_name);
char* H5RBDB_childname_timestamp_add_prefix(unsigned long timestamp);
char* H5RBDB_filename_add_prefix(const char* src);
char* H5RBDB_groupname_add_prefix(const char* src, const char* filename);
char* H5RBDB_datasetname_add_prefix(const char* src, const char* type);
char* H5RBDB_linkname_add_prefix(const char* src);
char* H5RBDB_linkname_clear_prefix(const char* link_db_name);
char* H5RBDB_attrname_add_prefix(const char* name, const char* container_obj);
char* H5RBDB_attrname_clear_prefix(const char* attr_db_name);
char* H5RBDB_groupname_get_prefix(const char* db_name);
char* H5RBDB_groupname_clear_prefix(const char* group_db_name);
char* H5RBDB_dataspacename_add_prefix(const char* name);

#endif // __H5RBDB_UTILS_H__