/**
 * Author: Dimokritos Stamatakis
 * The group functions of the Retro Berkeley DB VOL plugin
 * October, 2015
 */

#define H5G_FRIEND      /*suppress error about including H5Gpkg   */

#include <db.h>
#include "dbinc/utils.h"
#include "H5Gpkg.h"     /* Groups               */

#include <db.h>
#include <sys/time.h>
#include "H5RBDB.h"
#include "H5RBDB_file.h"
#include "H5RBDB_group.h"
#include "H5RBDB_utils.h"

/*-------------------------------------------------------------------------
 * Function:	H5RBDB_group_create
 * Purpose:	    Creates a Database that represents a group and updates the 
 * 		        children of the HDF5 file containing the group, such as the children of the
 *              parent group.
 * Return:	Success:	a pointer to the newly created group object (group_t *).
 *		    Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
H5RBDB_group_t * H5RBDB_group_create(H5RBDB_file_t* file_obj, H5VL_loc_params_t loc_params, const char* group_name, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id){
    //DB *f_dbp;
    DB *g_dbp, *parent_dbp;
    DB_ENV* f_dbenv;
    DB_TXN* f_tid;
    hid_t pexists;
    int ret;
    H5RBDB_group_t* new_group_obj;
    unsigned crt_order_flags;
    INIT_COUNTING_GROUP

    pexists = H5Pexist(gapl_id, "readonly");
    // avoid compiler warnings!
    loc_params = loc_params;
    gcpl_id = gcpl_id;
    gapl_id = gapl_id;
    dxpl_id = dxpl_id;
    
    // retrieve the DB handles of the already opened DB containing the HDF5 file where we will store the new group
    // TODO: maybe update file DB to contain the current group (although we can find it by traversing the root group of the file)
    //f_dbp = file_obj->dbp;
    f_dbenv = file_obj->dbenv_info->dbenv;
    if (file_obj->dbenv_info->active_txn == 0){
        printf("Cannot proceed without a transaction!\n");
        return NULL;
    }
    //if (crt_order_flags & H5P_CRT_ORDER_TRACKED 
    //if (file_obj->active_txn == 0)
	//    try(f_dbenv->txn_begin(f_dbenv, NULL, &file_obj->tid, DB_TXN_SNAPSHOT));
    f_tid = file_obj->dbenv_info->tid;
    START_COUNTING_GROUP
    // initialize the struct for the new group
    new_group_obj = H5RBDB_group_initialize(group_name, file_obj->name);
    STOP_COUNTING_GROUP("Initialize group object");
    
    H5Pget_link_creation_order(gcpl_id, &crt_order_flags);
    if (crt_order_flags > 0){ // for now we treat the same the H5P_CRT_ORDER_TRACKED and H5P_CRT_ORDER_INDEXED flags. In both cases we keep track of the creation order of groups
	    new_group_obj->creation_order = 1;
    }
    START_COUNTING_GROUP
    // create a database with name new_group_obj->db_name inside the file containing the HDF5 file, as specified in file_obj->name
    try(H5RBDB_database_open(&f_dbenv, &g_dbp, &f_tid, file_obj->dbenv_info->db_file, new_group_obj->db_name, DB_TYPE_GROUP, 1, 0));
    //printf("Created group database %s\n", new_group_obj->absolute_name);
    STOP_COUNTING_GROUP("Create group DB");
    printf("Created group %s\n", new_group_obj->db_name);
    // We are not in retrospection and parent is root group. Root group is already open, no need to open it!
    if(!file_obj->dbenv_info->in_retrospection &&  new_group_obj->parent_name[0] == '/' && new_group_obj->parent_name[1] == '\0') {
        parent_dbp = file_obj->root_dbp;
    }
    else { // parent is not root.
        // first check if this group exists in the DB handles DB!
        if (H5RBDB_key_exists(&file_obj->db_handles_dbp, NULL, new_group_obj->db_parent_name)){
            START_COUNTING_GROUP
            try(H5RBDB_database_get(&file_obj->db_handles_dbp, NULL, new_group_obj->db_parent_name, (void*) &parent_dbp, sizeof(DB*)));
            STOP_COUNTING_GROUP("~~ getting DB handle")
        }
        else {
            START_COUNTING_GROUP
            // open parent Database
            try(H5RBDB_database_open(&f_dbenv, &parent_dbp, &f_tid, file_obj->dbenv_info->db_file, new_group_obj->db_parent_name, DB_TYPE_GROUP, 0, 0));
            STOP_COUNTING_GROUP("Open parent group DB");
            START_COUNTING_GROUP
            try(H5RBDB_database_put(&file_obj->db_handles_dbp, NULL, new_group_obj->db_parent_name, (void*) &parent_dbp, sizeof(DB*)));
            STOP_COUNTING_GROUP("~~ putting parent DB handle")
        }
    }
    
    START_COUNTING_GROUP
    // add the new group in the parent Database's children
    H5RBDB_add_child_group(&parent_dbp, &f_tid, new_group_obj->db_name, new_group_obj->creation_order);
    STOP_COUNTING_GROUP("Add new group to parent's children");

    START_COUNTING_GROUP
    try(H5RBDB_database_put(&file_obj->db_handles_dbp, NULL, new_group_obj->db_name, (void*) &g_dbp, sizeof(DB*)));
    STOP_COUNTING_GROUP("~~ putting DB handle")

    // do not close parent DB!
    /*
    if (!file_obj->dbenv_info->in_retrospection && new_group_obj->parent_name[0] == '/' && new_group_obj->parent_name[1] == '\0') {
        // do not close parent database handle!
    }
    else {
        // always commit before closing a table! Retro has a bug in this case.
        try(f_tid->commit(f_tid, 0));
        START_COUNTING_GROUP
        // close parent database
        try(parent_dbp->close(parent_dbp, 0));
        STOP_COUNTING_GROUP("Close parent database");
        try(f_dbenv->txn_begin(f_dbenv, NULL, &f_tid, DB_TXN_SNAPSHOT));
        file_obj->dbenv_info->tid = f_tid;
    }*/

    new_group_obj->readonly = pexists;

    // new_group_obj->dbenv = malloc(sizeof(DB_ENV));
    // *new_group_obj->dbenv = *f_dbenv;
    // new_group_obj->dbp = malloc(sizeof(DB));
    // *(new_group_obj->dbp) = *g_dbp;
    // We don't need to copy the values of the dbenv and db handles,
    // the memory is allocated and will remain there! Only the address
    // will be lost after the end of the function, so we only have to 
    // store the address!
    new_group_obj->file_obj = file_obj;
    new_group_obj->file_obj->dbenv_info->dbenv = f_dbenv;
    new_group_obj->dbp = g_dbp;
    return new_group_obj;
}
/*-------------------------------------------------------------------------
 * Function:	H5RBDB_group_open
 * Purpose:	Opens a Database that represents a group.
 * Return:	Success:	a pointer to the group object (group_t *).
 *		    Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *-------------------------------------------------------------------------*/
