/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * The Retro BDB plugin                                                      *
 * Created by Dimokritos Stamatakis <dimos@brandeis.edu>                     *
 * Research project with Liuba Shrira <liuba@brandeis.edu>                   *
 * Brandeis University, Waltham, MA                                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Programmer:  Dimokritos Stamatakis <dimos@brandeis.edu>
 *              October, 2015
 *
 * Purpose:	The Retro BDB VOL plugin maps the Data Model into Berkeley DB and uses one DB table per HDF5 file.
 */

#define H5A_FRIEND		/*suppress error about including H5Apkg	  */
#define H5D_FRIEND		/*suppress error about including H5Dpkg	  */
#define H5F_FRIEND		/*suppress error about including H5Fpkg	  */
#define H5G_FRIEND		/*suppress error about including H5Gpkg   */
#define H5L_FRIEND		/*suppress error about including H5Lpkg   */
#define H5O_FRIEND		/*suppress error about including H5Opkg	  */
#define H5R_FRIEND		/*suppress error about including H5Rpkg	  */
#define H5T_FRIEND		/*suppress error about including H5Tpkg	  */

/* Interface initialization */
#define H5_INTERFACE_INIT_FUNC	H5VL_RetroBDB_init_interface

#include "H5private.h"		/* Generic Functions			*/
#include "H5Apkg.h"             /* Attribute pkg                        */
#include "H5Dpkg.h"             /* Dataset pkg                          */
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5Fpkg.h"             /* File pkg                             */
#include "H5Gpkg.h"		/* Groups		  		*/
#include "H5HGprivate.h"	/* Global Heaps				*/
#include "H5Iprivate.h"		/* IDs			  		*/
#include "H5Lpkg.h"             /* links headers			*/
#include "H5MFprivate.h"	/* File memory management		*/
#include "H5MMprivate.h"	/* Memory management			*/
#include "H5Opkg.h"             /* Object headers			*/
#include "H5Pprivate.h"		/* Property lists			*/
#include "H5Rpkg.h"		/* References   			*/
#include "H5SMprivate.h"	/* Shared Object Header Messages	*/
#include "H5Tpkg.h"		/* Datatypes				*/
#include "H5VLprivate.h"	/* VOL plugins				*/
#include "H5VLRetroBDB.h"         /* RetroBDB VOL plugin			*/

#include "H5RBDB.h"
#include "H5RBDB_file.h"
#include "H5RBDB_group.h"
#include "H5RBDB_dataset.h"
#include "H5RBDB_attr.h"
#include "H5RBDB_link.h"
#include "H5DimLog.h"



/*
 * The vol identification number.
 */
static hid_t H5VL_RETROBDB_g = 1;


/* Prototypes */
static H5F_t *H5VL_RetroBDB_get_file(void *obj, H5I_type_t type);

static herr_t H5VL_RetroBDB_initialize(void);

static herr_t H5VL_RetroBDB_terminate(hid_t vpl_id);

herr_t H5VL_set_vol_RBDB(hid_t fapl_id, const char* dbenv_home, const char* db_file);

herr_t H5VL_terminate_vol_RBDB(hid_t fapl_id);

void* H5VL_RetroBDB_fapl_copy(void* vol_info);

