/**
 * Author: Dimokritos Stamatakis
 * The dataset functions of the Retro Berkeley DB VOL plugin
 * March, 2016
 */

#define H5G_FRIEND      /*suppress error about including H5Gpkg   */
#define H5D_FRIEND      /*suppress error about including H5Dpkg   */
#define H5S_FRIEND      /*suppress error about including H5Spkg   */

#include <db.h>
#include "dbinc/utils.h"
#include "H5Gpkg.h"     /* Groups               */
#include "H5Dpkg.h"             /* Dataset pkg                          */
#include "H5Spkg.h"
#include "H5Sprivate.h"

#include <db.h>
#include <sys/time.h>
#include "H5RBDB.h"
#include "H5RBDB_file.h"
#include "H5RBDB_dataset.h"
#include "H5RBDB_group.h"

 #define KEY_TMP_LEN 256

/*-------------------------------------------------------------------------
 * Function:	H5RBDB_dataset_create
 * Purpose:	Creates a dataset entry in the Database of the group specified in the path.
 * Return:	Success:	a pointer to the newly created dataset structure (H5RBDB_dataset_t *).
 *		    Failure:        NULL
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
H5RBDB_dataset_t * H5RBDB_dataset_create(H5RBDB_file_t* file_obj, const char* name, hid_t space_id, hid_t type_id, hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id){
    DB *f_dbp, *parent_dbp;
    DB_ENV* f_dbenv;
    DB_TXN* f_tid;
    H5RBDB_dataset_metadata_t dataset_metadata;
    const H5S_t    *space;              /* Dataspace for dataset */
    int ret;
    H5RBDB_dataset_t* new_dataset_obj;
    const char key_tmp[KEY_TMP_LEN];
    H5RBDB_dataspace_nums_t dspace_nums;
    INIT_COUNTING_DATASET
    // retrieve the DB handles of the already opened DB containing the HDF5 file where we will store the new group
    f_dbp = file_obj->dbp;
    f_dbenv = file_obj->dbenv_info->dbenv;
    if (file_obj->dbenv_info->active_txn == 0){
        printf("Cannot proceed without a transaction!\n");
        return NULL;
    }
    f_tid = file_obj->dbenv_info->tid;

    new_dataset_obj = H5RBDB_dataset_initialize(name, file_obj->name);

    //TODO: check the type (from type_id) as well!

    //printf("name: %s\nabsolute name: %s\nparent: %s\nparent DB name: %s\nmetadata key: %s\ndata key: %s\n", 
      //  new_dataset_obj->name, new_dataset_obj->absolute_name, new_dataset_obj->parent_name, new_dataset_obj->parent_db_name, new_dataset_obj->metadata_key, new_dataset_obj->data_key);
    // We are not in retrospection and parent is root group. Root group is already open, no need to open it!
    if(!file_obj->dbenv_info->in_retrospection &&  new_dataset_obj->parent_name[0] == '/' && new_dataset_obj->parent_name[1] == '\0') {
        new_dataset_obj->dbp = file_obj->root_dbp;
        printf("Parent is root group, we have pointer!\n");
    }
    else{ // parent is not root group, open the database! (May find a way to retrieve database pointer so that to avoid opening again)
        START_COUNTING_DATASET
        H5RBDB_database_open(&f_dbenv, &parent_dbp, &f_tid, file_obj->dbenv_info->db_file, new_dataset_obj->parent_db_name, DB_TYPE_GROUP, 0, 1);
        new_dataset_obj->dbp = parent_dbp;
        STOP_COUNTING_DATASET("Open group DB");
    }

    if(NULL == (space = (const H5S_t *)H5I_object_verify(space_id, H5I_DATASPACE))){
        fprintf(stderr, "not a dataspace ID");
        return -1;
    }

    new_dataset_obj->dataspace.nelem = space->extent.nelem;
    new_dataset_obj->dataspace.rank = space->extent.rank;
    new_dataset_obj->dataspace.size = space->extent.size;
    new_dataset_obj->dataspace.max = space->extent.max;

    // store dataspace information
    dspace_nums.nelem = new_dataset_obj->dataspace.nelem;
    dspace_nums.rank = new_dataset_obj->dataspace.rank;
    H5RBDB_put_dataspace_nums(&new_dataset_obj->dbp, &f_tid, new_dataset_obj->dataspace_key, &dspace_nums);
    //H5RBDB_put_dataspace(&new_dataset_obj->dbp, &f_tid, DSPACE_NELEM, new_dataset_obj->dataspace_key, (void*)&new_dataset_obj->dataspace.nelem);
    //H5RBDB_put_dataspace(&new_dataset_obj->dbp, &f_tid, DSPACE_RANK, new_dataset_obj->dataspace_key, (void*)&new_dataset_obj->dataspace.rank);
    H5RBDB_put_dataspace_multiple(&new_dataset_obj->dbp, &f_tid, DSPACE_SIZE, new_dataset_obj->dataspace_key, (void*)new_dataset_obj->dataspace.size, new_dataset_obj->dataspace.rank);
    H5RBDB_put_dataspace_multiple(&new_dataset_obj->dbp, &f_tid, DSPACE_MAX, new_dataset_obj->dataspace_key, (void*)new_dataset_obj->dataspace.max, new_dataset_obj->dataspace.rank);

    // probably not required to store
    //dataset_metadata.type_id = type_id;
    //dataset_metadata.dcpl_id = dcpl_id;    
    //try(database_put(&new_dataset_obj->dbp, &file_obj->tid, new_dataset_obj->metadata_key, 
      //                              &dataset_metadata, sizeof(H5RBDB_dataset_metadata_t)));
    new_dataset_obj->type_id = type_id;
    new_dataset_obj->dapl_id = dapl_id;
    new_dataset_obj->dxpl_id = dxpl_id;
    new_dataset_obj->file_obj = file_obj;
    return new_dataset_obj;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_dataset_open
 * Purpose: Opens a dataset from the Database of the group specified in the path.
 * Return:  Success:    a pointer to the opened dataset structure (H5RBDB_dataset_t *).
 *          Failure:        NULL
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
H5RBDB_dataset_t * H5RBDB_dataset_open(H5RBDB_file_t* file_obj, const char* name, hid_t dapl_id, hid_t dxpl_id){
    DB *f_dbp, *parent_dbp;
    DB_ENV* f_dbenv;
    DB_TXN* f_tid;
    H5RBDB_dataset_t dataset;
    H5RBDB_dataset_metadata_t dataset_metadata;
    void* value_buf;
    int ret, i;
    H5RBDB_dataset_t* new_dataset_obj;
    H5RBDB_dataspace_nums_t dspace_nums;
    INIT_COUNTING_DATASET

    f_dbenv = file_obj->dbenv_info->dbenv;
    if (file_obj->dbenv_info->active_txn == 0){
        printf("Cannot proceed without a transaction!\n");
        return NULL;
    }
    f_tid = file_obj->dbenv_info->tid;

    new_dataset_obj = H5RBDB_dataset_initialize(name, file_obj->name);
    //printf("name: %s\nabsolute name: %s\nparent: %s\nparent DB name: %s\nmetadata key: %s\ndata key: %s\n", 
      //  new_dataset_obj->name, new_dataset_obj->absolute_name, new_dataset_obj->parent_name, new_dataset_obj->parent_db_name, 
        //new_dataset_obj->metadata_key, new_dataset_obj->data_key);
    // We are not in retrospection and parent is root group. Root group is already open, no need to open it!
    if(!file_obj->dbenv_info->in_retrospection && new_dataset_obj->parent_name[0] == '/' && new_dataset_obj->parent_name[1] == '\0') {
        new_dataset_obj->dbp = file_obj->root_dbp;
        printf("Parent is root group, we have dbp pointer: %p\n", new_dataset_obj->dbp);
    }
    else{ // parent is not root group, open the database! (May find a way to retrieve database pointer so that to avoid opening again)
        START_COUNTING_DATASET
        H5RBDB_database_open(&f_dbenv, &parent_dbp, &f_tid, file_obj->dbenv_info->db_file, new_dataset_obj->parent_db_name, DB_TYPE_GROUP, 0, 1);
        new_dataset_obj->dbp = parent_dbp;
        STOP_COUNTING_DATASET("Open group DB");
    }

    // retrieve dataspace information
    H5RBDB_get_dataspace_nums(&new_dataset_obj->dbp, &f_tid, new_dataset_obj->dataspace_key, (void*)&dspace_nums);
    new_dataset_obj->dataspace.nelem = dspace_nums.nelem;
    new_dataset_obj->dataspace.rank = dspace_nums.rank;
    //H5RBDB_get_dataspace(&new_dataset_obj->dbp, &f_tid, DSPACE_NELEM, new_dataset_obj->dataspace_key, (void*)&new_dataset_obj->dataspace.nelem);
    //H5RBDB_get_dataspace(&new_dataset_obj->dbp, &f_tid, DSPACE_RANK, new_dataset_obj->dataspace_key, (void*)&new_dataset_obj->dataspace.rank);
    new_dataset_obj->dataspace.size = H5RBDB_get_dataspace_multiple(&new_dataset_obj->dbp, &f_tid, DSPACE_SIZE, 
        new_dataset_obj->dataspace_key, new_dataset_obj->dataspace.rank);
    new_dataset_obj->dataspace.max = H5RBDB_get_dataspace_multiple(&new_dataset_obj->dbp, &f_tid, DSPACE_MAX, 
        new_dataset_obj->dataspace_key, new_dataset_obj->dataspace.rank);

    //printf("rank: %u\n", new_dataset_obj->dataspace.rank);
    //for (i=0; i<new_dataset_obj->dataspace.rank; i++){
    //    printf("size[%d]: %u\n", i, new_dataset_obj->dataspace.size[i]);
    //}
    //printf("Getting in buffer size %u\n", sizeof(H5RBDB_dataset_metadata_t));
    /*if ( (ret = database_get(&new_dataset_obj->dbp, &f_tid, new_dataset_obj->metadata_key, (void*)&dataset_metadata, sizeof(H5RBDB_dataset_metadata_t))) == 0 ){
        new_dataset_obj->space_id = dataset_metadata.space_id;
        new_dataset_obj->type_id = dataset_metadata.type_id;
        new_dataset_obj->dcpl_id = dataset_metadata.dcpl_id;
    }
    else {
        fprintf(stderr, "Something went wrong when accessing dataset! Couldn't find metadata!\n");
    }*/
    // store the new data access property list and data transfer property list as got from the API call
    new_dataset_obj->dapl_id = dapl_id;
    new_dataset_obj->dxpl_id = dxpl_id;
    
    new_dataset_obj->file_obj = file_obj;
    new_dataset_obj->file_obj->dbenv_info->dbenv = f_dbenv;

    return new_dataset_obj;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_dataset_write
 * Purpose: Writes data to dataset
 * Return:  Success:    0
 *          Failure:    negative value specifying error
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_dataset_write(H5RBDB_dataset_t* dataset, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t dxpl_id, const void* buf){
    hsize_t *size;              /* Current size of the dimensions */
    hsize_t *max;               /* Maximum size of the dimensions */
    hsize_t num_cells;
    hsize_t write_size;
    unsigned rank;
    unsigned i;
    unsigned single_put = 0;
    //unsigned max_size_per_put = 4096;
    //unsigned max_size_per_put = 8192;
    //unsigned max_size_per_put = 16384;
    //unsigned max_size_per_put = 32768;
    unsigned max_size_per_put = 65536;
    //unsigned max_size_per_put = 131072;
    //unsigned max_size_per_put = 262144;
    //unsigned max_size_per_put = 1048576;
    //unsigned max_size_per_put = 10485760;
    unsigned size_per_put;
    unsigned offset = 0;
    unsigned total_written = 0;
    int ret;

    num_cells = 1;
    //printf("In dataset write...\n");
    //printf("name: %s\n", dataset->absolute_name);
    //printf("mem type id: %ld, size: %ld\n", mem_type_id, H5Tget_size(mem_type_id));

    if (mem_space_id == H5S_ALL)
        printf("mem space is H5S_ALL!\n");
    if (file_space_id == H5S_ALL)
        printf("file space is H5S_ALL!\n");


    rank = dataset->dataspace.rank;
    size = dataset->dataspace.size;
    max = dataset->dataspace.max;
    printf("rank is %u\n", rank);
    for (i=0; i<rank; i++){
        printf("dimension %u: %d\n", i, size[i]);
        num_cells *= size[i];
    }
    write_size = num_cells * H5Tget_size(mem_type_id);
    printf("Number of cells: %d\nTotal write size: %d\n", num_cells, write_size);


    if (single_put == 1)
        try(H5RBDB_database_put(&dataset->dbp, &dataset->file_obj->dbenv_info->tid, dataset->data_key, buf, write_size));
    else{
        i=0;
        while(offset < write_size){
            if (write_size - offset >= max_size_per_put)
                size_per_put = max_size_per_put;
            else
                size_per_put = write_size - offset;
            try(H5RBDB_database_put(&dataset->dbp, &dataset->file_obj->dbenv_info->tid, dataset->data_key, buf+offset, size_per_put));
            offset += size_per_put;
            total_written+=size_per_put;
            //printf("Written record %u of size %u\n", i, size_per_put);
            i++;
        }
    }
    //printf("Total written: %u\n", total_written);
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_dataset_read
 * Purpose: Reads data from dataset
 * Return:  Success:    0
 *          Failure:    negative value specifying error
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_dataset_read(H5RBDB_dataset_t* dataset, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t plist_id, void* buf){
    hsize_t *size;              /* Current size of the dimensions */
    hsize_t *max;               /* Maximum size of the dimensions */
    hsize_t num_cells;
    hsize_t read_size;
    unsigned rank;
    unsigned i;
    int ret;
    unsigned single_get = 0;

    num_cells = 1;

    //unsigned max_size_per_get = 4096;
    unsigned max_size_per_get = 65536;
    unsigned size_per_get;
    unsigned offset = 0;
    unsigned total_read = 0;
    

    //printf("In dataset read...\n");
    //printf("name: %s\n", dataset->absolute_name);
    //printf("mem type id: %d, size: %d\n", mem_type_id, H5Tget_size(mem_type_id));

    if (mem_space_id == H5S_ALL)
        printf("mem space is H5S_ALL!\n");
    if (file_space_id == H5S_ALL)
        printf("file space is H5S_ALL!\n");
    
    rank = dataset->dataspace.rank;
    size = dataset->dataspace.size;
    max = dataset->dataspace.max;
    //printf("rank is %u\n", rank);
    for (i=0; i<rank; i++){
        printf("dimension %u: %d\n", i, size[i]);
        num_cells *= size[i];
    }
    read_size = num_cells * H5Tget_size(mem_type_id);
    //printf("Number of cells: %d\nTotal read size: %d\n", num_cells, read_size);

    if (single_get == 1)
        try(H5RBDB_database_get(&dataset->dbp, &dataset->file_obj->dbenv_info->tid, dataset->data_key, buf, read_size));
    else{
        try(H5RBDB_database_cursor_get(&dataset->dbp, &dataset->file_obj->dbenv_info->tid, dataset->data_key, buf, max_size_per_get, read_size));
    }
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_dataset_initialize
 * Purpose: Allocates memory for new dataset object and initializes it
 * Return:  Success:    pointer to the allocated object for the dataset
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
H5RBDB_dataset_t* H5RBDB_dataset_initialize(const char* dataset_name, const char* file_name){
    H5RBDB_dataset_t* new_dataset_obj;
    const char* dataset_name_fixed;
    const char* prefix;
    
    dataset_name_fixed = H5RBDB_name_fix_slashes(dataset_name);
    
    new_dataset_obj = (H5RBDB_dataset_t*) malloc(sizeof(H5RBDB_dataset_t));
    new_dataset_obj->absolute_name = dataset_name_fixed;
    new_dataset_obj->name = H5RBDB_get_name_from_absolute(dataset_name_fixed);
    // construct key, by adding the "__dataset" prefix
    new_dataset_obj->metadata_key = H5RBDB_datasetname_add_prefix(new_dataset_obj->name, "metadata");
    new_dataset_obj->data_key = H5RBDB_datasetname_add_prefix(new_dataset_obj->name, "data");
    new_dataset_obj->dataspace_key = H5RBDB_datasetname_add_prefix(new_dataset_obj->name, "_dataspace");

    new_dataset_obj->parent_name = H5RBDB_get_parent_name_from_absolute(dataset_name_fixed);
    
    // construct DB name, by adding the "__group" prefix and the filename containing the group
    new_dataset_obj->parent_db_name = H5RBDB_groupname_add_prefix(new_dataset_obj->parent_name, file_name);
    return new_dataset_obj;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_put_dataspace_nums
 * Purpose: Inserts the specified dataspace numbers (nelem, rank) in database
 * Return:  Success:    -
 *          Failure:    aborts on failure
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
void H5RBDB_put_dataspace_nums(DB** dbp, DB_TXN** tid, const char* key, H5RBDB_dataspace_nums_t* value){
    int ret;
    char key_tmp [KEY_TMP_LEN];
    memset(key_tmp, 0, KEY_TMP_LEN);
    sprintf(key_tmp, "%s_dspace_nums", key);
    try(H5RBDB_database_put(dbp, tid, key_tmp, (void*)value, sizeof(H5RBDB_dataspace_nums_t)));
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_put_dataspace
 * Purpose: Inserts the specified dataspace in database
 * Return:  Success:    -
 *          Failure:    aborts on failure
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
void H5RBDB_put_dataspace(DB** dbp, DB_TXN** tid, dspace_field field, const char* key, void* value){
    char key_tmp [KEY_TMP_LEN];
    int ret;
    memset(key_tmp, 0, KEY_TMP_LEN);
    switch (field){
        case DSPACE_NELEM: // nelem
        sprintf(key_tmp, "%s_nelem", key);
        try(H5RBDB_database_put(dbp, tid, key_tmp, value, sizeof(hsize_t)));
        break;
        case DSPACE_RANK: // rank
        sprintf(key_tmp, "%s_rank", key);
        try(H5RBDB_database_put(dbp, tid, key_tmp, value, sizeof(unsigned)));
        break;
    }
}
/*-------------------------------------------------------------------------
TODO: make it store size and max in separate keys! Now we don't use DB_DUP!!
 * Function:    H5RBDB_put_dataspace_multiple
 * Purpose: Inserts multiple dataspace fields in database (size and max for each rank)
 * Return:  Success:    -
 *          Failure:    aborts on failure
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
void H5RBDB_put_dataspace_multiple(DB** dbp, DB_TXN** tid, dspace_field field, const char* key, void* value, unsigned rank){
    char key_tmp [KEY_TMP_LEN];
    hsize_t* value_hsize;
    int i, ret;

    memset(key_tmp, 0, KEY_TMP_LEN);
    value_hsize = (hsize_t*)value;

    switch (field){
        case DSPACE_SIZE: // size
            sprintf(key_tmp, "%s_size", key);
            for (i=0; i<rank; i++){
                //printf("--- Putting dataspace %s field with key %s, size %u and value %llu\n", key, key_tmp, sizeof(hsize_t), *( (hsize_t*) value) );
                try(H5RBDB_database_put(dbp, tid, key_tmp, &value_hsize[i], sizeof(hsize_t)));
                //printf("~~~ Put size...\n");
            }
            break;
        case DSPACE_MAX: // max
            sprintf(key_tmp, "%s_max", key);
            for (i=0; i<rank; i++){
                //printf("--- Putting dataspace %s field with key %s, size %u and value %llu\n", key, key_tmp, sizeof(hsize_t), *((hsize_t*) value) );
                try(H5RBDB_database_put(dbp, tid, key_tmp, &value_hsize[i], sizeof(hsize_t)));
                //printf("~~~ Put max...\n");
            }
            break;
    }
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_get_dataspace_nums
 * Purpose: Retrieves the specified dataspace numbers (nelem, rank) from database
 * Return:  Success:    -
 *          Failure:    aborts on failure
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
void H5RBDB_get_dataspace_nums(DB** dbp, DB_TXN** tid, const char* key, void* value){
    char key_tmp [KEY_TMP_LEN];
    int ret;
    memset(key_tmp, 0, KEY_TMP_LEN);
    sprintf(key_tmp, "%s_dspace_nums", key);
    try(H5RBDB_database_get(dbp, tid, key_tmp, value, sizeof(H5RBDB_dataspace_nums_t)));
    //printf("Got nelems: %u, rank: %u\n", ((H5RBDB_dataspace_nums_t*) value)->nelem, ((H5RBDB_dataspace_nums_t*) value)->rank);
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_get_dataspace
 * Purpose: Retrieves the specified dataspace from database
 * Return:  Success:    -
 *          Failure:    aborts on failure
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
void H5RBDB_get_dataspace(DB** dbp, DB_TXN** tid, dspace_field field, const char* key, void* value){
    char key_tmp [KEY_TMP_LEN];
    int ret;
    memset(key_tmp, 0, KEY_TMP_LEN);
    switch (field){
        case DSPACE_NELEM: // nelem
        sprintf(key_tmp, "%s_nelem", key);
        try(H5RBDB_database_get(dbp, tid, key_tmp, value, sizeof(hsize_t)));
        break;
        case DSPACE_RANK: // rank
        sprintf(key_tmp, "%s_rank", key);
        try(H5RBDB_database_get(dbp, tid, key_tmp, value, sizeof(unsigned)));
        break;
    }
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_get_dataspace_multiple
 * Purpose: Retrieves multiple dataspace fields from database (size and max for each rank)
 * Return:  Success:    pointer to multiple dataspace fields (type hsize_t*)
 *          Failure:    aborts on failure
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
void* H5RBDB_get_dataspace_multiple(DB** dbp, DB_TXN** tid, dspace_field field, const char* key, unsigned rank){
    char key_tmp [KEY_TMP_LEN];
    hsize_t* value_hsize;
    int ret;

    if (rank > 1){
        printf("Rank greater than 1 is not supported yet! We must fix the cursor operations, or choose an alternative design!\n");
        return NULL;
    }
    if (rank == 0)
        return NULL;

    memset(key_tmp, 0, KEY_TMP_LEN);
    
    // allocate space for the value
    value_hsize = malloc(rank * sizeof(hsize_t));    

    switch (field){
        case DSPACE_SIZE: // size
            sprintf(key_tmp, "%s_size", key);
            try(H5RBDB_database_cursor_get_hsize(dbp, tid, key_tmp, value_hsize, sizeof(hsize_t), rank));
            break;
        case DSPACE_MAX: // max
            sprintf(key_tmp, "%s_max", key);
            try(H5RBDB_database_cursor_get_hsize(dbp, tid, key_tmp, value_hsize, sizeof(hsize_t), rank));
            break;
        case DSPACE_NELEM: // number of elements
            printf("Getting number of elements not yet supported!\n");
            break;
        case DSPACE_RANK: // rank
            printf("Getting rank not yet supported!\n");
            break;
        default:
            printf("Dataspace field not known!\n");
            break;
    }
    return (void*)value_hsize;
}