H5RBDB_group_t * H5RBDB_group_open(void *obj, H5I_type_t type, const char *group_name, hid_t gapl_id, hid_t dxpl_id){
    DB *f_dbp, *g_dbp;
    DB_ENV* f_dbenv;
    DB_TXN* f_tid;
    H5RBDB_file_t* file_obj;
    H5RBDB_group_t* opened_group_obj, *group_obj;
    hid_t pexists;
    int ret;
    char* group_absolute_name;
    INIT_COUNTING_GROUP

    // avoid compiler warnings!
    gapl_id=gapl_id;
    dxpl_id=dxpl_id;
    
    pexists = H5Pexist(gapl_id, "readonly");
    if ( type == H5I_FILE){
	    file_obj = (H5RBDB_file_t*) obj;
	    group_absolute_name = group_name;
    }
    else if (type == H5I_GROUP){
	    // we need to get file_obj, and also add the absolute name as the group_name!
	    group_obj = (H5RBDB_group_t*) obj;
	    file_obj = group_obj->file_obj;
	    group_absolute_name = malloc(strlen(group_obj->absolute_name) + strlen(group_name) +1 +1);
	    sprintf(group_absolute_name, "%s/%s", group_obj->absolute_name, group_name);
    }
    else {
	    printf("unable to open group from given location id");
	    return NULL;
    }

    // retrieve the DB handles of the already opened DB containing the HDF5 file where we will get the requested group
    f_dbp = file_obj->dbp;
    f_dbenv = file_obj->dbenv_info->dbenv;
    if (file_obj->dbenv_info->active_txn == 0){
        fprintf(stderr, "Cannot proceed without a transaction!\n");
        return NULL;
    }

    f_tid = file_obj->dbenv_info->tid;

    //printf("Opening group %s\n", group_absolute_name);

    // initialize the struct for the requested group
    opened_group_obj = H5RBDB_group_initialize(group_absolute_name, file_obj->name);

    // first check if this group exists in the DB handles DB!
    if (H5RBDB_key_exists(&file_obj->db_handles_dbp, NULL, opened_group_obj->db_name)){
        START_COUNTING_GROUP
        try(H5RBDB_database_get(&file_obj->db_handles_dbp, NULL, opened_group_obj->db_name, (void*) &g_dbp, sizeof(DB*)));
        STOP_COUNTING_GROUP("getting DB handle %s\n", opened_group_obj->db_name)
    }
    else {
        START_COUNTING_GROUP
        // open the database with name group_absolute_name from inside the file containing the HDF5 file, as specified in file_obj->name
        ret = H5RBDB_database_open(&f_dbenv, &g_dbp, &f_tid, file_obj->dbenv_info->db_file, opened_group_obj->db_name, DB_TYPE_GROUP, 0, 0);
        if (ret == -1){ // this database does not exist. Its ok, user might ask to open any random group name.
            free(opened_group_obj);
            printf("Group %s not found!\n", group_name);
            try(g_dbp->close(g_dbp, 0));
            return NULL;
        }
        STOP_COUNTING_GROUP("Open requested group DB");
        if (file_obj->dbenv_info->in_retrospection == 0){// if we are in retrospection we shouldn't put in the db handles! Its an error to perform any updates while in retrospection!
            START_COUNTING_GROUP
            try(H5RBDB_database_put(&file_obj->db_handles_dbp, NULL, opened_group_obj->db_name, (void*) &g_dbp, sizeof(DB*)));
            STOP_COUNTING_GROUP("putting DB handle %s", opened_group_obj->db_name)
        }
    }
    opened_group_obj->readonly = (unsigned char) pexists;
    
    if (type == H5I_GROUP){
	    free(group_absolute_name);
    }
    
    // We don't need to copy the values of the dbenv and db handles,
    // the memory is allocated and will remain there! Only the address
    // will be lost after the end of the function, so we only have to 
    // store the address!
    // opened_group_obj->dbp = malloc(sizeof(DB));
    // *opened_group_obj->dbp = *g_dbp;
    // opened_group_obj->dbenv = malloc(sizeof(DB_ENV));
    // *opened_group_obj->dbenv = *f_dbenv;
    opened_group_obj->file_obj = file_obj;
    opened_group_obj->file_obj->dbenv_info->dbenv = f_dbenv;
    opened_group_obj->dbp = g_dbp;
    
    //START_COUNTING_GROUP
    //opened_group_obj->children_groups = H5RBDB_get_children_groups(&g_dbp, &f_dbenv, &f_tid);
    //STOP_COUNTING_GROUP("Get children groups");
    return opened_group_obj;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_group_close
 * Purpose: Closes the Database that represents the specified group.
 * Return:  Success:    0.
 *          Failure:    negative value specifying error, or abort if other failure
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_group_close(H5RBDB_group_t* group){
    int ret;
    DB* g_dbp;
    H5RBDB_file_t* file_obj;
    unsigned active_txn;
    DB_ENV* dbenv;
    DB_TXN* tid;
    DBT key;
    INIT_COUNTING_GROUP

    g_dbp = group->dbp;
    if (group == NULL || g_dbp == NULL) {
	//printf("group is NULL!\n");
        return -1;
    }
    file_obj = group->file_obj;
    if (file_obj == NULL || file_obj->dbenv_info == NULL)
	    return -1;
    dbenv = file_obj->dbenv_info->dbenv;
    tid = file_obj->dbenv_info->tid;
    active_txn = file_obj->dbenv_info->active_txn;
    //printf("Is readonly? %d\n", group->readonly);

    // always commit before closing a table! Retro has a bug in this case.
    // if we opened the group as read-only (no updates), then we don't have to commit!
    if (!group->readonly && active_txn == 1){
        START_COUNTING_GROUP
        try(tid->commit(tid, 0));
        STOP_COUNTING_GROUP("commit txn")
    }
    // close the group database
    START_COUNTING_GROUP
    // cheaper to use the DB_NOSYNC flag! Data will be written on env close. It is safe to use it
    // since we are using transactions and logging. BUT, Retro crashes if we close with NOSYNC!!
    //try(g_dbp->close(g_dbp, DB_NOSYNC));
    try(g_dbp->close(g_dbp, 0));
    STOP_COUNTING_GROUP("close group DB")
    if (!group->readonly && active_txn == 1){
        //printf("Starting new txn!\n");
        try(dbenv->txn_begin(dbenv, NULL, &tid, DB_TXN_SNAPSHOT));
        if (file_obj->dbenv_info->in_retrospection == 1)
	       tid->retrospection_data = file_obj->dbenv_info->bite;
        file_obj->dbenv_info->tid = tid;
    }
    file_obj->dbenv_info->dbenv = dbenv;
    // delete this group from the DB handles DB! It is not open anymore!
    if (file_obj->dbenv_info->in_retrospection == 0) {
        START_COUNTING_GROUP
        if (H5RBDB_key_exists(&file_obj->db_handles_dbp, NULL, group->db_name)){
            memset(&key, 0, sizeof(key));
            key.data = group->db_name;
            key.size = (unsigned int) strlen(group->db_name)+1;
            try(file_obj->db_handles_dbp->del(file_obj->db_handles_dbp, NULL, &key, 0));
        }
        STOP_COUNTING_GROUP("Deleting DB from DB handles DB")
    }
    memset(group, 0, sizeof(H5RBDB_group_t));
    free(group);
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_group_initialize
 * Purpose:     Allocates memory for the group object and initializes it
 * Return:  Success:    pointer to the newly allocated group object
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *-------------------------------------------------------------------------*/
H5RBDB_group_t* H5RBDB_group_initialize(const char* group_name, const char* file_name){
    H5RBDB_group_t* new_group_obj;
    group_children_list_t* g_children_list;
    const char* group_name_fixed;
    const char* db_name;
    
    group_name_fixed = H5RBDB_name_fix_slashes(group_name);
    
    new_group_obj = (H5RBDB_group_t*) malloc(sizeof(H5RBDB_group_t));
    memset(new_group_obj, 0, sizeof(H5RBDB_group_t));

    new_group_obj->absolute_name = group_name_fixed;
    new_group_obj->name = H5RBDB_get_name_from_absolute(group_name_fixed);
    
    // construct DB name, by adding the "__group" prefix and the filename containing the group
    db_name = H5RBDB_groupname_add_prefix(new_group_obj->absolute_name, file_name);
    new_group_obj->db_name = db_name;
    new_group_obj->parent_name = H5RBDB_get_parent_name_from_absolute(group_name_fixed);
    // convert parent name to db parent name
    new_group_obj->db_parent_name = H5RBDB_groupname_add_prefix(new_group_obj->parent_name, file_name);
    // allocate the children list
    g_children_list = (group_children_list_t*) malloc(sizeof(group_children_list_t));
    g_children_list->head = NULL;
    g_children_list->size = 0;
    
    new_group_obj->children_groups = g_children_list;

    return new_group_obj;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_add_child_group
 * Purpose:     adds specified child group in specified group's children
 * Return:  Success:    0
 *          Failure:    abort if failure
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *-------------------------------------------------------------------------*/
int H5RBDB_add_child_group(DB** parent_dbp, DB_TXN** tid, const char* absolute_name, hbool_t creation_order){
    int ret;
    struct timeval now;
    unsigned long timestamp;
    //unsigned modified;
    //modified = 1;
    ret = 0;
    try(H5RBDB_database_put_str(parent_dbp, tid, H5RBDB_childname_add_prefix(absolute_name), absolute_name));
    // get current time in microseconds to ensure the creation time is unique! Always the group create operation
    // takes much more than 1 microsecond, so we can ensure the creation times are unique!
    if (creation_order){
	    gettimeofday(&now, NULL);
	    timestamp = (1000000 * now.tv_sec) + now.tv_usec;
	    //printf("Adding child group %s: %s\n", H5RBDB_childname_timestamp_add_prefix(timestamp), absolute_name);
	    try(H5RBDB_database_put_str(parent_dbp, tid, H5RBDB_childname_timestamp_add_prefix(timestamp), absolute_name));
    }
    //try(H5RBDB_database_put_str(parent_dbp, tid, DB_KEYS_GROUP_CHILDREN, absolute_name));
    //try(H5RBDB_database_put(parent_dbp, tid, DB_KEYS_GROUP_MODIFIED, &modified, sizeof(unsigned)));
    return ret;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_get_children_groups
 * Deprecated! Stores children as a list in DB! Now we store children using the DB_DUP flag
 * to allow duplicates!
 * Purpose: Returns the list with the children groups of the specified group DB.
 * Return:  Success:    the list with the children groups of the specified group DB.
 * 			If group has no children, it returns a newly allocated empty list
 *          Failure:        NULL
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *-------------------------------------------------------------------------*/
 // Deprecated! Stores children as a list in DB! Now we store children using the DB_DUP flag to allow duplicates!
group_children_list_t* H5RBDB_get_children_groups(DB** g_dbp, DB_TXN** f_tid){
    int ret;
    DBT key, value;
    group_children_list_t* g_children_list;

    g_children_list = (group_children_list_t*) malloc(sizeof(group_children_list_t));
    
    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));

    key.data = (char*) DB_KEYS_GROUP_CHILDREN;
    key.size = strlen(DB_KEYS_GROUP_CHILDREN)+1;
    
    if ((ret = (*g_dbp)->get(*g_dbp, *f_tid, &key, &value, 0)) == DB_NOTFOUND){ // key not found! This group has no children yet!
        //printf("This group has no children yet!\n");
        g_children_list->head = NULL;
        g_children_list->size = 0;
    }
    else if (ret == 0){
        //printf("Getting children list\n");
        *g_children_list = *((group_children_list_t*) value.data);
        //*((group_children_list_t*) value.data);
        //printf("Already has %u children.\n", ((group_children_list_t*) value.data)->size);
        //printf("Head pointer: %p\n", g_children_list->head);
    }
    else {
        //(*g_dbp)->err(*g_dbp, ret, "g_dbp->get");
        if (*f_tid != NULL){
    	    // abort the transaction
    	    if(( ret = (*f_tid)->abort(*f_tid)) != 0){
    		//(*f_dbenv)->err(*f_dbenv, ret, "f_tid->abort");
	    }
	    }
        return NULL;
    }
    return g_children_list;
}
