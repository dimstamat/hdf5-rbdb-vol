/**
 * Author: Dimokritos Stamatakis
 * The Retro Berkeley DB VOL plugin
 * October, 2015
 */


#define H5C_FRIEND		/*suppress error about including H5Cpkg	  */
#define H5G_FRIEND      /*suppress error about including H5Gpkg   */

#include <db.h>
#include "dbinc/utils.h"

#include "H5private.h"		/* Generic Functions			*/
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5Iprivate.h"		/* IDs			  		*/
#include "H5Cpkg.h"
#include "H5ACprivate.h"
#include "H5VLprivate.h"    /* VOL plugins              */
#include "H5Gpkg.h"     /* Groups               */

#include <string.h>
#include <sys/time.h>

#include "H5RBDB.h"
#include "H5RBDB_utils.h"

/*-------------------------------------------------------------------------
 * Function:	H5RBDB_put_hid_t
 * Purpose:	Inserts the specified key/value pair as a hid_t.
 * Return:	Success:	the result of the put operation. 
 *		    Failure:	non-zero integer
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
int H5RBDB_put_hid_t(DB* dbp, DB_TXN* tid, char* key_str, hid_t value_id){
	int ret;
	DBT key, value;
	
	memset(&key, 0, sizeof(key));
	memset(&value, 0, sizeof(value));	

	key.data = key_str;
	key.size = (u_int32_t)strlen(key_str)+1;
	
	value.data = &value_id;
	value.size = sizeof(hid_t);
	
	ret = dbp->put(dbp, tid, &key, &value, 0);
	// no need to free the key/value, we didn't malloc anything
	return ret;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_database_put_str
 * Purpose: Inserts the specified key/value strings.
 * Return:  Success:    0
 *          Failure:    aborts if put fails
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
int H5RBDB_database_put_str(DB** dbp, DB_TXN** tid, char* key_str, char* value_str){
    int ret;
    DBT key, value;
    INIT_COUNTING
    
    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));

    key.data = key_str;
    key.size = (unsigned int) strlen(key_str)+1;
    value.data = value_str;
    value.size = (unsigned int) strlen(value_str)+1;
    //START_COUNTING
    try((*dbp)->put(*dbp, *tid, &key, &value, 0));
    //STOP_COUNTING("Put key value")
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_database_put
 * Purpose: Inserts the specified key string and void* value buffer, of size value_size.
 * Return:  Success:    0
 *          Failure:    aborts if put fails
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
int H5RBDB_database_put(DB** dbp, DB_TXN** tid, char* key_str, void* value_buf, unsigned value_size){
    int ret;
    DBT key, value;
    INIT_COUNTING
    
    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));

    key.data = key_str;
    key.size = (unsigned int) strlen(key_str)+1;
    value.data = value_buf;
    value.size = value_size;
    //printf("Putting %s, value size: %d\n", key_str, value.size);
    //START_COUNTING
    try((*dbp)->put(*dbp,  (tid == NULL ? NULL: *tid), &key, &value, 0));
    //STOP_COUNTING("Put key value")
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_database_get
 * Purpose: Retrieves the specified key string and stores in void* value buffer, of size value_length.
 *          Buffer must be allocated by user! If value size is bigger than specified value_length, put fails.
 * Return:  Success:    0
 *          Failure:    whatever get returned
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
int H5RBDB_database_get(DB** dbp, DB_TXN** tid, char* key_str, void * value_buf, u_int32_t value_length){
    DBT key, val;
    int ret;
    INIT_COUNTING
    
    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));
    key.data = key_str;
    key.size = (unsigned int) strlen(key_str)+1;
    val.data = value_buf;
    val.ulen = value_length;
    val.flags = DB_DBT_USERMEM;
    
    START_COUNTING
    // we might not want a transactional read, in this case do not dereference tid
    ret = (*dbp)->get(*dbp, (tid == NULL? NULL : *tid), &key, &val, 0);
    if (ret == DB_NOTFOUND){
        printf("Warning: Key %s not found!!\n", key_str);
        return ret;
    }
    else if (ret == 0){
        //printf("Successfully got value\n");
        return ret;
    }
    else {
        printf("Error getting %s! val.size is %u and provided is %u\n", key_str, val.size, val.ulen);
        err(ret);
    }
    err:
        return ret;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_database_get_hsize
 * Purpose: Retrieves the specified key string and void* value buffer of size size and number of elements
            nelem. The purpose if it is getting the values of each rank for a stored datatype, using a cursor.
 * Return:  Success:    0
 *          Failure:    negative value specifying the error
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
int H5RBDB_database_cursor_get_hsize(DB** dbp, DB_TXN** tid, char* key_str, void* value_buf, unsigned size, hsize_t nelements){
    DBT key, val;
    DBC* dbcp;
    int ret;
    hsize_t i=0;
    hsize_t val_hsize;

    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));
    key.data = key_str;
    key.size = (unsigned int) strlen(key_str)+1;    

    if (nelements > 1){
        printf("Rank greater than 1 is not supported yet! We must fix the cursor operations, or choose an alternative design!\n");
        return -1;
    }

    /* Acquire a cursor for the database. */
    //try((*dbp)->cursor(*dbp, *tid, &dbcp, 0));
    //printf("db cursor get hsize: key: %s, val size: %u, nelements: %u\n", key_str, size, nelements);
    //val.data = & ((hsize_t*)value_buf)[i];
    val.data = &val_hsize;
    val.ulen = size;
    val.flags = DB_DBT_USERMEM;
    /* Walk through the database and print out the key/value pairs. */
    //while ((ret = dbcp->c_get(dbcp, &key, &val, DB_NEXT)) == 0){
	ret = (*dbp)->get(*dbp, (tid == NULL? NULL : *tid), &key, &val, 0);
    if (ret == 0){
		//printf("---> Got %u\n", val_hsize);
	}
    else {
        printf(" Error getting %s: return val: %d\n", key_str, ret);
    }
        //i++;
        //if (i == nelements || nelements == 0) // in some cases rank is 0!
          //  break;
        //val.data = &((hsize_t*)value_buf)[i];
        //val.data = &val_hsize;
        //val.ulen = size;
        //val.flags = DB_DBT_USERMEM;
    //}
    //printf("Is it big enough?? %u\n", sizeof(((hsize_t*)value_buf)[i]));
    /*if (ret != 0) {
       fprintf(stderr, "Error getting key from cursor: %s\n", db_strerror(ret));
       fprintf(stderr, "Required size: %u\n", val.size);
       return -1;
    }
    if ((ret = dbcp->c_close(dbcp)) != 0){
      fprintf(stderr, "Error closing cursor: %s\n", db_strerror(ret));
      return -1;
    }*/
    ((hsize_t*)value_buf)[0] = val_hsize;
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_database_cursor_link_iter_apply
 * Purpose: Uses a cursor to iterate through the keys of the given prefix of length comp_len and
 *           calls the apply function, which is the function passed in H5Literate (in the same format)
 * Return:  Success:    0.
 *          Failure:    non-zero integer specifying the encountered error
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_database_cursor_link_iter_apply(H5RBDB_dbenv_t* dbenv_info, hid_t obj_id, DB** dbp, DB_TXN** tid, char* prefix, unsigned comp_len, H5L_iterate_t apply, H5L_info_t* link_info, char* file_name, void* op_data){
    int ret;
    unsigned i;
    DBC* dbcp;
    DB_TXN* inner_txn;
    DBT key, val;
    const char* name;
    const char* link_name_db;
    char linkval [LINK_VALUE_SIZE];
    unsigned use_inner_txn = 0;
    unsigned char use_txn = 1; // for now do not use transaction to iterate over links! If needed, we can leave it as an option in the client side!
    INIT_COUNTING
    
    memset(&key, 0, sizeof(key));
    memset(&val, 0, sizeof(val));
    key.data = prefix;
    key.size = strlen(prefix)+1;
    
    if (use_inner_txn)
	   try(dbenv_info->dbenv->txn_begin(dbenv_info->dbenv, NULL, &inner_txn, DB_TXN_SNAPSHOT));
    if(use_inner_txn)
    	try((*dbp)->cursor(*dbp, inner_txn, &dbcp, 0));
    else if (use_txn)
	   try((*dbp)->cursor(*dbp, *tid, &dbcp, 0));
    else {// no transaction! 
	try((*dbp)->cursor(*dbp, NULL, &dbcp, 0));
    }
    // get the first element, by moving the cursor at the first key with the given prefix
    ret = dbcp->get(dbcp, &key, &val, DB_SET_RANGE);
    //printf("Got %s: %s\n", (char*)key.data, (char*)val.data);
    if (ret == DB_NOTFOUND){
	try(dbcp->c_close(dbcp));
	if(use_inner_txn)
	    try(inner_txn->commit(inner_txn, 0));
        return 0;
    }
    for (i=0; i<comp_len; i++){
        if (*(((char*)key.data) + i) != *(prefix+i)){ // we are done, close the cursor!
            try(dbcp->c_close(dbcp));
	    if(use_inner_txn)
		try(inner_txn->commit(inner_txn, 0));
            return 0;
        }
    }
    //printf("%.*s : %.*s\n", (int)key.size, (char *)key.data, (int)val.size, (char *)val.data);
    if (link_info->type == H5L_TYPE_HARD){
        // we have to convert the group name which is in BDB format (__group_.....) in HDF5 format (remove prefix)
        name = H5RBDB_groupname_clear_prefix((char *)val.data);
    }
    else{
        name = H5RBDB_linkname_clear_prefix((char *)key.data);
        //op_data->linkval = (char*)val.data;
    }
    //TODO: retrieve the link_info.u.val_size field!
    // for each object call the callback!
    (*apply)(obj_id, name, link_info, op_data);
    // get the rest elements, by iterating through the next keys
    do{
	ret = dbcp->get(dbcp, &key, &val, DB_NEXT);
	//printf("Got %s: %s\n", (char*)key.data, (char*)val.data);
	if (ret == DB_NOTFOUND){
	    try(dbcp->c_close(dbcp));
	    if(use_inner_txn)
		try(inner_txn->commit(inner_txn, 0));
	    return 0;
	}
        for (i=0; i<comp_len; i++){
            if (*(((char*)key.data) + i) != *(prefix+i)){ // we are done, close the cursor!
                try(dbcp->c_close(dbcp));
		if(use_inner_txn)
			try(inner_txn->commit(inner_txn, 0));
                return 0;
            }
        }
        //printf("%.*s : %.*s\n", (int)key.size, (char *)key.data, (int)val.size, (char *)val.data);
        if (link_info->type == H5L_TYPE_HARD){
            // we have to convert the group name which is in BDB format (__group_.....) in HDF5 format (remove prefix)
            name = H5RBDB_groupname_clear_prefix((char *)val.data);
        }
        else{
            name = H5RBDB_linkname_clear_prefix((char *)key.data);
            //op_data->linkval = (char*)val.data;
        }
        // for each object call the callback!
        (*apply)(obj_id, name, link_info, op_data);
    }while (ret == 0);
    try(dbcp->c_close(dbcp));
    if(use_inner_txn)
    	try(inner_txn->commit(inner_txn, 0));
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_database_cursor_get
 * Purpose: Retrieves the specified key string and void* value buffer of record size rec_size and total size total_size,
            by using a DB cursor.
 * Return:  Success:    0
 *          Failure:    negative value specifying the error
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
int H5RBDB_database_cursor_get(DB** dbp, DB_TXN** tid, char* key_str, void* value_buf, unsigned rec_size, unsigned total_size){
    DBT key, val;
    DBC* dbcp;
    int ret, i=0;
    unsigned offset = 0;
    unsigned size_per_get;
    unsigned total_read = 0;

    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));
    key.data = key_str;
    key.size = (unsigned int) strlen(key_str)+1;    

    /* Acquire a cursor for the database. */
    try((*dbp)->cursor(*dbp, *tid, &dbcp, 0));

    //printf("Total size: %u\n", total_size);

    /* Walk through the database and get the key/value pairs. */
    do {
	printf("%u\n", val.size);
        // calculate the read size
        if (total_size - offset >= rec_size)
            size_per_get = rec_size;
        else if (total_size - offset > 0)
            size_per_get = total_size - offset;
	    else // there is nothing left to read, just break
		break;
        //printf("Reading record %d of size %u\n", i, size_per_get);
        // set the parameters for the cursor get
        // TODO: we should know what type is the value_buf. Cannot do pointer arithmetic to a void* !
        val.data = value_buf + offset;
        val.ulen = size_per_get;
        val.flags = DB_DBT_USERMEM;
	    val.size = size_per_get;
        // increase the offset (for next time)
        offset += size_per_get;
        total_read+=size_per_get;
        i++;
    }
    while ((ret = dbcp->c_get(dbcp, &key, &val, DB_NEXT)) == 0);
    //printf("%u\n", val.size);
    //printf("Read %u\n", total_read);

    if (ret != 0) {
       fprintf(stderr, "Error getting key from cursor: %s\n", db_strerror(ret));
       return -1;
    }
    if ((ret = dbcp->c_close(dbcp)) != 0){
      fprintf(stderr, "Error closing cursor: %s\n", db_strerror(ret));
      return -1;
    }
    return 0;
}

