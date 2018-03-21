/*
 * Programmer:  Dimokritos Stamatakis <dimos@brandeis.edu>
 *              October, 2015
 *
 * Purpose:	The public header file for the Retro BDB VOL plugin.
 */
#ifndef H5VLRetroBDB_H
#define H5VLRetroBDB_H

#define H5VL_RETROBDB	(H5VL_RetroBDB_init())
#define HDF5_VOL_RETROBDB_VERSION_1	1	/* Version number of BDB VOL plugin */

#ifdef __cplusplus
extern "C" {
#endif

H5_DLL hid_t H5VL_RetroBDB_init(void);
H5_DLL herr_t H5Pset_fapl_RetroBDB(hid_t fapl_id);


#ifdef __cplusplus
}
#endif

#endif
