/**
 * Author: Dimokritos Stamatakis
 * The link functions of the Retro Berkeley DB VOL plugin
 * May, 2016
 */

#define H5G_FRIEND      /*suppress error about including H5Gpkg   */
#define H5D_FRIEND      /*suppress error about including H5Dpkg   */
#define H5S_FRIEND      /*suppress error about including H5Spkg   */
#define H5T_FRIEND		/*suppress error about including H5Tpkg	  */

#include <db.h>
#include "dbinc/utils.h"
#include "H5Gpkg.h"             /* Groups      */
#include "H5Dpkg.h"             /* Dataset pkg */
#include "H5Spkg.h"
#include "H5Sprivate.h"
#include "H5Tpkg.h"		/* Datatypes				*/
#include "H5Iprivate.h"		/* IDs			  		*/
#include "H5Tprivate.h"

#include <db.h>
#include <sys/time.h>
#include "H5RBDB.h"
#include "H5RBDB_file.h"
#include "H5RBDB_group.h"
#include "H5RBDB_dataset.h" 
#include "H5RBDB_attr.h"
#include "H5RBDB_utils.h"

/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attr_create
 * Purpose:     Creates a new attribute with the given parameters
 * Return:  Success:    a pointer to the newely created attribute object
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
H5RBDB_attr_t* H5RBDB_attr_create(void* obj, H5I_type_t obj_type, const char* attr_name, H5T_t* type, H5S_t* space, hid_t acpl_id, hid_t dxpl_id){
    DB* dbp;
    DB_TXN* tid;
    H5RBDB_attr_t* attr = NULL;
    H5RBDB_group_t* group;
    H5RBDB_dataset_t* dataset;
    char key_str[128];
    hsize_t attr_size;
    INIT_COUNTING_ATTR
    
    START_COUNTING_ATTR
    switch (obj_type){
        case H5I_GROUP:
            group = (H5RBDB_group_t*) obj;
            dbp = group->dbp;
            if (!group->file_obj->dbenv_info->active_txn){
                printf("Cannot proceed without transaction!\n");
                goto done;
            }
            tid = group->file_obj->dbenv_info->tid;
            // probably just directly store the type->shared->size?
    	    attr_size = type->shared->size;
            if (space->extent.type == H5S_SCALAR) {
        		switch(type->shared->type){
        		    case H5T_INTEGER:
            			//printf("This is an integer attribute\n");
				        break;
        		    case H5T_FLOAT:
            			//printf("This is a float attribute\n");
				        break;
                    case H5T_TIME:
            			//printf("This is a time attribute\n");
            			break;
        		    case H5T_STRING:
            			//printf("This is a string attribute of size %llu\n", type->shared->size);
				        attr_size = type->shared->size;
            			break;
                    default:
                        printf("Warning: This type is not supported!\n");
                        goto done;
        		}
    	    }
    	    else if (space->extent.type == H5S_SIMPLE) {
                switch(type->shared->type) {
                    case H5T_ENUM:
                        //printf("This is an enum attribute of size %llu\n", type->shared->size);
                        attr_size = type->shared->size;
                        break;
                    default:
                        printf("Warning: This type for simple data space is not supported, yet\n");
                }
            }
            else {
    		  printf("Warning: Dataspace type not supported yet!\n");
              goto done;
    	    }
            break;
        case H5I_DATASET:
            dataset = (H5RBDB_dataset_t*) obj;
            break;
        default:
            printf("Warning: Wrong object type!\n");
            goto done;
    }
    attr = H5RBDB_attr_initialize(obj, obj_type, attr_name);
    attr->data_size = attr_size;
    //printf("Attribute storage size: %llu\n", attr->data_size);
    
    H5RBDB_attr_set(attr, space, type);
    H5RBDB_dataspace_nums_t dspace_nums;
    dspace_nums.nelem = attr->dataspace.nelem;
    dspace_nums.rank = attr->dataspace.rank;
    STOP_COUNTING_ATTR("init attr data structures")
    //printf("attr db absolute name: %s\n", attr->absolute_name);
    //H5RBDB_put_attr_metadata_multiple(&dbp, &tid, attr->absolute_name, &attr->dataspace, type, type->shared, attr->data_size);
    START_COUNTING_ATTR
    H5RBDB_put_attr_metadata(&dbp, &tid, attr, type);
    STOP_COUNTING_ATTR("put attr metadata");
    done:
        return attr;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_put_attr_metadata
 * Purpose:     Stores the specified attribute metadata as a struct with a single put operation
                and separate put operations for the dataspace size and max.
 * Return:      Success:    0
 *              Failure:    negative value, specifying the encountered error
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_put_attr_metadata(DB** dbp, DB_TXN** tid, H5RBDB_attr_t* attr, H5T_t* dtype) {
    H5RBDB_attr_metadata_t* attr_metadata;
    char attr_mdata_key [ATTR_NAME_SIZE+36];
    int i;

    attr_metadata = (H5RBDB_attr_metadata_t*) malloc(sizeof(H5RBDB_attr_metadata_t));
    attr_metadata->nelem = attr->dataspace.nelem;
    attr_metadata->rank = attr->dataspace.rank;
    //attr_metadata->dtype = *dtype;
    //attr_metadata->dtype_shared = *(dtype->shared);
    memcpy(&attr_metadata->dtype, dtype, sizeof(H5T_t));
    memcpy(&attr_metadata->dtype_shared, dtype->shared, sizeof(H5T_shared_t));



    attr_metadata->storage_size = attr->data_size;
    memset(attr_mdata_key, 0, ATTR_NAME_SIZE+36);
    sprintf(attr_mdata_key, "mdata_attr_%s", attr->absolute_name);    
    H5RBDB_database_put(dbp, tid, attr_mdata_key, attr_metadata, sizeof(H5RBDB_attr_metadata_t));
    H5RBDB_put_dataspace_multiple(dbp, tid, DSPACE_SIZE, attr_mdata_key, attr->dataspace.size, attr->dataspace.rank);
    H5RBDB_put_dataspace_multiple(dbp, tid, DSPACE_MAX, attr_mdata_key, attr->dataspace.max, attr->dataspace.rank);
    if ( attr->type->shared->u.enumer.value != NULL){ // in some cases this is NULL.
        for (i=0; i < attr->type->shared->u.enumer.nmembs; i++){ // store info about enum members
            memset(attr_mdata_key, 0, ATTR_NAME_SIZE+36);
            sprintf(attr_mdata_key, "mdata_attr_union_enumer_value_%d_%s", i, attr->absolute_name);
            H5RBDB_database_put(dbp, tid, attr_mdata_key, &attr->type->shared->u.enumer.value[i], sizeof(uint8_t));
        } 
        for (i=0; i < attr->type->shared->u.enumer.nmembs; i++){ // store info about enum members
            memset(attr_mdata_key, 0, ATTR_NAME_SIZE+36);
            sprintf(attr_mdata_key, "mdata_attr_union_enumer_name_%d_%s", i, attr->absolute_name);
            H5RBDB_database_put(dbp, tid, attr_mdata_key, attr->type->shared->u.enumer.name[i], strlen(attr->type->shared->u.enumer.name[i]) * sizeof(char) );
        } 
    }

    free(attr_metadata);
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_get_attr_metadata
 * Purpose:     Retrieves the specified attribute's metadata as a struct with a single get operation
                and separate get operations for the dataspace size and max.
 * Return:      Success:    0
 *              Failure:    negative value, specifying the encountered error
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_get_attr_metadata(DB** dbp, DB_TXN** tid, H5RBDB_attr_t* attr) {
    H5RBDB_attr_metadata_t* attr_metadata;
    char attr_mdata_key [ATTR_NAME_SIZE+36];
    int i;
    //printf("Getting metadata for attribute %s\n", attr->name);
    attr_metadata = (H5RBDB_attr_metadata_t*) malloc(sizeof(H5RBDB_attr_metadata_t));
    memset(attr_metadata, 0, sizeof(H5RBDB_attr_metadata_t));
    memset(attr_mdata_key, 0, ATTR_NAME_SIZE+36);
    sprintf(attr_mdata_key, "mdata_attr_%s", attr->absolute_name);
    H5RBDB_database_get(dbp, tid, attr_mdata_key, attr_metadata, sizeof(H5RBDB_attr_metadata_t));
    attr->dataspace.nelem = attr_metadata->nelem;
    attr->dataspace.rank = attr_metadata->rank;
    
    attr->type = (H5T_t*) malloc(sizeof(H5T_t));
    memcpy(attr->type, &attr_metadata->dtype, sizeof(H5T_t));
    attr->type->shared = (H5T_shared_t*) malloc(sizeof(H5T_shared_t));
    memcpy(attr->type->shared, &attr_metadata->dtype_shared, sizeof(H5T_shared_t));
    
    attr->data_size = attr_metadata->storage_size;
    attr->dataspace.size = H5RBDB_get_dataspace_multiple(dbp, tid, DSPACE_SIZE, 
        attr_mdata_key, attr->dataspace.rank);
    attr->dataspace.max = H5RBDB_get_dataspace_multiple(dbp, tid, DSPACE_MAX, 
        attr_mdata_key, attr->dataspace.rank);
    attr->malloced_dtype = 1;
    uint8_t nmembs = attr->type->shared->u.enumer.nmembs;
    //printf("Got enumer.nmembs: %d\n", nmembs);

    // maybe store parent in BDB as well... Not needed for now!
    attr->type->shared->parent = NULL;
    if ( attr->type->shared->u.enumer.value != NULL){ // in some cases this is NULL.
        attr->type->shared->u.enumer.value = (uint8_t*) malloc (nmembs * sizeof(uint8_t));
        const unsigned enumer_name_max = 512;
        attr->type->shared->u.enumer.name = (char**) malloc (nmembs * sizeof(char*));
        for (i=0; i < nmembs; i++){
            attr->type->shared->u.enumer.name[i] = (char*) malloc(enumer_name_max * sizeof(char)) ;
        }
        for (i=0; i < nmembs; i++){ // store info about enum members
            memset(attr_mdata_key, 0, ATTR_NAME_SIZE+36);
            sprintf(attr_mdata_key, "mdata_attr_union_enumer_value_%d_%s", i, attr->absolute_name);
            H5RBDB_database_get(dbp, tid, attr_mdata_key, &attr->type->shared->u.enumer.value[i], sizeof(uint8_t));
        }
        for (i=0; i < nmembs; i++){ // store info about enum members
            // sometimes the enum name is empty but whatever is stored in BDB is not! Attempt to get with max name size if name is empty!
            unsigned len = (strlen(attr->type->shared->u.enumer.name[i]) == 0? enumer_name_max : strlen(attr->type->shared->u.enumer.name[i]));
            memset(attr_mdata_key, 0, ATTR_NAME_SIZE+36);
            //printf("Enum name[%d]: %s\n", i, attr->type->shared->u.enumer.name[i]);
            sprintf(attr_mdata_key, "mdata_attr_union_enumer_name_%d_%s", i, attr->absolute_name);
            H5RBDB_database_get(dbp, tid, attr_mdata_key, attr->type->shared->u.enumer.name[i], len * sizeof(char) );
        } 
    }
    free(attr_metadata);
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_put_attr_metadata_multiple
 * Purpose:     !Deprecated! It is slower to store data items using the DB_MULTIPLE flag, rather than
                storing them as a single put of a single struct.
                Stores the specified attribute metadata with a single put operation under the same
                key (mdata_attr_%attrname%) and multiple values. It is using the DB_MULTIPLE flag.
 * Return:      Success:    0
 *              Failure:    negative value, specifying the encountered error
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_put_attr_metadata_multiple(DB** dbp, DB_TXN** tid, const char* attr_absolute_name, H5RBDB_dataspace_t* dspace, H5T_t* dtype, H5T_shared_t* dtype_shared, hsize_t storage_size){
    char attr_mdata_key [ATTR_NAME_SIZE+16];
    void* p;
    DBT key, val;
    int i, ret;
    char test_buf [452];
    INIT_COUNTING_ATTR

    for (i=0; i<452; i++)
        test_buf[i] = i*3;

    memset(attr_mdata_key, 0, ATTR_NAME_SIZE+16);
    sprintf(attr_mdata_key, "mdata_attr_%s", attr_absolute_name);

    memset(&key, 0, sizeof(key));
    key.data = malloc(ATTR_MDATA_BUF_SIZE);
    key.ulen = ATTR_MDATA_BUF_SIZE;
    memset(&val, 0, sizeof(val));
    val.data = malloc(ATTR_MDATA_BUF_SIZE);
    val.ulen = ATTR_MDATA_BUF_SIZE;

    DB_MULTIPLE_WRITE_INIT(p, &key);
    // we need to write 7 different data items, under same key. So make the bulk buffer
    // contain 7 times the same key. The other way is to write under different keys and 
    // then retrieve all of them, but this retrieves all data items even after the keys we want!
    // See: https://community.oracle.com/message/13893725#13893725
    for (i=0; i<7; i++) {
        sprintf(attr_mdata_key, "mdata_attr_%s_%d", attr_absolute_name, i);
        DB_MULTIPLE_WRITE_NEXT(p, &key, attr_mdata_key, strlen(attr_mdata_key)+1);
    }


    
    START_COUNTING_ATTR
    // write the data items to the val bulk buffer
    DB_MULTIPLE_WRITE_INIT(p, &val);
    DB_MULTIPLE_WRITE_NEXT(p, &val, &dspace->nelem, sizeof(hsize_t));
    DB_MULTIPLE_WRITE_NEXT(p, &val, &dspace->rank, sizeof(unsigned));
    DB_MULTIPLE_WRITE_NEXT(p, &val, &dspace->size, sizeof(dspace->rank * sizeof(hsize_t)));
    DB_MULTIPLE_WRITE_NEXT(p, &val, &dspace->max, sizeof(dspace->rank * sizeof(hsize_t)));
    DB_MULTIPLE_WRITE_NEXT(p, &val, dtype, sizeof(H5T_t));
    DB_MULTIPLE_WRITE_NEXT(p, &val, dtype_shared, sizeof(H5T_shared_t));
    DB_MULTIPLE_WRITE_NEXT(p, &val, &storage_size, sizeof(hsize_t));
    STOP_COUNTING_ATTR("initializing bulk buffer")
    //perform the single DB put operation to write the entire bulk buffer
    START_COUNTING_ATTR
    try((*dbp)->put(*dbp, *tid, &key, &val, DB_MULTIPLE));
    STOP_COUNTING_ATTR("DB_MULTIPLE put")
    free(key.data);
    free(val.data);
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attr_open
 * Purpose:     Opens an attribute with the given parameters
 * Return:  Success:    a pointer to the opened attribute object
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
H5RBDB_attr_t* H5RBDB_attr_open(void* obj, H5I_type_t obj_type, const char* attr_name, hid_t dxpl_id){
    H5RBDB_group_t* group;
    H5RBDB_dataset_t* dataset;
    H5RBDB_attr_t* attr = NULL;
    DB* dbp;
    DB_TXN* tid;
    char key_str[128];
    int ret;
    switch (obj_type){
        case H5I_GROUP:
            group = (H5RBDB_group_t*) obj;
            dbp = group->dbp;
            if (!group->file_obj->dbenv_info->active_txn){
                printf("Cannot proceed without transaction!\n");
                goto done;
            }
            tid = group->file_obj->dbenv_info->tid;
            break;
        case H5I_DATASET:
            dataset = (H5RBDB_dataset_t*) obj;
            break;
        default:
            printf("Wrong object type!\n");
            goto done;
    }
    attr = H5RBDB_attr_initialize(obj, obj_type, attr_name);
    try(H5RBDB_get_attr_metadata(&dbp, &tid, attr));
    done:
        return attr;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attr_read
 * Purpose:     Reads data from the specified attribute
 * Return:  Success:    0
 *          Failure:    negative value specifying error
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_attr_read(H5RBDB_attr_t* attr,  H5T_t *mem_type, void* buf, hid_t dxpl_id){
    size_t val_size;
    int ret;
    switch(mem_type->shared->type){
        case H5T_INTEGER:
            val_size = mem_type->shared->size;
            try(H5RBDB_database_get(&attr->dbp, &attr->file_obj->dbenv_info->tid, attr->absolute_name, buf, val_size));
            //printf("Read attribute %s, absolute name %s, value %d, size %u\n", attr->name, attr->absolute_name, *((int32_t*)buf), val_size);
            break;
        case H5T_FLOAT:
            val_size = mem_type->shared->size;
            try(H5RBDB_database_get(&attr->dbp, &attr->file_obj->dbenv_info->tid, attr->absolute_name, buf, val_size));
            //printf("Read attribute %s, absolute name %s, value %f, size %u\n", attr->name, attr->absolute_name, *((float*)buf), val_size);
            break;
        case H5T_STRING:
        // TODO: Fix this, so that to update the value size when we update the attribute!!
            val_size = mem_type->shared->size + 100;
            try(H5RBDB_database_get(&attr->dbp, &attr->file_obj->dbenv_info->tid, attr->absolute_name, buf, val_size));
            //printf("Read attribute %s, absolute name %s, value %s, size %u\n", attr->name, attr->absolute_name, (char*)buf, val_size);
            break;
        case H5T_ENUM:
            val_size = mem_type->shared->size;
            try(H5RBDB_database_get(&attr->dbp, &attr->file_obj->dbenv_info->tid, attr->absolute_name, buf, val_size));
            //printf("Read attribute of type enum with size %llu and value %u\n", val_size, *( (short*) buf) );
            break;
        default:
             printf("Warning: This type is not supported!\n");
             return -1;
    }
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attr_write
 * Purpose:     Writes data to the specified attribute
 * Return:  Success:    0
 *          Failure:    negative value specifying error
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_attr_write(H5RBDB_attr_t* attr, H5T_t* mem_type, void* buf, hid_t dxpl_id){
    size_t val_size;
    int ret;
    switch(mem_type->shared->type){
        case H5T_INTEGER:
            val_size = mem_type->shared->size;
            //printf("Writing attribute %s, absolute name %s, value %d, size %u\n", attr->name, attr->absolute_name, *((int32_t*)buf), val_size);
            try(H5RBDB_database_put(&attr->dbp, &attr->file_obj->dbenv_info->tid, attr->absolute_name, buf, val_size));
            break;
        case H5T_FLOAT:
            val_size = mem_type->shared->size;
            //printf("Write this float attribute\n");
            try(H5RBDB_database_put(&attr->dbp, &attr->file_obj->dbenv_info->tid, attr->absolute_name, buf, val_size));
            break;
        case H5T_STRING:
            val_size = mem_type->shared->size;
            if (val_size != attr->data_size){ // storage size is different than the one stored. Update it!
		          //printf(")))) Stored storage size is different than given! Given: %u, stored: %u, in attr->type->shared->size: %u\n", val_size, attr->data_size, attr->type->shared->size);
		          attr->data_size = val_size;
		          attr->type->shared->size = val_size;
		          assert(H5RBDB_put_attr_metadata(&attr->dbp, &attr->file_obj->dbenv_info->tid, attr, mem_type) == 0);
		          //printf(")))) After storing! Given: %u, stored: %u, in attr->type->shared->size: %u\n", val_size, attr->data_size, attr->type->shared->size);
	        }
	        // TODO: update the val_size in the attr metadata, so that to find it when opening!!
            try(H5RBDB_database_put(&attr->dbp, &attr->file_obj->dbenv_info->tid, attr->absolute_name, buf, val_size));
            break;
        case H5T_ENUM:
            val_size = mem_type->shared->size;
            //printf("Writing attribute of type enum with size %llu and value %u\n", val_size, *( (short*) buf) );
            try(H5RBDB_database_put(&attr->dbp, &attr->file_obj->dbenv_info->tid, attr->absolute_name, buf, val_size));
            break;
        default:
             printf("Warning: This type is not supported!\n");
             return -1;
    }
    return 1;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attr_exists
 * Purpose:     Checks whether specified attribute exists
 * Return:  Success:    1 if attribute exists, 0 otherwise
 *          Failure:    negative value specifying error
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
htri_t H5RBDB_attr_exists(void* obj, H5I_type_t obj_type, char* attr_name, hid_t dxpl_id){
    H5RBDB_group_t* group;
    H5RBDB_dataset_t* dataset;
    DB* dbp;
    DB_TXN* tid;
    char* key_name;
    char* container_obj;
    htri_t exists = -1;

    switch (obj_type){
        case H5I_GROUP:
            group = (H5RBDB_group_t*) obj;
            dbp = group->dbp;
            if (!group->file_obj->dbenv_info->active_txn){
                printf("Cannot proceed without transaction!\n");
                goto done;
            }
            tid = group->file_obj->dbenv_info->tid;
            container_obj = "group";
            key_name = H5RBDB_attrname_add_prefix(attr_name, container_obj);
            break;
        case H5I_DATASET:
            dataset = (H5RBDB_dataset_t*) obj;
            container_obj = "dataset";
            printf("attr_exists for dataset attributes is not yet supported!\n");
            goto done;
        default:
            printf("Wrong object type!\n");
            goto done;
    }
    exists = H5RBDB_key_exists(&dbp, &tid, key_name);
    done:
        return exists;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attr_get_type
 * Purpose:     Gets the datatype of the specified attribute
 * Return:  Success:    The location id of the datatype, must correspond to 
 *                      an object of type H5T_t.
 *          Failure:    negative value specifying error
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
hid_t H5RBDB_attr_get_type(H5RBDB_attr_t* attr){
	hid_t ret_id;
	H5T_t* dt;
	//printf("Type of attribute datatype: %d\n", attr->type->shared->type);
	/*
	* Copy the attribute's datatype.  If the type is a named type then
	* reopen the type before returning it to the user. Make the type
	* read-only.
	*/
	if(NULL == (dt = H5T_copy(attr->type, H5T_COPY_REOPEN))){
	    printf("Unable to copy datatype!\n");
	    return -1;
	}

	/* Mark any datatypes as being in memory now */
	if(H5T_set_loc(dt, NULL, H5T_LOC_MEMORY) < 0){
	    printf("Invalid datatype location!\n");
	    return -1;
	}
	/* Lock copied type */
	//if(H5T_lock(dt, FALSE) < 0) {
	//    printf("Unable to lock transient datatype!\n");
	//    return -1;
	//}
	if((ret_id = H5I_register(H5I_DATATYPE, dt, TRUE)) < 0){
		return -1;
	}
	//printf("In H5RBDB_get_type: Register returns %ld\n", ret_id);
	return ret_id;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attr_iterate
 * Purpose:     Iterates through all attributes of specified obj and calls 
 *              the specified op function for each attribute
 * Return:      Success:    0
 *              Failure:    negative value specifying error
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_attr_iterate(void* obj, hid_t obj_id, H5I_type_t obj_type, H5_index_t idx_type, H5_iter_order_t order,  hsize_t * idx, H5A_operator2_t op, void* op_data){
    H5RBDB_group_t* group;
    H5RBDB_dataset_t* dataset;
    herr_t res = 0;
    int ret;
    
    switch (obj_type){
        case H5I_GROUP:
            group = (H5RBDB_group_t*) obj;
            if (!group->file_obj->dbenv_info->active_txn){
                printf("Cannot proceed without transaction!\n");
		res = -1;
                goto done;
            }
            try(H5RBDB_database_cursor_attr_iter_apply(group->file_obj->dbenv_info, &group->dbp, &group->file_obj->dbenv_info->tid, obj_id, "__a", 3, op, group->file_obj->name, op_data));
            break;
        case H5I_DATASET:
            dataset = (H5RBDB_dataset_t*) obj;
	        printf("Not yet supported to iterate over attributes of a dataset!\n");
	        res = -1;
	        goto done;
        default:
            printf("Wrong object type!\n");
            res = -1;
            goto done;
    }
    done:
        return res;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_database_cursor_attr_iter_apply
 * Purpose: Uses a cursor to iterate through the keys of the given prefix of length comp_len and
 *           calls the apply function, which is the function passed in H5Literate (in the same format)
 * Return:  Success:    0.
 *          Failure:    non-zero integer specifying the encountered error, aborts if any BDB errors
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_database_cursor_attr_iter_apply(H5RBDB_dbenv_t* dbenv_info, DB** dbp, DB_TXN** tid, hid_t obj_id, char* prefix, unsigned comp_len, H5A_operator2_t apply, char* file_name, void* op_data){
    int ret;
    unsigned i;
    DBC* dbcp;
    DB_TXN* inner_txn;
    DBT key, val;
    const char* name;
    unsigned use_inner_txn = 0;
    H5A_info_t attr_info;
    INIT_COUNTING_ATTR
    
    memset(&key, 0, sizeof(key));
    memset(&val, 0, sizeof(val));
    key.data = prefix;
    key.size = strlen(prefix)+1;
    //printf("Iterating over attributes! Prefix is: %s\n", prefix);
    if (use_inner_txn)
	   try(dbenv_info->dbenv->txn_begin(dbenv_info->dbenv, NULL, &inner_txn, DB_TXN_SNAPSHOT));
    if(use_inner_txn)
    	try((*dbp)->cursor(*dbp, inner_txn, &dbcp, 0));
    else
	try((*dbp)->cursor(*dbp, *tid, &dbcp, 0));
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
    name = H5RBDB_attrname_clear_prefix((char *)key.data);
    //printf("Found attribute %s\n", name);

    if ( (*apply)(obj_id, name, &attr_info, op_data) != 0){
	    printf("Error iterating over attributes!\n");
	    return -1;
    }
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
    name = H5RBDB_attrname_clear_prefix((char *)key.data);
    if ((*apply)(obj_id, name, &attr_info, op_data) != 0){
	    printf("Error iterating over attributes!\n");
	    return -1;
    }
    } while (ret == 0);
    try(dbcp->c_close(dbcp));
    if(use_inner_txn)
    	try(inner_txn->commit(inner_txn, 0));    
    return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attr_close
 * Purpose:     Closes the specified attribute
 * Return:  Success:    0
 *          Failure:    negative value specifying error
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
herr_t H5RBDB_attr_close(H5RBDB_attr_t* attr){
	//printf("Closing attr... %s\n", attr->name);
	if (attr->malloced_dtype) {
        free(attr->type->shared);
    	free(attr->type);
    }
	free(attr->dataspace_key);
	free(attr);
	//printf("OK!\n");
	return 0;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attr_initialize
 * Purpose:     Allocates memory and initializes the object for an attribute
 * Return:  Success:    pointer to the allocated attribute object
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
static H5RBDB_attr_t* H5RBDB_attr_initialize(void* container_obj, H5I_type_t obj_type, const char* attr_name){
    H5RBDB_attr_t* new_attr_obj;
    const char* prefix;
    const char* container_obj_name;
    
    new_attr_obj = (H5RBDB_attr_t*) malloc(sizeof(H5RBDB_attr_t));
    memset(new_attr_obj, 0, sizeof(H5RBDB_attr_t));
    new_attr_obj->name = attr_name;

    new_attr_obj->obj_type = obj_type;
    new_attr_obj->container_obj = container_obj;

    switch(obj_type){
        case H5I_GROUP:
            container_obj_name = "group";
            new_attr_obj->file_obj = ((H5RBDB_group_t*) container_obj)->file_obj;
            new_attr_obj->dbp = ((H5RBDB_group_t*) container_obj)->dbp;
            break;
        case H5I_DATASET:
            container_obj_name = "dataset";
            new_attr_obj->file_obj = ((H5RBDB_dataset_t*) container_obj)->file_obj;
            new_attr_obj->dbp = ((H5RBDB_dataset_t*) container_obj)->dbp;
            break;
        default:
            printf("This attribute object type is not supported!\n");
    }

    new_attr_obj->absolute_name = H5RBDB_attrname_add_prefix(attr_name, container_obj_name);
    new_attr_obj->dataspace_key = H5RBDB_dataspacename_add_prefix(new_attr_obj->absolute_name);
    return new_attr_obj;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attr_set
 * Purpose:     Sets attribute parameters for dataspace and datatype
 * Return:  Success:    
 *          Failure:    
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
static void H5RBDB_attr_set(H5RBDB_attr_t* attr, H5S_t* space, H5T_t* type){
    H5RBDB_dataspace_t dataspace;
    dataspace.nelem = space->extent.nelem;
    dataspace.rank = space->extent.rank;
    dataspace.size = space->extent.size;
    dataspace.max = space->extent.max;
    attr->dataspace = dataspace;
    attr->type = type;
    attr->type->shared->u.atomic = type->shared->u.atomic;
    attr->type->shared->u.compnd = type->shared->u.compnd;
    attr->type->shared->u.enumer = type->shared->u.enumer;
    attr->type->shared->u.vlen = type->shared->u.vlen;
    attr->type->shared->u.opaque = type->shared->u.opaque;
    attr->type->shared->u.array = type->shared->u.array;
    attr->type->shared->u.enumer.value = type->shared->u.enumer.value;
    attr->malloced_dtype = 0;
}