/*-------------------------------------------------------------------------
 * Function:	H5RBDB_environment_open
 * Purpose:	Opens the environment with specified name and dbenv_home
 * Return:	Success:	0.
 *		    Failure:	non-zero integer specifying the encountered error
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_environment_open(DB_ENV** dbenv, const char* dbenv_home){
    int ret;
    u_int32_t env_flags;
    

    INIT_COUNTING
    
    env_flags =	DB_PRIVATE      |	\
                DB_CREATE       |   \
        		DB_RECOVER	    |	\
        		DB_THREAD	    |	\
        		DB_INIT_LOG     |	\
        		DB_INIT_MPOOL   |   \
        		DB_INIT_TXN	    |	\
        		DB_INIT_RETRO;
		
    START_COUNTING
    // create the environment handle
    try(db_env_create(dbenv, 0));
    STOP_COUNTING("Create a new environment")
    
    //try((*dbenv)->set_mp_pagesize(*dbenv, PAGE_SIZE));
    //try((*dbenv)->set_cachesize(*dbenv, 0, CACHE_SIZE, 0));
    
    //(*dbenv)->set_flags(*dbenv, DB_TXN_NOSYNC, 1);
    START_COUNTING
    // open the environment
    try ((*dbenv)->open(*dbenv, dbenv_home, env_flags, 0664));
    STOP_COUNTING("Open the environment")
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_db_handles_database_create
 * Purpose: Creates the db handles database (type HASH) with specified file and DB name.
            The purpose is to store the handles of the opened DBs, so that to leave them open until we close the underlying HDF5 file.
 * Return:  Success:    0.
 *          Failure:    non-zero integer specifying the encountered error
 * Programmer:  Dimokritos Stamatakis
 *              July, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_db_handles_database_create(DB_ENV** dbenv, DB** dbp, const char* file_name, const char* db_name){
    int ret;
    u_int32_t db_flags = 0;
    db_flags = DB_CREATE;
    // create a new database handle
    try(db_create(dbp, *dbenv, 0));
    try((*dbp)->open(*dbp, NULL, file_name, db_name, DB_HASH, db_flags, 0664));
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:	H5RBDB_database open
 * Purpose:	Opens the database with specified file and DB name and creates it if specified.
 * Return:	Success:	0.
 *		    Failure:	non-zero integer specifying the encountered error
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
int H5RBDB_database_open(DB_ENV** dbenv, DB** dbp , DB_TXN** tid, const char* file_name, const char* db_name, H5RBDB_db_type db_type, unsigned char create_db, unsigned char snapshottable) {
    int ret;
    u_int32_t db_flags;
    DBT key, value;
    DB_MPOOLFILE *mpf;

    INIT_COUNTING
    db_flags =         DB_THREAD       |                                \
                        DB_MULTIVERSION |                                \
                        DB_TXN_SNAPSHOT ;

    // set the flags, depending on whether we prefer DB creation or not
    if (create_db){
        db_flags = db_flags | DB_CREATE;
    }
    else {
        db_flags = DB_THREAD | DB_MULTIVERSION;
    }
    // create a new database handle
    try(db_create(dbp, *dbenv, 0));
    
    if (db_type == DB_TYPE_GROUP){
	   //(*dbp)->set_flags(*dbp, DB_DUP);
    }
    // TODO: handle case where group name is the same as file name
    // open the database with given file_name and db_name
    //START_COUNTING
    ret = (*dbp)->open(*dbp, *tid, file_name, db_name, DB_BTREE, db_flags, 0664);
    if (ret != 0){
        printf("Error opening DB %s from file name %s!!: %s\n", db_name, file_name, db_strerror(ret));
        return -1;
    }
    if(create_db){
        //printf("Created DB %s in file with name %s\n", db_name, file_name);
    }
    //STOP_COUNTING("Open DB");
    
    /*// set the specified Database type and throw an error if database type exists and is different than the one specified
    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));
    key.data = DB_KEYS_DB_TYPE;
    key.size = strlen(DB_KEYS_DB_TYPE)+1;
    
    ret = (*dbp)->get(*dbp, *tid, &key, &value, 0);
    */

    if(create_db){
    	memset(&key, 0, sizeof(key));
    	memset(&value, 0, sizeof(value));	

    	key.data = (char*) DB_KEYS_DB_TYPE;
    	key.size = (unsigned int) strlen(DB_KEYS_DB_TYPE)+1;
    	
    	value.data = &db_type;
    	value.size = sizeof(H5RBDB_db_type);
    	try((*dbp)->put(*dbp, *tid, &key, &value, 0));
    }
    
    if (snapshottable){
    	/* Convenience variables */
    	mpf = (*dbp)->get_mpf(*dbp);
    	/* Take snapshots of dbp (in future this will be more convenient) */
    	try(mpf->set_flags(mpf, DB_MPOOL_SNAPSHOT, 1));
    }
    // success
    return 0;
}