/* Atrribute callbacks */
static void *H5VL_RetroBDB_attr_create(void *obj, H5VL_loc_params_t loc_params, const char *attr_name, hid_t acpl_id, hid_t aapl_id, hid_t dxpl_id, void **req);
static void *H5VL_RetroBDB_attr_open(void *obj, H5VL_loc_params_t loc_params, const char *attr_name, hid_t aapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_attr_read(void *attr, hid_t dtype_id, void *buf, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_attr_write(void *attr, hid_t dtype_id, const void *buf, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_attr_get(void *obj, H5VL_attr_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_attr_specific(void *obj, H5VL_loc_params_t loc_params, H5VL_attr_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_attr_close(void *attr, hid_t dxpl_id, void **req);

/* Datatype callbacks */
static void *H5VL_RetroBDB_datatype_commit(void *obj, H5VL_loc_params_t loc_params, const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id, hid_t dxpl_id, void **req);
static void *H5VL_RetroBDB_datatype_open(void *obj, H5VL_loc_params_t loc_params, const char *name, hid_t tapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_datatype_get(void *dt, H5VL_datatype_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_datatype_close(void *dt, hid_t dxpl_id, void **req);

/* Dataset callbacks */
static void *H5VL_RetroBDB_dataset_create(void *obj, H5VL_loc_params_t loc_params, const char *name, hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req);
static void *H5VL_RetroBDB_dataset_open(void *obj, H5VL_loc_params_t loc_params, const char *name, hid_t dapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_dataset_read(void *dset, hid_t mem_type_id, hid_t mem_space_id,
                                       hid_t file_space_id, hid_t plist_id, void *buf, void **req);
static herr_t H5VL_RetroBDB_dataset_write(void *dset, hid_t mem_type_id, hid_t mem_space_id,
                                        hid_t file_space_id, hid_t plist_id, const void *buf, void **req);
static herr_t H5VL_RetroBDB_dataset_get(void *dset, H5VL_dataset_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_dataset_specific(void *dset, H5VL_dataset_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_dataset_close(void *dset, hid_t dxpl_id, void **req);

/* File callbacks */
static void *H5VL_RetroBDB_file_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id, void **req);
static void *H5VL_RetroBDB_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_file_get(void *file, H5VL_file_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_file_specific(void *file, H5VL_file_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_file_optional(void *file, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_file_close(void *file, hid_t dxpl_id, void **req);

/* Group callbacks */
static void *H5VL_RetroBDB_group_create(void *obj, H5VL_loc_params_t loc_params, const char *name, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req);
static void *H5VL_RetroBDB_group_open(void *obj, H5VL_loc_params_t loc_params, const char *name, hid_t gapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_group_get(void *obj, H5VL_group_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_group_close(void *grp, hid_t dxpl_id, void **req);

/* Link callbacks */
static herr_t H5VL_RetroBDB_link_create(H5VL_link_create_type_t create_type, void *obj, 
                                      H5VL_loc_params_t loc_params, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_link_copy(void *src_obj, H5VL_loc_params_t loc_params1,
                                    void *dst_obj, H5VL_loc_params_t loc_params2,
                                    hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_link_move(void *src_obj, H5VL_loc_params_t loc_params1,
                                    void *dst_obj, H5VL_loc_params_t loc_params2,
                                    hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_link_get(void *obj, H5VL_loc_params_t loc_params, H5VL_link_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_link_specific(void *obj, H5VL_loc_params_t loc_params, H5VL_link_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);

/* Object callbacks */
static void *H5VL_RetroBDB_object_open(void *obj, H5VL_loc_params_t loc_params, H5I_type_t *opened_type, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_object_copy(void *src_obj, H5VL_loc_params_t loc_params1, const char *src_name, 
                                      void *dst_obj, H5VL_loc_params_t loc_params2, const char *dst_name, 
                                      hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_RetroBDB_object_get(void *obj, H5VL_loc_params_t loc_params, H5VL_object_get_t get_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_object_specific(void *obj, H5VL_loc_params_t loc_params, H5VL_object_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments);
static herr_t H5VL_RetroBDB_object_optional(void *obj, hid_t dxpl_id, void **req, va_list arguments);


static H5VL_class_t H5VL_RetroBDB_g = {
    HDF5_VOL_RETROBDB_VERSION_1,                  /* Version number */
    H5_VOL_RETROBDB,                              /* Plugin value */
    "RetroBDB",					/* Plugin name */
    H5VL_RetroBDB_initialize,                   /* initialize */
    H5VL_RetroBDB_terminate,                    /* terminate */
    0,                                          /* fapl_size */
    H5VL_RetroBDB_fapl_copy,                    /* fapl_copy */
    NULL,                                       /* fapl_free */
    {                                           /* attribute_cls */
        H5VL_RetroBDB_attr_create,                /* create */
        H5VL_RetroBDB_attr_open,                  /* open */
        H5VL_RetroBDB_attr_read,                  /* read */
        H5VL_RetroBDB_attr_write,                 /* write */
        H5VL_RetroBDB_attr_get,                   /* get */
        H5VL_RetroBDB_attr_specific,              /* specific */
        NULL,                                   /* optional */
        H5VL_RetroBDB_attr_close                  /* close */
    },
    {                                           /* dataset_cls */
        H5VL_RetroBDB_dataset_create,             /* create */
        H5VL_RetroBDB_dataset_open,               /* open */
        H5VL_RetroBDB_dataset_read,               /* read */
        H5VL_RetroBDB_dataset_write,              /* write */
        H5VL_RetroBDB_dataset_get,                /* get */
        H5VL_RetroBDB_dataset_specific,           /* specific */
        NULL,                                   /* optional */
        H5VL_RetroBDB_dataset_close               /* close */
    },
    {                                           /* datatype_cls */
        H5VL_RetroBDB_datatype_commit,            /* commit */
        H5VL_RetroBDB_datatype_open,              /* open */
        H5VL_RetroBDB_datatype_get,               /* get */
        NULL,                                   /* specific */
        NULL,                                   /* optional */
        H5VL_RetroBDB_datatype_close              /* close */
    },
    {                                           /* file_cls */
        H5VL_RetroBDB_file_create,                /* create */
        H5VL_RetroBDB_file_open,                  /* open */
        H5VL_RetroBDB_file_get,                   /* get */
        H5VL_RetroBDB_file_specific,              /* specific */
        H5VL_RetroBDB_file_optional,              /* optional */
        H5VL_RetroBDB_file_close                  /* close */
    },
    {                                           /* group_cls */
        H5VL_RetroBDB_group_create,               /* create */
        H5VL_RetroBDB_group_open,                 /* open */
        H5VL_RetroBDB_group_get,                  /* get */
        NULL,                                   /* specific */
        NULL,                                   /* optional */
        H5VL_RetroBDB_group_close                 /* close */
    },
    {                                           /* link_cls */
        H5VL_RetroBDB_link_create,                /* create */
        H5VL_RetroBDB_link_copy,                  /* copy */
        H5VL_RetroBDB_link_move,                  /* move */
        H5VL_RetroBDB_link_get,                   /* get */
        H5VL_RetroBDB_link_specific,              /* specific */
        NULL                                    /* optional */
    },
    {                                           /* object_cls */
        H5VL_RetroBDB_object_open,                /* open */
        H5VL_RetroBDB_object_copy,                /* copy */
        H5VL_RetroBDB_object_get,                 /* get */
        H5VL_RetroBDB_object_specific,            /* specific */
        H5VL_RetroBDB_object_optional             /* optional */
    },
    {                                           /* async_cls */
        NULL,                                   /* cancel */
        NULL,                                   /* test */
        NULL                                    /* wait */
    },
    NULL
};

/*-------------------------------------------------------------------------
 * Function:    H5VL_set_vol_RBDB
 * Purpose: Sets the specified file access property list to use the Retro BDB VOL plugin,
            with the specified DB environment path (dbenv_home) and file name for DB tables (db_file)
 * Return:  Success:    0
 *          Failure:    non-zero value indicating failure
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
herr_t H5VL_set_vol_RBDB(hid_t fapl_id, const char* dbenv_home, const char* db_file){
    H5RBDB_dbenv_t dbenv_info;
    H5P_genplist_t* plist;
    herr_t ret_value;
    DB_ENV* dbenv;
    hid_t vol_id;
    
    if(fapl_id == H5P_DEFAULT){
        printf("Can't set values in default property list!\n");
        goto done;
    }
    if(NULL == (plist = H5P_object_verify(fapl_id, H5P_FILE_ACCESS))) {
        printf("not a file access property list!\n");
        goto done;
    }
    
    printf("Before H5VL_RETROBDB_g: %ld\n", H5VL_RETROBDB_g);

    if ((vol_id = H5VL_RetroBDB_init()) < 0){
        printf("Error initializing RetroBDB VOL plugin!\n");
        goto done;
    }
    
    printf("After H5VL_RETROBDB_g: %ld\n", H5VL_RETROBDB_g);

    H5RBDB_environment_open(&dbenv, dbenv_home);
    memset(&dbenv_info, 0, sizeof(H5RBDB_dbenv_t));
    dbenv_info.dbenv_home = strdup(dbenv_home);
    dbenv_info.db_file = strdup(db_file);
    dbenv_info.dbenv = dbenv;
    ret_value = H5P_set_vol(plist, H5VL_RETROBDB_g, &dbenv_info);

    done:
        return ret_value;
}

/*-------------------------------------------------------------------------
 * Function:    H5VL_terminate_vol_RBDB
 * Purpose: Terminates access to the specified file access property list corresponding to the Retro BDB VOL plugin
 * Return:  Success:    0
 *          Failure:    non-zero value indicating failure
 * Programmer:  Dimokritos Stamatakis
 *              July, 2016
 *-------------------------------------------------------------------------*/
herr_t H5VL_terminate_vol_RBDB(hid_t fapl_id){
	H5RBDB_dbenv_t* dbenv_info;
	int ret;
	printf("Terminating VOL\n");
	
	if((dbenv_info = (H5RBDB_dbenv_t*) H5Pget_vol_info(fapl_id)) == NULL){
		printf("Error getting DB environment struct!\n");
		return -1;
	}
	if (dbenv_info->open_files > 0){
		printf("Error! %d files are still open!\n", dbenv_info->open_files);
		return -2;
	}
	if (dbenv_info->active_txn){
		printf("Error! There is an active transaction!\n");
		return -3;
	}
	try(dbenv_info->dbenv->close(dbenv_info->dbenv, 0));
	//free(dbenv_info);
	return 0;
}


/*-------------------------------------------------------------------------
 * Function:    H5VL_RetroBDB_fapl_copy
 * Purpose: Makes a copy of the specified vol_info struct (after type casting it to H5RBDB_dbenv_t) and returns that copy
 * Return:  Success:    pointer to the copy of the specified vol_info struct
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
void* H5VL_RetroBDB_fapl_copy(void* vol_info){
	H5RBDB_dbenv_t* dbenv_info, *dbenv_info_copy;
	
	dbenv_info = (H5RBDB_dbenv_t*) vol_info;
	dbenv_info_copy = (H5RBDB_dbenv_t*) malloc(sizeof(H5RBDB_dbenv_t));
	memcpy(dbenv_info_copy, dbenv_info, sizeof(H5RBDB_dbenv_t));
	return (void*)dbenv_info_copy;
}


/*--------------------------------------------------------------------------
NAME
   H5VL_RetroBDB_init_interface -- Initialize interface-specific information
USAGE
    herr_t H5VL_RetroBDB_init_interface()

RETURNS
    Non-negative on success/Negative on failure
DESCRIPTION
    Initializes any interface-specific data or routines.  (Just calls
    H5VL_RetroBDB_init currently).

--------------------------------------------------------------------------*/
static herr_t
H5VL_RetroBDB_init_interface(void)
{
    FUNC_ENTER_NOAPI_NOINIT_NOERR
    H5VL_RetroBDB_init();
    FUNC_LEAVE_NOAPI(SUCCEED)
} /* H5VL_RetroBDB_init_interface() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_init
 *
 * Purpose:	Initialize this vol plugin by registering the driver with the
 *		library.
 *
 * Return:	Success:	The ID for the RetroBDB plugin.
 *		Failure:	Negative.
 *
 * Programmer:	Dimokritos Stamatakis
 *              October, 2015
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL_RetroBDB_init(void)
{
    hid_t ret_value = FAIL;            /* Return value */

    //printf(" --------- H5VL_RetroBDB_init!\n");
    FUNC_ENTER_NOAPI(FAIL)
    #if LOG_LEVEL > 0
        H5DIMLOG_INFO(LOG_NAME_DEFAULT, "start");
        if (H5VL_RetroBDB_g.file_cls.create == NULL){
            H5DIMLOG_INFO(LOG_NAME_DEFAULT, "File create function pointer is NULL!");
        }
    #endif
    /* Register the RetroBDB VOL, if it isn't already */
    if(NULL == H5I_object_verify(H5VL_RETROBDB_g, H5I_VOL)) {
        #if LOG_LEVEL > 0
            if (H5VL_RetroBDB_g.file_cls.create == NULL){
                H5DIMLOG_INFO(LOG_NAME_DEFAULT, "After H5VL_object_verify == NULL: File create function pointer is NULL!");
            }
        #endif
        if((H5VL_RETROBDB_g = H5VL_register((const H5VL_class_t *)&H5VL_RetroBDB_g, 
                                          sizeof(H5VL_class_t), TRUE)) < 0)
            HGOTO_ERROR(H5E_ATOM, H5E_CANTINSERT, FAIL, "can't create ID for RetroBDB plugin")
    }
    #if LOG_LEVEL > 0
        if (H5VL_RetroBDB_g.file_cls.create == NULL){
            H5DIMLOG_INFO(LOG_NAME_DEFAULT, "After H5VL_register: File create function pointer is NULL!");
        }
    #endif
    /* Set return value */
    ret_value = H5VL_RETROBDB_g;

done:
    #if LOG_LEVEL > 0
        H5DIMLOG_INFO(LOG_NAME_DEFAULT, "end");
    #endif
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_init() */



static herr_t
H5VL_RetroBDB_initialize(void){
	return -1;
}

static herr_t H5VL_RetroBDB_terminate(hid_t vpl_id){
	printf("-------------Terminateeee!---------------\n");
	return -1;
}



/*---------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_get_file
 *
 * Purpose:	utility routine to get file object
 *
 * Returns:     Non-negative on success or negative on failure
 *
 * Programmer:  Mohamad Chaarawi
 *              June, 2012
 *
 *---------------------------------------------------------------------------
 */
static H5F_t *
H5VL_RetroBDB_get_file(void *obj, H5I_type_t type)
{
    return NULL;
}/* H5VL_RetroBDB_get_file */


/*---------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_register
 *
 * Purpose:	utility routine to register an ID with the RetroBDB VOL plugin 
 *              as an auxilary object
 *
 * Returns:     Non-negative on success or negative on failure
 *
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *
 *---------------------------------------------------------------------------
 */
hid_t
H5VL_RetroBDB_register(H5I_type_t type, void *obj, hbool_t app_ref)
{
    hid_t    ret_value = FAIL;

    FUNC_ENTER_NOAPI_NOINIT
    
    #if LOG_LEVEL > 0
        H5DIMLOG_INFO(LOG_NAME_DEFAULT, "start");
    #endif
	
    //printf("---- H5VL_RetroBDB_register!\n");
    HDassert(obj);

    /* make sure this is from the internal RetroBDB plugin not from the API */
    if(type == H5I_DATATYPE) {
        HDassert(((H5T_t *)obj)->vol_obj == NULL);
    }

    /* Get an atom for the object */
    if ((ret_value = H5VL_object_register(obj, type, H5VL_RETROBDB_g, app_ref)) < 0)
        HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize object handle")

#if 0
    else {
        H5VL_t *vol_info = NULL;        /* VOL info struct */
        H5VL_object_t *new_obj = NULL;

        /* setup VOL object */
        if(NULL == (new_obj = H5FL_CALLOC(H5VL_object_t)))
            HGOTO_ERROR(H5E_VOL, H5E_NOSPACE, NULL, "can't allocate top object structure")
        new_obj->vol_obj = obj;

        /* setup VOL info struct */
        if(NULL == (vol_info = H5FL_CALLOC(H5VL_t)))
            HGOTO_ERROR(H5E_FILE, H5E_NOSPACE, NULL, "can't allocate VL info struct")
        vol_info->vol_cls = &H5VL_RetroBDB_g;
        vol_info->nrefs = 1;
        vol_info->vol_id = H5VL_RETROBDB_g;
        if(H5I_inc_ref(vol_info->id, FALSE) < 0)
            HGOTO_ERROR(H5E_ATOM, H5E_CANTINC, NULL, "unable to increment ref count on VOL plugin")
        new_obj->vol_info = vol_info;

        if((ret_value = H5I_register(obj_type, new_obj, app_ref)) < 0)
            HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to register object ID")
    }
#endif

done:
    #if LOG_LEVEL > 0
        H5DIMLOG_INFO(LOG_NAME_DEFAULT, "end");
    #endif
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5VL_RetroBDB_register */


/*---------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_unregister
 *
 * Purpose:	utility routine to decrement ref count on RetroBDB VOL plugin
 *
 * Returns:     Non-negative on success or negative on failure
 *
 * Programmer:  Mohamad Chaarawi
 *              August, 2014
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VL_RetroBDB_unregister(hid_t obj_id)
{
	return -1;
} /* H5VL_RetroBDB_unregister */


/*-------------------------------------------------------------------------
 * Function:	H5Pset_fapl_RetroBDB
 *
 * Purpose:	Modify the file access property list to use the H5VL_RETROBDB
 *		plugin defined in this source file.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Pset_fapl_RetroBDB(hid_t fapl_id)
{
    return -1;
} /* end H5Pset_fapl_RetroBDB() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_attr_create
 *
 * Purpose:	Creates an attribute on an object.
 *
 * Return:	Success:	attr id. 
 *		Failure:	NULL
 *
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_attr_create(void *obj, H5VL_loc_params_t loc_params, const char *attr_name, hid_t acpl_id, 
                        hid_t H5_ATTR_UNUSED aapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5G_loc_t       loc;                /* Object location */
    H5G_loc_t       obj_loc;            /* Location used to open group */
    hbool_t         loc_found = FALSE;  
    H5P_genplist_t  *plist;             /* Property list pointer */
    hid_t           type_id, space_id;
    H5T_t       *type, *dt;         /* Datatype to use for attribute */
    H5S_t       *space;             /* Dataspace to use for attribute */
    H5RBDB_attr_t* attr = NULL;
    void            *ret_value = NULL;

    FUNC_ENTER_NOAPI_NOINIT

    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(acpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, NULL, "can't find object for ID")

    /* get creation properties */
    if(H5P_get(plist, H5VL_PROP_ATTR_TYPE_ID, &type_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, NULL, "can't get property value for datatype id")
    if(H5P_get(plist, H5VL_PROP_ATTR_SPACE_ID, &space_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, NULL, "can't get property value for space id")

    if(H5G_loc_real(obj, loc_params.obj_type, &loc) < 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a file or file object")
    // TODO: check if we have write permission in the file!
    //HGOTO_ERROR(H5E_ARGS, H5E_WRITEERROR, NULL, "no write intent on file")
	
    //printf("Creating attribute, type id: %ld\n", type_id);

    if(NULL == (dt = (H5T_t *)H5I_object_verify(type_id, H5I_DATATYPE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a datatype")
    /* If this is a named datatype, get the plugin pointer to the datatype */
    type = H5T_get_actual_type(dt);

    if(NULL == (space = (H5S_t *)H5I_object_verify(space_id, H5I_DATASPACE)))
    HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a data space")

    if(loc_params.obj_type == H5I_GROUP){
        //printf("Creating attribute for group %s\n", ((H5RBDB_group_t*)obj)->name );

    } else if (loc_params.obj_type == H5I_DATASET){
        //printf("Creating attribute for dataset!\n");
    }

    if(loc_params.type == H5VL_OBJECT_BY_SELF) { /* H5Acreate */
        if(NULL == (attr = H5RBDB_attr_create(obj, loc_params.obj_type, attr_name, type, space, acpl_id, dxpl_id)))
            HGOTO_ERROR(H5E_ATTR, H5E_CANTINIT, NULL, "unable to create attribute")
        /* Go do the real work for attaching the attribute to the dataset */
        //if(NULL == (attr = H5A_create(&loc, attr_name, type, space, acpl_id, dxpl_id)))
          //  HGOTO_ERROR(H5E_ATTR, H5E_CANTINIT, NULL, "unable to create attribute")
    }
    else if(loc_params.type == H5VL_OBJECT_BY_NAME) { /* H5Acreate_by_name */
        H5G_name_t          obj_path;               /* Opened object group hier. path */
        H5O_loc_t           obj_oloc;               /* Opened object object location */

        /* Set up opened group location to fill in */
        obj_loc.oloc = &obj_oloc;
        obj_loc.path = &obj_path;
        //H5G_loc_reset(&obj_loc);

        // Find the object's location
        //if(H5G_loc_find(&loc, loc_params.loc_data.loc_by_name.name, &obj_loc/*out*/, 
        //                loc_params.loc_data.loc_by_name.lapl_id, H5AC_ind_dxpl_id) < 0)
        //    HGOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL, "object not found")
        //loc_found = TRUE;

        // Go do the real work for attaching the attribute to the dataset */
        //if(NULL == (attr = H5A_create(&obj_loc, attr_name, type, space, acpl_id, dxpl_id)))
          //  HGOTO_ERROR(H5E_ATTR, H5E_CANTINIT, NULL, "unable to create attribute")
    }
    else {
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "unknown attribute create parameters")
    }
    ret_value = (void *)attr;

done:
    /* Release resources */
    if(loc_found && H5G_loc_free(&obj_loc) < 0)
        HDONE_ERROR(H5E_ATTR, H5E_CANTRELEASE, NULL, "can't free location") 
   FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_attr_create() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_attr_open
 *
 * Purpose:	Opens a attr inside a h5 file.
 *
 * Return:	Success:	attr id. 
 *		Failure:	NULL
 *
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_attr_open(void *obj, H5VL_loc_params_t loc_params, const char *attr_name, 
                      hid_t H5_ATTR_UNUSED aapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5G_loc_t    loc;             /* Object location */
    H5RBDB_attr_t *attr = NULL;    /* Attribute opened */
    void         *ret_value;

    FUNC_ENTER_NOAPI_NOINIT
    /* check arguments */
    if(H5G_loc_real(obj, loc_params.obj_type, &loc) < 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a file or file object")

    if(loc_params.type == H5VL_OBJECT_BY_SELF) { /* H5Aopen */
        /* Read in attribute from object header */
        if(NULL == (attr = H5RBDB_attr_open(obj, loc_params.obj_type, attr_name, dxpl_id)))
            HGOTO_ERROR(H5E_ATTR, H5E_CANTOPENOBJ, NULL, "unable to open attribute: '%s'", attr_name)
    }
    else if(loc_params.type == H5VL_OBJECT_BY_NAME) { /* H5Aopen_by_name */
        printf("Opening by name not yet supported!\n");
        /* Open the attribute on the object header */
        //if(NULL == (attr = H5A_open_by_name(&loc, loc_params.loc_data.loc_by_name.name, attr_name, 
          //                                  loc_params.loc_data.loc_by_name.lapl_id, dxpl_id)))
            //HGOTO_ERROR(H5E_ATTR, H5E_CANTOPENOBJ, NULL, "can't open attribute")
    }
    else if(loc_params.type == H5VL_OBJECT_BY_IDX) { /* H5Aopen_by_idx */
        printf("Opening by ID not yet supported!\n");
        /* Open the attribute in the object header */
        /*if(NULL == (attr = H5A_open_by_idx(&loc, loc_params.loc_data.loc_by_idx.name, 
                                           loc_params.loc_data.loc_by_idx.idx_type, 
                                           loc_params.loc_data.loc_by_idx.order, 
                                           loc_params.loc_data.loc_by_idx.n, 
                                           loc_params.loc_data.loc_by_idx.lapl_id, dxpl_id)))
            HGOTO_ERROR(H5E_ATTR, H5E_CANTOPENOBJ, NULL, "unable to open attribute")*/
    }
    else {
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "unknown attribute open parameters")
    }

    ret_value = (void *)attr;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_attr_open() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_attr_read
 *
 * Purpose:	Reads in data from attribute.
 *
 *              Non-negative on success/Negative on failure
 *
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_attr_read(void *attr, hid_t dtype_id, void *buf, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5T_t *mem_type;            /* Memory datatype */
    herr_t ret_value;           /* Return value */

    FUNC_ENTER_NOAPI_NOINIT
    if(NULL == (mem_type = (H5T_t *)H5I_object_verify(dtype_id, H5I_DATATYPE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a datatype")
    /* Go write the actual data to the attribute */
    if((ret_value = H5RBDB_attr_read((H5RBDB_attr_t*)attr, mem_type, buf, dxpl_id)) < 0)
        HGOTO_ERROR(H5E_ATTR, H5E_READERROR, FAIL, "unable to read attribute")
done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_attr_read() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_attr_write
 *
 * Purpose:	Writes out data to attribute.
 *
 *              Non-negative on success/Negative on failure
 *
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_attr_write(void *attr, hid_t dtype_id, const void *buf, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5T_t *mem_type;            /* Memory datatype */
    herr_t ret_value;           /* Return value */

    FUNC_ENTER_NOAPI_NOINIT

    if(NULL == (mem_type = (H5T_t *)H5I_object_verify(dtype_id, H5I_DATATYPE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a datatype")
    /* Go write the actual data to the attribute */
    if((ret_value = H5RBDB_attr_write((H5RBDB_attr_t *)attr, mem_type, buf, dxpl_id)) < 0)
        HGOTO_ERROR(H5E_ATTR, H5E_WRITEERROR, FAIL, "unable to write attribute")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_attr_write() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_attr_get
 *
 * Purpose:	Gets certain information about an attribute
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_attr_get(void *obj, H5VL_attr_get_t get_type, hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_NOAPI_NOINIT

    switch (get_type) {
        /* H5Aget_space */
        case H5VL_ATTR_GET_SPACE:
            {
                HGOTO_ERROR(H5E_ARGS, H5E_CANTGET, FAIL, "Attribute get space is not yet supported!")
                break;
            }
        /* H5Aget_type */
        case H5VL_ATTR_GET_TYPE:
            {
		hid_t   *ret_id = va_arg (arguments, hid_t *);
		H5RBDB_attr_t   *attr = (H5RBDB_attr_t *)obj;
                if((*ret_id  = H5RBDB_attr_get_type(attr)) < 0)
                    HGOTO_ERROR(H5E_ARGS, H5E_CANTGET, FAIL, "can't get datatype ID of attribute")
		//printf("Type of attribute datatype: %d\n", attr->type->shared->type);
		//printf("Register returns %ld\n", *ret_id);
                break;
            }
        /* H5Aget_create_plist */
        case H5VL_ATTR_GET_ACPL:
            {
                HGOTO_ERROR(H5E_ARGS, H5E_CANTGET, FAIL, "Attribute get create property list is not yet supported!")
                break;
            }
        /* H5Aget_name */
        case H5VL_ATTR_GET_NAME:
            {
                HGOTO_ERROR(H5E_ARGS, H5E_CANTGET, FAIL, "Attribute get name is not yet supported!")
                break;
            }
        /* H5Aget_info */
        case H5VL_ATTR_GET_INFO:
            {
                HGOTO_ERROR(H5E_ARGS, H5E_CANTGET, FAIL, "Attribute get info is not yet supported!")
                break;
            }
        case H5VL_ATTR_GET_STORAGE_SIZE:
            {
                hsize_t *ret = va_arg (arguments, hsize_t *);
                H5RBDB_attr_t   *attr = (H5RBDB_attr_t *)obj;

                /* Set return value */
                *ret = attr->data_size;
                break;
            }
        default:
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get this type of information from attr")
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_attr_get() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_attr_specific
 *
 * Purpose:	Specific operations for attributes
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Mohamad Chaarawi
 *              August, 2014
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_attr_specific(void *obj, H5VL_loc_params_t loc_params, H5VL_attr_specific_t specific_type, 
                          hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    herr_t      ret_value = SUCCEED;    /* Return value */
    
    H5G_loc_t       obj_loc;            /* Location used to open group */
    hid_t obj_id;

    FUNC_ENTER_NOAPI_NOINIT

    switch (specific_type) {
        case H5VL_ATTR_DELETE:
            {
                HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "deleting attribute is not yet supported!")
                break;
            }
        case H5VL_ATTR_EXISTS:
            {
                const char *attr_name = va_arg (arguments, const char *);
                htri_t  *ret = va_arg (arguments, htri_t *);
                H5G_loc_t loc;

                /* check arguments */
                if(H5G_loc_real(obj, loc_params.obj_type, &loc) < 0)
                    HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object")

                if(loc_params.type == H5VL_OBJECT_BY_SELF) { /* H5Aexists */
                    /* Check if the attribute exists */
                    if((*ret = H5RBDB_attr_exists(obj, loc_params.obj_type, attr_name, dxpl_id)) < 0)
                        HGOTO_ERROR(H5E_ATTR, H5E_CANTGET, FAIL, "unable to determine if attribute exists")
                }
                else if(loc_params.type == H5VL_OBJECT_BY_NAME) { /* H5Aopen_by_name */
                    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "locating attribute by name is not yet supported!")
                    /* Check if the attribute exists */
                    //if((*ret = H5A_exists_by_name(loc, loc_params.loc_data.loc_by_name.name, attr_name, 
                      //                            loc_params.loc_data.loc_by_name.lapl_id, dxpl_id)) < 0)
                        //HGOTO_ERROR(H5E_ATTR, H5E_CANTGET, FAIL, "unable to determine if attribute exists")
                }
                else {
                    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "unknown parameters")
                }
                break;
            }
        case H5VL_ATTR_ITER:
            {
		    
                H5_index_t idx_type = va_arg (arguments, H5_index_t);
                H5_iter_order_t order = va_arg (arguments, H5_iter_order_t);
                hsize_t *idx = va_arg (arguments, hsize_t *);
                H5A_operator2_t op = va_arg (arguments, H5A_operator2_t);
                void *op_data = va_arg (arguments, void *);
		
		if ((obj_id = H5VL_object_register(obj, loc_params.obj_type, H5VL_RETROBDB_g, 0)) < 0)
			HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize object handle")
		
                if((ret_value = H5RBDB_attr_iterate(obj, obj_id, loc_params.obj_type, idx_type, order, idx, op, op_data)) < 0)
                    HGOTO_ERROR(H5E_ATTR, H5E_BADITER, FAIL, "error iterating over attributes")
		
                break;
            }
        /* H5Arename/rename_by_name */
        case H5VL_ATTR_RENAME:
            {
                HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "renaming attribute is not yet supported!")
                break;
            }
        default:
            HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "invalid specific operation")
    }
done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_attr_specific() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_attr_close
 *
 * Purpose:	Closes an attribute.
 *
 * Return:	Success:	0
 *		Failure:	-1, attr not closed.
 *
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_attr_close(void *attr, hid_t H5_ATTR_UNUSED dxpl_id, void H5_ATTR_UNUSED **req)
{
    return H5RBDB_attr_close((H5RBDB_attr_t*) attr);
} /* end H5VL_RetroBDB_attr_close() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_datatype_commit
 *
 * Purpose:	Commits a datatype inside a native h5 file.
 *
 * Return:	Success:	datatype id. 
 *		Failure:	NULL
 *
 * Programmer:  Mohamad Chaarawi
 *              March, 2012
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_datatype_commit(void *obj, H5VL_loc_params_t loc_params, const char *name, hid_t type_id, 
                            hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    //printf("-------- datatype commit\n");
    return NULL;
} /* end H5VL_RetroBDB_datatype_commit() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_datatype_open
 *
 * Purpose:	Opens a named datatype inside a native h5 file.
 *
 * Return:	Success:	datatype id. 
 *		Failure:	NULL
 *
 * Programmer:  Mohamad Chaarawi
 *              March, 2012
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_datatype_open(void *obj, H5VL_loc_params_t loc_params, const char *name, 
                          hid_t tapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    //printf("-------- datatype open\n");
    return NULL;
} /* end H5VL_RetroBDB_datatype_open() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_datatype_get
 *
 * Purpose:	Gets certain information about a datatype
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Mohamad Chaarawi
 *              June, 2013
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_datatype_get(void *obj, H5VL_datatype_get_t get_type, 
                         hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    //printf("-------- datatype get\n");
    return -1;
} /* end H5VL_RetroBDB_datatype_get() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_datatype_close
 *
 * Purpose:	Closes an datatype.
 *
 * Return:	Success:	0
 *		Failure:	-1, datatype not closed.
 *
 * Programmer:  Mohamad Chaarawi
 *              March, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_datatype_close(void *dt, hid_t H5_ATTR_UNUSED dxpl_id, void H5_ATTR_UNUSED **req)
{
    //printf("-------- datatype close\n");
    return -1;
} /* end H5VL_RetroBDB_datatype_close() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_dataset_create
 *
 * Purpose:	Creates a dataset inside a native h5 file.
 *
 * Return:	Success:	dataset id. 
 *		Failure:	NULL
 *
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_dataset_create(void *obj, H5VL_loc_params_t loc_params, const char *name, hid_t dcpl_id, 
                           hid_t dapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5P_genplist_t *plist;              /* Property list pointer */
    H5G_loc_t      loc;                 /* Object location to insert dataset into */
    hid_t          type_id, space_id, lcpl_id;
    H5RBDB_file_t* file;
    H5RBDB_dataset_t* dataset;
    void* ret_value;
    
    FUNC_ENTER_NOAPI_NOINIT

    file = (H5RBDB_file_t*) obj;

    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(dcpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, NULL, "can't find object for ID")

    /* get creation properties */
    if(H5P_get(plist, H5VL_PROP_DSET_TYPE_ID, &type_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, NULL, "can't get property value for datatype id")
    if(H5P_get(plist, H5VL_PROP_DSET_SPACE_ID, &space_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, NULL, "can't get property value for space id")
    if(H5P_get(plist, H5VL_PROP_DSET_LCPL_ID, &lcpl_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, NULL, "can't get property value for lcpl id")

    /* Check arguments */
    //if(H5G_loc_real(obj, loc_params.obj_type, &loc) < 0)
    //    HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a file or file object")
    if(H5I_DATATYPE != H5I_get_type(type_id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a datatype ID")


    if (NULL == (dataset = H5RBDB_dataset_create(file, name, space_id, type_id, dcpl_id, dapl_id, dxpl_id)))
        HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "unable to create dataset")

    ret_value = (void*) dataset;
done:
    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5VL_RetroBDB_dataset_create() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_dataset_open
 *
 * Purpose:	Opens a dataset inside a Berkeley DB database.
 *
 * Return:	Success:	dataset id. 
 *		Failure:	NULL
 *
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_dataset_open(void *obj, H5VL_loc_params_t loc_params, const char *name, 
                         hid_t dapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5RBDB_file_t* file;
    H5RBDB_dataset_t* dataset;

    file = (H5RBDB_file_t*)obj;
    dataset = H5RBDB_dataset_open(file, name, dapl_id, dxpl_id);

    return (void*)dataset;
} /* end H5VL_RetroBDB_dataset_open() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_dataset_read
 *
 * Purpose:	Reads raw data from a dataset into a buffer.
 *
 * Return:	Success:	0
 *		Failure:	-1, dataset not readd.
 *
 * Programmer:  Mohamad Chaarawi
 *              March, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_dataset_read(void *obj, hid_t mem_type_id, hid_t mem_space_id,
                         hid_t file_space_id, hid_t plist_id, void *buf, void H5_ATTR_UNUSED **req)
{
    H5RBDB_dataset_t* dataset;

    dataset = (H5RBDB_dataset_t*)obj;

    return H5RBDB_dataset_read(dataset, mem_type_id, mem_space_id, file_space_id, plist_id, buf);
} /* end H5VL_RetroBDB_dataset_read() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_dataset_write
 *
 * Purpose:	Writes raw data from a buffer into a dataset.
 *
 * Return:	Success:	0
 *		Failure:	-1, dataset not writed.
 *
 * Programmer:  Mohamad Chaarawi
 *              March, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_dataset_write(void *obj, hid_t mem_type_id, hid_t mem_space_id,
                          hid_t file_space_id, hid_t dxpl_id, const void *buf, void H5_ATTR_UNUSED **req)
{
    H5RBDB_dataset_t* dataset;

    dataset = (H5RBDB_dataset_t*)obj;

    return H5RBDB_dataset_write(dataset, mem_type_id, mem_space_id, file_space_id, dxpl_id, buf);
} /* end H5VL_RetroBDB_dataset_write() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_dataset_get
 *
 * Purpose:	Gets certain information about a dataset
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Mohamad Chaarawi
 *              March, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_dataset_get(void *obj, H5VL_dataset_get_t get_type, hid_t dxpl_id, 
                        void H5_ATTR_UNUSED **req, va_list arguments)
{
    return -1;
} /* end H5VL_RetroBDB_dataset_get() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_dataset_specific
 *
 * Purpose:	Specific operations for datasets
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Mohamad Chaarawi
 *              August, 2014
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_dataset_specific(void *obj, H5VL_dataset_specific_t specific_type, 
                             hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    return -1;
} /* end H5VL_RetroBDB_dataset_specific() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_dataset_close
 *
 * Purpose:	Closes a dataset.
 *
 * Return:	Success:	0
 *		Failure:	-1, dataset not closed.
 *
 * Programmer:  Mohamad Chaarawi
 *              March, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_dataset_close(void *dset, hid_t H5_ATTR_UNUSED dxpl_id, void H5_ATTR_UNUSED **req)
{
    return -1;
} /* end H5VL_RetroBDB_dataset_close() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_file_create
 *
 * Purpose:	Creates a file as a Berkeley DB database.
 *
 * Return:	Success:	the file id. 
 *		Failure:	NULL
 *
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_file_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, 
                        hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5RBDB_file_t* new_db = NULL;
    void  *ret_value = NULL;

    FUNC_ENTER_NOAPI_NOINIT
    
    #if LOG_LEVEL > 0
        H5DIMLOG_INFO(LOG_NAME_DEFAULT, "start");
        H5DIMLOG_INFO_FORMAT(LOG_NAME_DEFAULT, "flags: %u\t access list ID: %d", flags, fapl_id);
        H5DIMLOG_INFO(LOG_NAME_DEFAULT, "creating file in BDB!");
    #endif
        
    #if LOG_LEVEL > 0
        H5DIMLOG_INFO("file_create.log", "Retro BDB file_create!");
    #endif
    /*
     * Adjust bit flags by turning on the creation bit and making sure that
     * the EXCL or TRUNC bit is set.  All newly-created files are opened for
     * reading and writing.
     */
    if(0==(flags & (H5F_ACC_EXCL|H5F_ACC_TRUNC)))
	flags |= H5F_ACC_EXCL;	 /*default*/
    flags |= H5F_ACC_RDWR | H5F_ACC_CREAT;
    
    
    #if LOG_LEVEL > 0
        H5DIMLOG_INFO_FORMAT("retro_bdb_file_create.log", "flags: %u", flags);
    #endif
    

    /* Create the file */ 
    if(NULL == (new_db = H5RBDB_file_create(name, flags, fcpl_id, fapl_id, dxpl_id)))
        HGOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, NULL, "unable to create file")
        
    ret_value = (void *)new_db;

done:
    #if LOG_LEVEL > 0
        H5DIMLOG_INFO(LOG_NAME_DEFAULT, "end");
    #endif
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_file_create() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_file_open
 *
 * Purpose:	Opens a file as a Berkeley DB database.
 *
 * Return:	Success:	file id. 
 *		Failure:	NULL
 *
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    void * ret_value = NULL;
    H5RBDB_file_t* opened_file = NULL;
    
    FUNC_ENTER_NOAPI_NOINIT
    
    if (NULL == (opened_file = H5RBDB_file_open(name, flags, fapl_id, dxpl_id) ))
        HGOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, NULL, "unable to open file")
    ret_value = (void*) opened_file;
    
done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_file_open() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_file_get
 *
 * Purpose:	Gets certain data about a file
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_file_get(void *obj, H5VL_file_get_t get_type, hid_t H5_ATTR_UNUSED dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    H5F_t *f = NULL;  /* File struct */
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_NOAPI_NOINIT

    switch (get_type) {
        /* H5Fget_access_plist */
        case H5VL_FILE_GET_FAPL:
            {
                break;
            }
        /* H5Fget_create_plist */
        case H5VL_FILE_GET_FCPL:
            {
                break;
            }
        /* H5Fget_obj_count */
        case H5VL_FILE_GET_OBJ_COUNT:
            {
                break;
            }
        /* H5Fget_obj_ids */
        case H5VL_FILE_GET_OBJ_IDS:
            {
                break;
            }
        /* H5Fget_intent */
        case H5VL_FILE_GET_INTENT:
            {
                break;
            }
        /* H5Fget_name */
        case H5VL_FILE_GET_NAME:
            {
                H5I_type_t type = va_arg (arguments, H5I_type_t);
                size_t     size = va_arg (arguments, size_t);
                char      *name = va_arg (arguments, char *);
                ssize_t   *ret  = va_arg (arguments, ssize_t *);
                size_t     len;

                H5RBDB_file_t* file = (H5RBDB_file_t*) obj;

                len = HDstrlen(file->name);

                if(name) {
                    HDstrncpy(name, file->name, MIN(len + 1,size));
                    if(len >= size)
                        name[size-1]='\0';
                } /* end if */

                /* Set the return value for the API call */
                *ret = (ssize_t)len;
                break;
            }
        /* H5I_get_file_id */
        case H5VL_OBJECT_GET_FILE:
            {
                break;
            }
        default:
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get this type of information")
    } /* end switch */
done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_file_get() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_file_specific
 *
 * Purpose:	Perform an operation
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_file_specific(void *obj, H5VL_file_specific_t specific_type, hid_t dxpl_id, 
                          void H5_ATTR_UNUSED **req, va_list arguments)
{
   return -1; 
} /* end H5VL_RetroBDB_file_specific() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_file_optional
 *
 * Purpose:	Perform a plugin specific operation on a native file
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_file_optional(void *obj, hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    return -1;
} /* end H5VL_RetroBDB_file_optional() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_file_close
 *
 * Purpose:	Closes a file.
 *
 * Return:	Success:	0
 *		Failure:	-1, file not closed.
 *
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_file_close(void *file, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    herr_t ret_value = SUCCEED;
    //int ret;
    H5RBDB_file_t* f;
    
    FUNC_ENTER_NOAPI_NOINIT

    f = (H5RBDB_file_t*)file;
    if ( (H5RBDB_file_close(f, dxpl_id)) != 0)
        HGOTO_ERROR(H5E_FILE, H5E_CANTCLOSEFILE, NULL, "unable to close file")
        
done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_file_close() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_group_create
 *
 * Purpose:	Creates a group inside a native h5 file.
 *
 * Return:	Success:	group id. 
 *		Failure:	NULL
 *
 * Programmer:  Dimokritos Stamatakis
 *              October, 2015
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_group_create(void *obj, H5VL_loc_params_t loc_params, const char *name, hid_t gcpl_id, 
                         hid_t gapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5RBDB_file_t* file;
    H5G_loc_t      loc;                 /* Location to create group */
    H5RBDB_group_t* group;
    H5P_genplist_t* plist;
    hid_t lcpl_id;
    void* ret_value;
    
    FUNC_ENTER_NOAPI_NOINIT
    
    file = (H5RBDB_file_t*) obj;
    
    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(gcpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, NULL, "can't find object for ID")

    /* get creation properties */
    if(H5P_get(plist, H5VL_PROP_GRP_LCPL_ID, &lcpl_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, NULL, "can't get property value for lcpl id")
	


	
	
    if (NULL == (group = H5RBDB_group_create(file, loc_params, name, gcpl_id, gapl_id, dxpl_id)))
	   HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, NULL, "unable to create group")

    ret_value = (void*) group;    
done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_group_create() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_group_open
 *
 * Purpose:	Opens a group inside a native h5 file.
 *
 * Return:	Success:	group id. 
 *		Failure:	NULL
 *
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_group_open(void *obj, H5VL_loc_params_t loc_params, const char *name, 
                       hid_t gapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5RBDB_group_t  *grp = NULL;         /* New group opend */
    H5RBDB_file_t* file = NULL;		/* File containing group */
    void           *ret_value;

    FUNC_ENTER_NOAPI_NOINIT
        
    //printf("In RetroBDB_group_open! name : %s, location params obj type: %d\n", name, loc_params.obj_type);
    
    // TODO: maybe we have to set gapl_id and dxpl_id to the opened group!?
    /* Open the group */
    if((grp = H5RBDB_group_open(obj, loc_params.obj_type, name, gapl_id, dxpl_id)) == NULL)
        HGOTO_ERROR(H5E_SYM, H5E_CANTOPENOBJ, NULL, "unable to open group")

    ret_value = (void *)grp;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_group_open() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_group_get
 *
 * Purpose:	Gets certain data about a group
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Dimokritos Stamatakis
 *              December, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_group_get(void *obj, H5VL_group_get_t get_type, hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_NOAPI_NOINIT

    switch (get_type) {
        /* H5Gget_create_plist */
        case H5VL_GROUP_GET_GCPL:
            {
                hid_t *new_gcpl_id = va_arg (arguments, hid_t *);
                H5RBDB_group_t *grp = (H5RBDB_group_t *)obj;

                if((*new_gcpl_id = grp->gcpl_id) < 0)
                    HGOTO_ERROR(H5E_ARGS, H5E_CANTGET, FAIL, "can't get creation property list for group")
                break;
            }
        /* H5Gget_info */
        case H5VL_GROUP_GET_INFO: // not yet implemented
            {
                printf("In RetroBDB_group_get! Getting info. Not yet implemented!\n");
            }
        default:
            printf("In RetroBDB_group_get! Can't get this type of information from group.\n");
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get this type of information from group")
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_group_get() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_group_close
 *
 * Purpose:	Closes a group.
 *
 * Return:	Success:	0
 *		Failure:	-1, group not closed.
 *
 * Programmer:  Dimokritos Stamatakis
 *              January, 2016
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_group_close(void *grp, hid_t H5_ATTR_UNUSED dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5RBDB_group_t* group;
    group = (H5RBDB_group_t*)grp;
    H5RBDB_group_close(group);
    return 0;
} /* end H5VL_RetroBDB_group_close() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_link_create
 *
 * Purpose:	Creates an hard/soft/UD/external links.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_link_create(H5VL_link_create_type_t create_type, void *obj, H5VL_loc_params_t loc_params,
                        hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    H5P_genplist_t   *plist;                     /* Property list pointer */
    herr_t           ret_value = SUCCEED;        /* Return value */
    H5RBDB_group_t* group;

    FUNC_ENTER_NOAPI_NOINIT

    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(lcpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, FAIL, "can't find object for ID");

    if(loc_params.obj_type != H5I_GROUP){
        HGOTO_ERROR(H5E_LINK, H5E_CANTINIT, FAIL, "Creating links to objects other than groups is not yet supported in the Retro BDB VOL plugin!")
    }    

    switch (create_type) {
        case H5VL_LINK_CREATE_HARD:
            {
                printf("Creating hard links is not yet supported in the Retro BDB VOL plugin!\n");
                break;
            }
        case H5VL_LINK_CREATE_SOFT:
            {
                char        *target_name;
                H5G_loc_t   link_loc;               /* Group location for new link */
                group = (H5RBDB_group_t*) obj;

                
                if(H5P_get(plist, H5VL_PROP_LINK_TARGET_NAME, &target_name) < 0)
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't get property value for targe name")
                if ((ret_value = H5RBDB_link_create(group, loc_params.loc_data.loc_by_name.name, target_name, 
                    lcpl_id, lapl_id, dxpl_id)) < 0 )
                    HGOTO_ERROR(H5E_LINK, H5E_CANTINIT, FAIL, "unable to create link")
                break;
            }
        case H5VL_LINK_CREATE_UD:
            {
                printf("Creating UD links is not yet supported in the Retro BDB VOL plugin!\n");
                break;
            }
        default:
            HGOTO_ERROR(H5E_LINK, H5E_CANTINIT, FAIL, "invalid link creation call")
    }
done:
    FUNC_LEAVE_NOAPI(ret_value)

} /* end H5VL_RetroBDB_link_create() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_link_copy
 *
 * Purpose:	Renames an object within an HDF5 file and copies it to a new
 *              group.  The original name SRC is unlinked from the group graph
 *              and then inserted with the new name DST (which can specify a
 *              new path for the object) as an atomic operation. The names
 *              are interpreted relative to SRC_LOC_ID and
 *              DST_LOC_ID, which are either file IDs or group ID.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_link_copy(void *src_obj, H5VL_loc_params_t loc_params1, 
                      void *dst_obj, H5VL_loc_params_t loc_params2,
                      hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    return 0;
} /* end H5VL_RetroBDB_link_copy() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_link_move
 *
 * Purpose:	Renames an object within an HDF5 file and moves it to a new
 *              group.  The original name SRC is unlinked from the group graph
 *              and then inserted with the new name DST (which can specify a
 *              new path for the object) as an atomic operation. The names
 *              are interpreted relative to SRC_LOC_ID and
 *              DST_LOC_ID, which are either file IDs or group ID.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_link_move(void *src_obj, H5VL_loc_params_t loc_params1, 
                      void *dst_obj, H5VL_loc_params_t loc_params2,
                      hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
    return 0;
} /* end H5VL_RetroBDB_link_move() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_link_get
 *
 * Purpose:	Gets certain data about a link
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_link_get(void *obj, H5VL_loc_params_t loc_params, H5VL_link_get_t get_type, 
                     hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    H5G_loc_t   loc;
    herr_t      ret_value = SUCCEED;    /* Return value */
    H5RBDB_group_t* group;

    FUNC_ENTER_NOAPI_NOINIT
    
    if(H5G_loc_real(obj, loc_params.obj_type, &loc) < 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object")

    group = (H5RBDB_group_t*) obj; // we expect the specified loc_id to be of type group

    switch (get_type) {
        /* H5Lget_info/H5Lget_info_by_idx */
        case H5VL_LINK_GET_INFO:
            {
                HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "LINK_GET_INFO not yet supported")
                /*
                H5L_info_t *linfo  = va_arg (arguments, H5L_info_t *);

                // Get the link information
                if(loc_params.type == H5VL_OBJECT_BY_NAME) { // H5Lget_info
                    if(H5L_get_info(&loc, loc_params.loc_data.loc_by_name.name, linfo, 
                                    loc_params.loc_data.loc_by_name.lapl_id, dxpl_id) < 0)
                        HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to get link info")
                }
                else if(loc_params.type == H5VL_OBJECT_BY_IDX) { // H5Lget_info_by_idx
                    H5L_trav_gibi_t udata;              // User data for callback

                    // Set up user data for retrieving information
                    udata.idx_type = loc_params.loc_data.loc_by_idx.idx_type;
                    udata.order = loc_params.loc_data.loc_by_idx.order;
                    udata.n = loc_params.loc_data.loc_by_idx.n;
                    udata.dxpl_id = dxpl_id;
                    udata.linfo = linfo;
                    if(H5L_get_info_by_idx(&loc, loc_params.loc_data.loc_by_idx.name, &udata, 
                                           loc_params.loc_data.loc_by_idx.lapl_id, dxpl_id) < 0)
                        HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to get link info")
                }
                else
                    HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to get link info")
                */
                break;
            }
        // H5Lget_name_by_idx
        case H5VL_LINK_GET_NAME:
            {
                HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "LINK_GET_NAME not yet supported")
                /*
                char       *name   = va_arg (arguments, char *);
                size_t      size   = va_arg (arguments, size_t);
                ssize_t    *ret    = va_arg (arguments, ssize_t *);
                H5L_trav_gnbi_t udata;              // User data for callback

                // Set up user data for callback
                udata.idx_type = loc_params.loc_data.loc_by_idx.idx_type;
                udata.order = loc_params.loc_data.loc_by_idx.order;
                udata.n = loc_params.loc_data.loc_by_idx.n;
                udata.dxpl_id = dxpl_id;
                udata.name = name;
                udata.size = size;
                udata.name_len = -1;

                // Get the link name
                if(H5L_get_name_by_idx(&loc, loc_params.loc_data.loc_by_idx.name, &udata, 
                                       loc_params.loc_data.loc_by_idx.lapl_id, dxpl_id) < 0)
                    HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to get link info")

                *ret = udata.name_len;
                */
                break;
            }
        // H5Lget_val/H5Lget_val_by_idx
        case H5VL_LINK_GET_VAL:
            {
                void       *buf    = va_arg (arguments, void *);
                size_t     size    = va_arg (arguments, size_t);

                // Get the link information
                if(loc_params.type == H5VL_OBJECT_BY_NAME) { // H5Lget_val
                    if(H5RBDB_link_getval(group, loc_params.loc_data.loc_by_name.name, buf, size, 
                                   loc_params.loc_data.loc_by_name.lapl_id, dxpl_id) < 0)
                        HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to get link value")
                }
                else if(loc_params.type == H5VL_OBJECT_BY_IDX) { // H5Lget_val_by_idx
                    /*
                    H5L_trav_gvbi_t udata;              // User data for callback

                    // Set up user data for retrieving information
                    udata.idx_type = loc_params.loc_data.loc_by_idx.idx_type;
                    udata.order = loc_params.loc_data.loc_by_idx.order;
                    udata.n = loc_params.loc_data.loc_by_idx.n;
                    udata.dxpl_id = dxpl_id;
                    udata.buf = buf;
                    udata.size = size;

                    if(H5L_get_val_by_idx(&loc, loc_params.loc_data.loc_by_idx.name, &udata, 
                                          loc_params.loc_data.loc_by_idx.lapl_id, dxpl_id) < 0)
                        HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to get link val")
                        */
                }
                else
                    HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to get link val")

                break;
            }
        default:
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get this type of information from link")
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_link_get() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_link_specific
 *
 * Purpose:	Specific operations with links
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_link_specific(void *obj, H5VL_loc_params_t loc_params, H5VL_link_specific_t specific_type, 
                          hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    herr_t      ret_value = SUCCEED;    /* Return value */
    H5G_loc_t   loc;
    long latency_us;
    struct timeval begin, end;
    hid_t file_id, group_id;
    H5RBDB_group_t* group;
    H5RBDB_file_t* file;

    FUNC_ENTER_NOAPI_NOINIT

    switch (specific_type) {
        case H5VL_LINK_EXISTS:
            {
                htri_t *ret = va_arg (arguments, htri_t *);
                
                //if(H5G_loc_real(obj, loc_params.obj_type, &loc) < 0)
                //    HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object")

                /* Check for the existence of the link */
                if((*ret = H5RBDB_link_exists(obj, loc_params.loc_data.loc_by_name.name, 
                                      loc_params.loc_data.loc_by_name.lapl_id, dxpl_id)) < 0)
                    HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to specific link info")
                break;
            }
        case H5VL_LINK_ITER:
            {
                hbool_t recursive = va_arg (arguments, hbool_t);
                H5_index_t idx_type = va_arg (arguments, H5_index_t);
                H5_iter_order_t order = va_arg (arguments, H5_iter_order_t);
                hsize_t *idx_p = va_arg (arguments, hsize_t *);
                H5L_iterate_t op = va_arg (arguments, H5L_iterate_t);
                void *op_data = va_arg (arguments, void *);
		
		        if(H5G_loc_real(obj, loc_params.obj_type, &loc) < 0)
		              HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object")
		        //printf("Object type: %d\n", loc_params.obj_type);
                /*if(NULL == (grp = H5G__open_name(loc, group_name, lapl_id, dxpl_id)))
                    HGOTO_ERROR(H5E_SYM, H5E_CANTOPENOBJ, FAIL, "unable to open group")
                printf("Full path: %s\n", loc.path->full_path_r);*/
		
		// we assume that obj is of type group!
		if (loc_params.obj_type != H5I_GROUP){
			printf("Link specific for object other than group not supported!\n");
			return -1;
		}
		group = (H5RBDB_group_t*) obj;
		file = group->file_obj;
		if ((group_id = H5VL_object_register(group, loc_params.obj_type, H5VL_RETROBDB_g, 0)) < 0)
			HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize object handle")
		if (file->file_id == 0){
			if ((file_id = H5VL_object_register(file, H5I_FILE, H5VL_RETROBDB_g, 0)) < 0)
				HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize object handle")
			file->file_id = file_id;
		}
                //gettimeofday(&begin, NULL);
                if((ret_value = H5RBDB_link_iterate(group, file->file_id, group_id, recursive, idx_type, order, 
                                            idx_p, op, op_data, dxpl_id)) < 0)
                    HGOTO_ERROR(H5E_SYM, H5E_BADITER, FAIL, "error iterating over links")
                //gettimeofday(&end, NULL);
                //latency_us = 1000000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec);
                //printf("RBDB iterate: %ld\n", latency_us);
                break;
            }
        case H5VL_LINK_DELETE:
            {
                // not yet implemented!
                HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "LINK_DELETE Not yet supported!")
                break;
            }
        default:
            HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "invalid specific operation")
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_RetroBDB_link_specific() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_object_open
 *
 * Purpose:	Opens a object inside a native h5 file.
 *
 * Return:	Success:	object id. 
 *		Failure:	NULL
 *
 * Programmer:  Mohamad Chaarawi
 *              March, 2012
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_RetroBDB_object_open(void *obj, H5VL_loc_params_t loc_params, H5I_type_t *opened_type, 
                        hid_t dxpl_id, void H5_ATTR_UNUSED **req)
{
	return NULL;
} /* end H5VL_RetroBDB_object_open() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_object_copy
 *
 * Purpose:	Copys a object inside a native h5 file.
 *
 * Return:	Success:	object id. 
 *		Failure:	NULL
 *
 * Programmer:  Mohamad Chaarawi
 *              March, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t 
H5VL_RetroBDB_object_copy(void *src_obj, H5VL_loc_params_t loc_params1, const char *src_name, 
                        void *dst_obj, H5VL_loc_params_t loc_params2, const char *dst_name, 
                        hid_t ocpypl_id, hid_t lcpl_id, hid_t H5_ATTR_UNUSED dxpl_id, void H5_ATTR_UNUSED **req)
{
    return -1;
} /* end H5VL_RetroBDB_object_copy() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_object_get
 *
 * Purpose:	Gets certain data about a file
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Mohamad Chaarawi
 *              March, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_object_get(void *obj, H5VL_loc_params_t loc_params, H5VL_object_get_t get_type, 
                       hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    return -1;
} /* end H5VL_RetroBDB_object_get() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_object_specific
 *
 * Purpose:	Perform a plugin specific operation for an objectibute
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_object_specific(void *obj, H5VL_loc_params_t loc_params, H5VL_object_specific_t specific_type, 
                        hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    return -1;
} /* end H5VL_RetroBDB_object_specific() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_RetroBDB_object_optional
 *
 * Purpose:	Perform a plugin specific operation for an objectibute
 *
 * Return:	Success:	0
 *		Failure:	-1
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2012
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_RetroBDB_object_optional(void *obj, hid_t dxpl_id, void H5_ATTR_UNUSED **req, va_list arguments)
{
    //printf("Object optional!\n");
    return -1;
} /* end H5VL_RetroBDB_object_optional() */
