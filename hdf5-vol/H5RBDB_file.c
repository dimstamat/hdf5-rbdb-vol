/**
 * Author: Dimokritos Stamatakis
 * The file functions of the Retro Berkeley DB VOL plugin
 * October, 2015
 */

#define H5G_FRIEND      /*suppress error about including H5Gpkg   */

#include <db.h>
#include "dbinc/utils.h"
#include "H5Gpkg.h"     /* Groups               */
#include "H5Iprivate.h"
#include <db.h>
#include <sys/time.h>

#include "H5RBDB.h"
#include "H5RBDB_utils.h"
#include "H5RBDB_file.h"


// Deprecated! Now we use the VOL info to store DB environment through file access property list (H5RBDB_dbenv_t)
#if 0
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_get_env_file
 * Purpose: Gets the object storing the DB environment for the specified environment name
 * Return:  Success:    pointer to the object storing the DB environment (H5RBDB_dbenv_file_t* )
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
H5RBDB_dbenv_file_t * H5RBDB_get_env_file(DB_ENV** dbenv, DB_TXN** tid, const char* env_name){
    DB *dbenv_file_dbp;
    unsigned dbenv_file_exists;
    int ret;
    herr_t status;
    hid_t plist_id;
    hid_t dbenv_file_locid, fapl_id;
    H5VL_object_t* obj;
    H5RBDB_dbenv_file_t* dbenv_file_obj;
    
    // now get the db environment file
    dbenv_file_exists = H5RBDB_database_exists(dbenv, &dbenv_file_dbp, tid, NULL, env_name, DB_TABLES_DBENV_FILE);
    if (!dbenv_file_exists){
	    H5RBDB_database_open(dbenv, &dbenv_file_dbp, tid, env_name, DB_TABLES_DBENV_FILE, DB_TYPE_FILE, 1, 0);
        printf("Created new table for db environment file!\n");
    }
    else {
	    H5RBDB_database_open(dbenv, &dbenv_file_dbp, tid, env_name, DB_TABLES_DBENV_FILE, DB_TYPE_FILE, 0, 0);
    }
    // create a new property to specify that we want a special case of H5Fcreate that will only create the file data stucture
    if ((plist_id = H5Pcreate(H5P_FILE_CREATE)) < 0){
    	printf("Error creating property list!\n");
    	return NULL;
    }
    if ((status = H5Pinsert2( plist_id, "type_dbenv_file", 16, (char*) "type_dbenv_file", NULL, NULL, NULL, NULL, NULL, NULL )) < 0){
    	printf("Error inserting new property!\n");
    	return NULL;
    }
    fapl_id = H5Pcreate (H5P_FILE_ACCESS);
    status = H5RBDB_set_vol_RBDB(fapl_id, "/home/dimos/projects/hdf5/h5app", "h5app");

    // get the dbenv file loc id
    ret = H5RBDB_database_get(&dbenv_file_dbp, tid, DB_KEYS_DBENV_FILE_LOCID, &dbenv_file_locid, sizeof(hid_t));
    if (ret == DB_NOTFOUND){
    	printf("dbenv file loc id not found! Create one!\n");
    	dbenv_file_locid = H5Fcreate(FILENAME_DB_ENVIRONMENT, H5F_ACC_TRUNC, plist_id, fapl_id);
    	H5RBDB_database_put(&dbenv_file_dbp, tid, DB_KEYS_DBENV_FILE_LOCID, &dbenv_file_locid, sizeof(hid_t));
    }
    if(NULL == (obj = (H5VL_object_t *)H5I_object(dbenv_file_locid)) ){ // dbenv file object does not exist, create it!
    	printf("Must create new dbenv file object!\n");
    	dbenv_file_locid = H5Fcreate(FILENAME_DB_ENVIRONMENT, H5F_ACC_TRUNC, plist_id, fapl_id);
    	obj = (H5VL_object_t *)H5I_object(dbenv_file_locid);
    	if (obj == NULL){
    		printf("Cannot get dbenv file object!\n");
    		return NULL;
    	}
    	H5RBDB_database_put(&dbenv_file_dbp, tid, DB_KEYS_DBENV_FILE_LOCID, &dbenv_file_locid, sizeof(hid_t));
    }
    dbenv_file_obj = (H5RBDB_dbenv_file_t*) obj->vol_obj;
    
    if (dbenv_file_obj->is_initialized){
    	printf("dbenv file already exists and is initialized!\n");
    	try((*tid)->commit(*tid, 0));
        printf("txn committed!\n");
        try(dbenv_file_dbp->close(dbenv_file_dbp, 0));
        try((*dbenv)->close((*dbenv), 0));
        printf("dbenv closed!\n");
        *dbenv = dbenv_file_obj->dbenv;
    	if (dbenv_file_obj->active_txn == 1)
            *tid = dbenv_file_obj->tid;
        else {
            try((*dbenv)->txn_begin(*dbenv, NULL, tid, DB_TXN_SNAPSHOT));
            dbenv_file_obj->tid = *tid;
            dbenv_file_obj->active_txn = 1;
        }
    }
    else {
        printf("dbenv file is not initialized! Initialize it!\n");
    	try((*tid)->commit(*tid, 0));
        printf("txn committed\n");
        try(dbenv_file_dbp->close(dbenv_file_dbp, 0));
        printf("dbenv file db closed\n");
        try((*dbenv)->txn_begin(*dbenv, NULL, tid, DB_TXN_SNAPSHOT));
        printf("new txn begins\n");
        dbenv_file_obj->dbenv = *dbenv;
        dbenv_file_obj->tid = *tid;
        dbenv_file_obj->is_initialized = 1;
        dbenv_file_obj->locid = dbenv_file_locid;
    }
    dbenv_file_obj->open_files++;
    return dbenv_file_obj;
}
#endif

