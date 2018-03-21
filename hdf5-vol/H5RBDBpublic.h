#ifndef __H5RBDBPUBLIC_H__
#define __H5RBDBPUBLIC_H__

#ifdef __cplusplus
extern "C" {
#endif

int H5RBDB_txn_begin(hid_t loc_id);
int H5RBDB_txn_commit(hid_t loc_id);
void* H5RBDB_get_txn(hid_t loc_id);

u_int32_t H5RBDB_declare_snapshot(hid_t loc_id);
herr_t H5RBDB_as_of_snapshot(hid_t loc_id, u_int32_t snap_id);
herr_t H5RBDB_retro_free(hid_t loc_id);


#ifdef __cplusplus
}
#endif

#endif