/*-------------------------------------------------------------------------
 * Function:    H5RBDB_database_exists
 * Purpose: checks whether specified database exists in the specified environment
 * Return:  Success:    1 if the database exists in current environment, 0 otherwise
 *          Failure:    abort
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
unsigned H5RBDB_database_exists(DB_ENV** dbenv, DB** dbp, DB_TXN** tid, RETROSPECTION_DATA* bite, const char* file_name, const char* db_name){
    int open_res, ret;
    u_int32_t db_flags;

    db_flags =         DB_THREAD       | \
                        DB_MULTIVERSION;

    // create a new database handle
    try(db_create(dbp, *dbenv, 0));
    // open the database with given file_name and db_name
    open_res = (*dbp)->open(*dbp, *tid, file_name, db_name, DB_BTREE, db_flags, 0664);
    // TODO: probably we don't need to commit, since we didn't do any updates!
    //try((*tid)->commit(*tid, 0));
    try((*dbp)->close(*dbp, 0));
    //try((*dbenv)->txn_begin(*dbenv, NULL, tid, DB_TXN_SNAPSHOT));
    //if (bite != NULL)
	//(*tid)->retrospection_data = bite;
    return ((open_res == 0)? 1: 0);
}

/*-------------------------------------------------------------------------
 * Function:    H5RBDB_key_exists
 * Purpose: checks whether specified key exists in the specified database
 * Return:  Success:    1 if the key exists in current database, 0 otherwise
 *          Failure:    abort
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_key_exists(DB** dbp, DB_TXN** tid, const char* key_str){
    DBT key, val;
    int ret;
    char value_buf[512];
    
    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));
    key.data = key_str;
    key.size = (unsigned int) strlen(key_str)+1;
    val.data = value_buf;
    val.ulen = 512;
    val.flags = DB_DBT_USERMEM;
    
    if(*dbp == NULL){
        printf("$$$$$$$$ Oh NO! dbp of key %s is NULL!\n", key_str);
        return -1;
    }
    // we might not want a transactional read, in this case do not dereference tid
    ret = (*dbp)->get(*dbp, (tid == NULL? NULL : *tid), &key, &val, 0);
    if (ret == 0)
        return 1;
    else if (ret == DB_NOTFOUND)
        return 0;
    else { // error!
        fprintf(stderr, "H5RBDB.c: Error checking whether key %s exists: %s\n", key_str, db_strerror(ret));
        return -1;
    }
}


/*-------------------------------------------------------------------------
 * Function:    H5RBDB_get_txn
 * Purpose: returns the current transaction within the specified DB environment, if not committed.
 * Return:  Success:    the current transaction within the specified DB environment, if not committed.
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
void* H5RBDB_get_txn(hid_t loc_id){
    H5RBDB_dbenv_t* dbenv_info;
    DB_TXN* tid = NULL;

    if((dbenv_info = (H5RBDB_dbenv_t*) H5Pget_vol_info(loc_id)) == NULL){
        printf("Error getting DB environment struct!\n");
        return NULL;
    }
    if (dbenv_info->active_txn == 1){
        tid = dbenv_info->tid;
    }
    return (void*)tid;
}

/*-------------------------------------------------------------------------
 * Function:    H5RBDB_txn_begin
 * Purpose: creates a new Transaction in the specified DB environment
 * Return:  Success:    zero.
 *          Failure:    whatever the txn_begin returned
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_txn_begin(hid_t loc_id){
    H5RBDB_dbenv_t* dbenv_info;
    int ret;

    if((dbenv_info = (H5RBDB_dbenv_t*) H5Pget_vol_info(loc_id)) == NULL){
        printf("Error getting DB environment struct!\n");
        return NULL;
    }
    try(dbenv_info->dbenv->txn_begin(dbenv_info->dbenv, NULL, &dbenv_info->tid, 0));
    if ( dbenv_info->in_retrospection == 1)
	dbenv_info->tid->retrospection_data = dbenv_info->bite;
    dbenv_info->active_txn = 1;
    return 0;
 } 
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_txn_commit
 * Purpose: commits the Transaction stored in the specified DB environment
 * Return:  Success:    zero.
 *          Failure:    whatever the txn_commit returned
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_txn_commit(hid_t loc_id){
    H5RBDB_dbenv_t* dbenv_info;
    int ret;

    if((dbenv_info = (H5RBDB_dbenv_t*) H5Pget_vol_info(loc_id)) == NULL){
        printf("Error getting DB environment struct!\n");
        return NULL;
    }
    try(dbenv_info->tid->commit(dbenv_info->tid, 0));
    dbenv_info->active_txn = 0;
    return 0;
 }
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_declare_snapshot
 * Purpose:     declares a snapshot of the specified DB environment
 * Return:  Success:    the declared snapshot id.
 *          Failure:    0
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
u_int32_t H5RBDB_declare_snapshot(hid_t loc_id){
    int ret;
    H5RBDB_dbenv_t* dbenv_info;
    u_int32_t snap_id;
    INIT_COUNTING
    if((dbenv_info = (H5RBDB_dbenv_t*) H5Pget_vol_info(loc_id)) == NULL){
        printf("Error getting DB environment struct!\n");
        return 0;
    }

    if (!dbenv_info->active_txn){
    	fprintf(stderr, "Cannot proceed without a transaction!\n");
    	return 0;
    }
    
    START_COUNTING
    try(__retro_declare_snapshot(dbenv_info->dbenv, dbenv_info->tid, &snap_id, 0));
    STOP_COUNTING("Retro declare snapshot")
    START_COUNTING
    try(dbenv_info->tid->commit(dbenv_info->tid, 0));
    STOP_COUNTING("TXN commit!")
    try(dbenv_info->dbenv->txn_begin(dbenv_info->dbenv, NULL, &dbenv_info->tid, DB_TXN_SNAPSHOT));
    //STOP_COUNTING("Declared snapshot and committed")
    return snap_id;
 }
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_as_of_snapshot
 * Purpose:     runs as_of_snapshot of specified snapshot id in the specified DB environment
 * Return:  Success:    0
 *          Failure:    negative value indicating the error
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_as_of_snapshot(hid_t loc_id, u_int32_t snap_id){
    int ret;
    H5RBDB_dbenv_t* dbenv_info;
    RETROSPECTION_DATA *bite;
    DB* table_catalog_dbp;
   
    if((dbenv_info = (H5RBDB_dbenv_t*) H5Pget_vol_info(loc_id)) == NULL){
        printf("Error getting DB environment struct!\n");
        return -1;
    }
    if(!dbenv_info->active_txn) {
	    printf("Cannot proceed without a transaction!\n");
	    return -1;
    }
    try(retrospection_init(dbenv_info->dbenv, snap_id, &bite));
    
    dbenv_info->tid->retrospection_data = bite;
    dbenv_info->bite = bite;
    dbenv_info->in_retrospection = 1;
    // in order for the as_of snapshot to work properly, we have to open an *existing* table while in retrospection.
    // we do so by opening the DB_TABLES_TABLE_CATALOG table which is always created on file create and exists.
    try(H5RBDB_database_open(&dbenv_info->dbenv, &table_catalog_dbp, &dbenv_info->tid, dbenv_info->db_file, DB_TABLES_TABLE_CATALOG, DB_TYPE_FILE, 0, 1));
    try(table_catalog_dbp->close(table_catalog_dbp, DB_NOSYNC));
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_retro_free
 * Purpose:     frees the retrospection in the DB environment of the file obj 
 *              of the given file id, so all following queries will reflect the 
 *              current state
 * Return:      Success:    0
 *              Failure:    negative value indicating the error
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_retro_free(hid_t loc_id){
    int ret;
    H5RBDB_dbenv_t* dbenv_info;

    if((dbenv_info = (H5RBDB_dbenv_t*) H5Pget_vol_info(loc_id)) == NULL){
        printf("Error getting DB environment struct!\n");
        return NULL;
    }    
    try(retrospection_free(dbenv_info->dbenv, dbenv_info->bite));
    dbenv_info->in_retrospection = 0;
    return 0;
}