/*-------------------------------------------------------------------------
 * Function:	H5RBDB_file_create
 * Purpose:	Creates a Database environment in BDB that represents an application with multiple HDF5 files.
 *          Each HDF5 file is stored in separate physical file, in the same Database environment.
 * Return:	Success:	pointer to the new object for the file
 *		    Failure:	NULL
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
H5RBDB_file_t * H5RBDB_file_create(const char* name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id){
    DB *dbp, *g_dbp, * table_catalog_dbp, *db_handles_dbp;
    DB_ENV *dbenv;
    DB_TXN *tid;
    const char* fname_fixed;
    const char* gname_fixed;
    int ret;
    H5RBDB_file_t* new_file_obj;
    const char* db_file;
    char db_handles_dbname[256];
    unsigned table_catalog_exists;
    hid_t pexists;
    H5RBDB_dbenv_t* dbenv_info;
    INIT_COUNTING_FILE

    if ( (dbenv_info = (H5RBDB_dbenv_t*) H5Pget_vol_info(fapl_id)) == NULL){
        printf("Error getting DB environment struct!\n");
        return NULL;
    }
    dbenv = dbenv_info->dbenv;
    db_file = dbenv_info->db_file;
    //printf("Is initialized!? %u\n", dbenv_info->is_initialized);
    fname_fixed = H5RBDB_filename_add_prefix(name);
    
    if (!dbenv_info->active_txn){
	    printf("Cannot proceed without a transaction!\n");
	    return NULL;
    }
    tid = dbenv_info->tid;
    
    // check if it is the first time we create the environment
    // we need to know so that to set the DB_SNAPSHOT flag
    // in the mpool file or not. If we set it multiple times in an existing environment it crashes.
    // also we check at the end, because it crashes if we check at the beginning...
    table_catalog_exists = H5RBDB_database_exists(&dbenv, &table_catalog_dbp, &tid, NULL, db_file, DB_TABLES_TABLE_CATALOG);
    if(! table_catalog_exists){
        //printf("######### Creating DB for table catalog!\n");
        try(H5RBDB_database_open(&dbenv, &table_catalog_dbp, &tid, db_file, DB_TABLES_TABLE_CATALOG, DB_TYPE_FILE, 1, 1));
        try(tid->commit(tid, 0));
        try(table_catalog_dbp->close(table_catalog_dbp, 0));
        //printf("######### Closing DB for table catalog!\n");
	   try(dbenv->txn_begin(dbenv, NULL, &tid, DB_TXN_SNAPSHOT));
    }

    // create a new Database which will contain the new HDF5 file in the specified environment
    //printf("######### Creating DB for file!\n");
    H5RBDB_database_open(&dbenv, &dbp, &tid, db_file, fname_fixed, DB_TYPE_FILE, 1, 0);
    //try(tid->commit(tid, 0));
    //try(dbenv->txn_begin(dbenv, NULL, &tid, DB_TXN_SNAPSHOT));
    
    // put the property lists in database
    //try(H5RBDB_put_hid_t(dbp, tid, "fcpl_id", fcpl_id));
    //try(H5RBDB_put_hid_t(dbp, tid, "fapl_id", fapl_id));
    //try(H5RBDB_put_hid_t(dbp, tid, "dxpl_id", dxpl_id));
    
    gname_fixed = H5RBDB_groupname_add_prefix("/", name);
    //printf("Creating root group db...\n");
    // create a database for root group inside the new file
    //printf("######### Creating DB for root group!\n");
    H5RBDB_database_open(&dbenv, &g_dbp, &tid, db_file, gname_fixed, DB_TYPE_GROUP, 1, 0);
    //try(tid->commit(tid, 0));
    //try(dbenv->txn_begin(dbenv, NULL, &tid, DB_TXN_SNAPSHOT));
    
    //try(tid->commit(tid, 0));
    //try(dbenv->txn_begin(dbenv, NULL, &tid, DB_TXN_SNAPSHOT));


    // create the db handle database, a HASH database for storing the handles of the opened DBs
    memset(db_handles_dbname, 0, 256);
    sprintf(db_handles_dbname, "db_handles_%s_%s", db_file, name);
    
    //TODO: Make db handels DB in-memory! file parameter should be NULL!
    // In-memory databases never intended to be preserved on disk may be created by setting the file parameter to NULL. Whether other threads of control can access this database is driven entirely by whether the database parameter is set to NULL.
    //START_COUNTING_FILE
    H5RBDB_db_handles_database_create(&dbenv, &db_handles_dbp, db_file, db_handles_dbname);
    //H5RBDB_db_handles_database_create(&dbenv, &db_handles_dbp, NULL, db_handles_dbname);
    //ret = dbenv->dbremove(dbenv, tid, dbenv_info->db_file, db_handles_dbname, 0);
    //printf("db remove returns: %s\n", db_strerror(ret));
    //STOP_COUNTING_FILE("create DB handles DB")
    
    dbenv_info->tid = tid;
    dbenv_info->open_files++;

    new_file_obj = (H5RBDB_file_t*) malloc(sizeof(H5RBDB_file_t));
    memset(new_file_obj, 0, sizeof(H5RBDB_file_t));
    new_file_obj->dbenv_info = dbenv_info;
    new_file_obj->db_handles_dbp = db_handles_dbp;
    new_file_obj->db_handles_dbname = strdup(db_handles_dbname);
    new_file_obj->dbenv_info_initialized = 1;
    new_file_obj->dbenv_info->dbenv = dbenv;
    new_file_obj->dbenv_info->in_retrospection = 0;
    new_file_obj->dbp = dbp;
    new_file_obj->root_dbp = g_dbp;
    new_file_obj->name = strdup(name);
    new_file_obj->flags = flags;
    new_file_obj->fcpl_id = fcpl_id;
    new_file_obj->fapl_id = fapl_id;
    new_file_obj->dxpl_id = dxpl_id;
    
    return new_file_obj;
}
/*-------------------------------------------------------------------------
 * Function:	H5RBDB_file_open
 * Purpose:	Opens a Database in BDB that represents a HDF5 file.
 * Return:	Success:	pointer to the file object (H5RBDB_file_t* )
 *		    Failure:	NULL
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
H5RBDB_file_t * H5RBDB_file_open(const char* name, unsigned flags, hid_t fapl_id, hid_t dxpl_id){
    DB *dbp, *g_dbp, *db_handles_dbp;
    DB_ENV *dbenv;
    DB_TXN *tid;
    int ret;
    H5RBDB_file_t* file_obj;
    H5RBDB_dbenv_t* dbenv_info;
    const char* db_file;
    const char* gname_fixed;
    const char* fname_fixed;
    char db_handles_dbname [256];
    
    if ( (dbenv_info = (H5RBDB_dbenv_t*) H5Pget_vol_info(fapl_id)) == NULL){
        printf("Error getting DB environment struct!\n");
        return NULL;
    }
    dbenv = dbenv_info->dbenv;
    db_file = dbenv_info->db_file;
    
    if (!dbenv_info->active_txn){
	    printf("Cannot proceed without a transaction!\n");
	    return NULL;
    }
    tid = dbenv_info->tid;
    
    file_obj = (H5RBDB_file_t*) malloc(sizeof(H5RBDB_file_t));
    memset(file_obj, 0, sizeof(H5RBDB_file_t));
    fname_fixed = H5RBDB_filename_add_prefix(name);
    
    H5RBDB_database_open(&dbenv, &dbp, &tid, db_file, fname_fixed, DB_TYPE_FILE, 0, 0);

    gname_fixed = H5RBDB_groupname_add_prefix("/", name);
    // open the database of the root group inside the opened file
    H5RBDB_database_open(&dbenv, &g_dbp, &tid, db_file, gname_fixed, DB_TYPE_GROUP, 0, 0);
    // create the db handle database, a HASH database for storing the handles of the opened DBs
    memset(db_handles_dbname, 0, 256);
    sprintf(db_handles_dbname, "db_handles_%s_%s", db_file, name);
    H5RBDB_db_handles_database_create(&dbenv, &db_handles_dbp, db_file, db_handles_dbname);
    
    dbenv_info->open_files++;
    dbenv_info->tid = tid;
    
    file_obj->dbenv_info = dbenv_info;
    file_obj->db_handles_dbp = db_handles_dbp;
    file_obj->db_handles_dbname = strdup(db_handles_dbname);
    file_obj->dbenv_info_initialized = 1;
    file_obj->root_dbp = g_dbp;
    file_obj->name = strdup(name);
    file_obj->flags = flags;
    file_obj->fapl_id = fapl_id;
    file_obj->dxpl_id = dxpl_id;
    file_obj->dbp = dbp;
    return file_obj;
}

/*-------------------------------------------------------------------------
 * Function:	H5RBDB_file_close
 * Purpose:	Closes the Database in BDB that represents a HDF5 file.
 * Return:	Success:	0
 *		Failure:        -1
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *-------------------------------------------------------------------------*/
int H5RBDB_file_close(H5RBDB_file_t* file, hid_t dxpl_id){
    DB *dbp, *root_dbp, *db_handles_dbp;
    DB_ENV *dbenv;
    DB_TXN* tid;
    herr_t ret;
    unsigned active_txn;
    INIT_COUNTING_FILE

    // avoid compiler warnings!
    dxpl_id = dxpl_id;

    // retrieve the DB handle of the already opened db
    dbp = file->dbp;
    root_dbp = file->root_dbp;
    
    if (!file->dbenv_info->active_txn){
	    printf("Cannot proceed without a transaction!\n");
	    return -1;
    }
    // file_close was called after everything was done (even after client calls file_close), so make sure to not attempt 
    // to close the Database if the DB handles are NULL and file name is not valid! Sometimes even with non NULL DB handles it is invalid file obj!
    if (file->dbenv_info != NULL && file->dbenv_info_initialized == 1 && dbp != NULL && strlen(file->name)>0 ){
        dbenv = file->dbenv_info->dbenv;
        tid = file->dbenv_info->tid;
        active_txn = file->dbenv_info->active_txn;
	// close the db handles DB
	//printf("######### Closing DB for db handles with name %s!\n", file->db_handles_dbname);
    try(file->db_handles_dbp->close(file->db_handles_dbp, 0));
	//printf("OK!\n");
    //printf("Removing Hashtable database %s from file %s\n", file->db_handles_dbname, file->dbenv_info->db_file);
	//START_COUNTING_FILE
	// Probably we need to use transactions in order to remove the db handles table.
    // We get the following error if we try to remove it:
    // src/retro/retro_retrospection.c:889: __retro_indirect: Assertion `(ret = (page_table_get(&retrospection_data->page_table, fileid, pageno, snap_id, &m))) == 0' failed.
    // No need to remove the db handles table! It is in memory only. Make sure there is no other file trying to access the db handle!
    // Maybe mark it as invalid.
    //ret = dbenv->dbremove(dbenv, tid, file->dbenv_info->db_file, file->db_handles_dbname, 0);
    //printf("db remove returns: %s\n", db_strerror(ret));
	//assert(ret == 0);
    //STOP_COUNTING_FILE("remove db handles db")
	try(tid->commit(tid, 0));
	// close the database
	try(dbp->close(dbp, 0));
	if (root_dbp != NULL){
	// close the root group database
		try(root_dbp->close(root_dbp, 0));
	}
	try(dbenv->txn_begin(dbenv, NULL, &tid, DB_TXN_SNAPSHOT));
	file->dbenv_info->tid = tid;
	file->dbenv_info->open_files--;
        
        // close all opened DBs! Iterate through all records of the DB handles DB!
	free(file->name);
	memset(file, 0, sizeof(H5RBDB_file_t));
        free(file);
    }
    return 0;
}
