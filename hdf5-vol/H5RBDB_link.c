/**
 * Author: Dimokritos Stamatakis
 * The link functions of the Retro Berkeley DB VOL plugin
 * May, 2016
 */

#define H5G_FRIEND      /*suppress error about including H5Gpkg   */
#include "H5Gpkg.h"     /* Groups               */

#include <sys/time.h>
#include "H5RBDB.h"
#include "H5RBDB_file.h"
#include "H5RBDB_group.h"
#include "H5RBDB_link.h"
#include "H5RBDB_utils.h"

/*-------------------------------------------------------------------------
 * Function:    H5RBDB_link_create
 * Purpose:     Creates a new link with the given parameters
 * Return:  Success:    a pointer to the newely created link object
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_link_create(H5RBDB_group_t* group, const char* link_name, const char* target_name, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id){
    DB_TXN* tid;
    DB* dbp;
    const char* link_name_db_key;
    int ret;

    ret = 0;
    if (group->file_obj->dbenv_info->active_txn)
        tid = group->file_obj->dbenv_info->tid;
    else {
        printf("link creation cannot proceed without a transaction!\n");
        return -1;
    }
    dbp = group->dbp;
    link_name_db_key = H5RBDB_linkname_add_prefix(link_name);
    //printf("Creating link %s -> %s\n", link_name_db_key, target_name);
    //TODO: store the link_info.u.val_size field!
    ret = H5RBDB_database_put_str(&dbp, &tid, link_name_db_key, target_name);
    free(link_name_db_key);
    return ret;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_link_exists
 * Purpose:     Checks whether specified link exists
 * Return:  Success:    1 if exists, 0 otherwise
 *          Failure:    negative value specifying the error
 * Programmer:  Dimokritos Stamatakis
 *              March, 2017
 *-------------------------------------------------------------------------*/
htri_t H5RBDB_link_exists(void* obj, const char* name, hid_t lapl, hid_t dxpl) {
    H5RBDB_group_t* group;
    DB* dbp;
    DB_TXN* tid;
    char* key_name;
    char* container_obj;
    htri_t exists = -1;

    group = (H5RBDB_group_t* ) obj;
    printf("Checking whether %s exists!\n", name);
    dbp = group->dbp;
    if (!group->file_obj->dbenv_info->active_txn){
        printf("Cannot proceed without transaction!\n");
        goto done;
    }
    tid = group->file_obj->dbenv_info->tid;
    key_name = H5RBDB_linkname_add_prefix(name);
    exists = H5RBDB_key_exists(&dbp, &tid, key_name);
    done:
        return exists;
}

/*-------------------------------------------------------------------------
 * Function:    H5RBDB_link_getval
 * Purpose:     Gets the value of the specified link
 * Return:  Success:    0
 *          Failure:    negative value specifying the error
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_link_getval(H5RBDB_group_t* group, char* link_name, void* buf, size_t size, hid_t lapl_id, hid_t dxpl_id){
    DB_TXN* tid;
    DB* dbp;
    const char* link_name_db_key;
    //char link_val[LINK_VALUE_SIZE];
    int ret;

    // avoid compiler warnings
    lapl_id = lapl_id;
    dxpl_id = dxpl_id;

    //memset(link_val, 0, LINK_VALUE_SIZE);
    if (group->file_obj->dbenv_info->active_txn)
        tid = group->file_obj->dbenv_info->tid;
    else {
        printf("link getval cannot proceed without a transaction!\n");
        return -1;
    }
    dbp = group->dbp;
    link_name_db_key = H5RBDB_linkname_add_prefix(link_name);
    // size should include the prefix also! (strlen("__link_") = 7));
    ret = H5RBDB_database_get(&dbp, &tid, link_name_db_key, buf, (u_int32_t) size + strlen("__link_"));
    if (ret == DB_NOTFOUND){
        printf("link not found!\n");
        return -1;
    }
    else if (ret != 0){
        printf("Error locating link!\n");
        return -1;
    }
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_link_iterate
 *
 * Purpose:     Iterate through the children of the given group, as well as the soft links and call the given 
                function (passed as pointer) for each group.
                It is using the DB_DUP flag to allow duplicate values in the same key and 
                an offset cursor (DB_SET_RANGE) to move the cursor in that key
 * Return:      Success:    0
 *              Failure:    negative value
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_link_iterate(H5RBDB_group_t* group_obj, hid_t file_id, hid_t group_id, hbool_t recursive, 
                   H5_index_t idx_type, H5_iter_order_t order, hsize_t *idx_p, 
                   H5L_iterate_t op, void *op_data, hid_t dxpl_id){
    DB_TXN* txn;
    hid_t fapl_id;
    H5L_info_t link_info;
    INIT_COUNTING

    // avoid compiler warnings!
    idx_type = idx_type;
    order = order;
    idx_p = idx_p;
    dxpl_id = dxpl_id;

    txn = group_obj->file_obj->dbenv_info->tid;

    // do not commit transaction when closing group!
    group_obj->readonly = 1;
    //printf("Iterating through children of %s\n", group_obj->name);
    if(!recursive) {
        // Dim
        //printf("** @H5RBDB_iterate: not recursive: calling callback for %s\n", group_obj->name);
        //txn = (DB_TXN*) H5RBDB_get_txn(fapl_id);
        link_info.type = H5L_TYPE_HARD;
	if (idx_type == H5_INDEX_NAME) {
		// use comp_len = 3 to speed-up the comparison, no other key starts with '__c'!
		//H5RBDB_database_cursor_link_iter_apply(group_obj->file_obj->dbenv_info, file_id, &group_obj->dbp, &txn, "__c", 3, op, &link_info, group_obj->file_obj->name, op_data);
		H5RBDB_database_cursor_link_iter_apply(group_obj->file_obj->dbenv_info, group_id, &group_obj->dbp, &txn, "__c", 3, op, &link_info, group_obj->file_obj->name, op_data);
	}
	else if (idx_type == H5_INDEX_CRT_ORDER) {
		//H5RBDB_database_cursor_link_iter_apply(group_obj->file_obj->dbenv_info, file_id, &group_obj->dbp, &txn, "__o", 3, op, &link_info, group_obj->file_obj->name, op_data);
		H5RBDB_database_cursor_link_iter_apply(group_obj->file_obj->dbenv_info, group_id, &group_obj->dbp, &txn, "__o", 3, op, &link_info, group_obj->file_obj->name, op_data);
	}
	link_info.type = H5L_TYPE_SOFT;
	H5RBDB_database_cursor_link_iter_apply(group_obj->file_obj->dbenv_info, group_id, &group_obj->dbp, &txn, "__l", 3, op, &link_info, group_obj->file_obj->name, op_data);
        /*if(H5Lget_info(1, "test", &linfo, H5P_DEFAULT) < 0) {
            fprintf(stderr, "unable to get link info from \"%s\"\n", "test");
        }*/
	group_obj->file_obj->dbenv_info->tid = txn;
    }
    return 0;
}
