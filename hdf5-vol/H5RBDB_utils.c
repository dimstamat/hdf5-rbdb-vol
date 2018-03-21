/**
 * Author: Dimokritos Stamatakis
 * The Retro Berkeley DB utilities functions
 * March, 2016
 */

#define H5G_FRIEND      /*suppress error about including H5Gpkg   */

#include <db.h>
#include "dbinc/utils.h"
#include "H5Gpkg.h"     /* Groups               */

#include <db.h>
#include <sys/time.h>
#include "H5RBDB.h"
#include "H5RBDB_utils.h"

/*-------------------------------------------------------------------------
 * Function:    H5RBDB_childname_add_prefix
 * Purpose: Adds child prefix to the given group name XXX: __child_XXX
 * Return:  Success:    a pointer to the malloced string containing the child name with the prefix.
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_childname_add_prefix(const char* group_name){
    const char *prefix = "__child_";
    char* dst;
    long unsigned int size = strlen(prefix) + strlen(group_name) + 1;
    dst = (char*) malloc(size*sizeof(char));
    strcpy(dst, prefix);
    strcat(dst, group_name);
    return dst;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_childname_timestamp_add_prefix
 * Purpose: Adds child prefix to the given creation timestamp TTT: __order_crt_child_TTT
 * Return:  Success:    a pointer to the malloced string containing the timestamp with the prefix.
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              July, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_childname_timestamp_add_prefix(unsigned long timestamp){
    const char *prefix = "__order_crt_child_";
    char* dst;
    const int n = snprintf(NULL, 0, "%lu", timestamp);
    if(n <= 0){
	    printf("Error while creating child timestamp prefix!\n");
	    return NULL;
    }
    dst = (char*) malloc(strlen(prefix)*sizeof(char) + n*sizeof(char) + 1);
    sprintf(dst, "%s%lu", prefix, timestamp);
    return dst;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_filename_add_prefix
 * Purpose: Adds prefix to the given filename XXX: __file_XXX
 * Return:  Success:    a pointer to the malloced string containing the filename with the prefix.
 *          Failure:        NULL
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_filename_add_prefix(const char* src){
    const char *prefix = "__file_";
    char* dst;
    long unsigned int size = strlen(prefix) + strlen(src) + 1;
    dst = (char*) malloc(size*sizeof(char));
    strcpy(dst, prefix);
    strcat(dst, src);
    return dst;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_groupname_add_prefix
 * Purpose: Adds prefix to the given group XXX inside file YYY: __group_YYY_XXX
 * Return:  Success:    a pointer to the malloced string containing the group name with the prefix.
 *          Failure:        NULL
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_groupname_add_prefix(const char* src, const char* filename){
    const char* prefix = "__group_";
    char* dst;
    size_t size;
    if (src == NULL){ // this group has no parent! (root group)
        return NULL;
    }
    size = strlen(prefix) + strlen(filename) + 1 + strlen(src)+1;
    dst = (char*) malloc(size*sizeof(char));
    strcpy(dst, prefix);
    strcat(dst, filename);
    strcat(dst, "_");
    strcat(dst, src);
    return dst;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_datasetname_add_prefix
 * Purpose: Adds prefix to the given dataset XXX with type YYY: __dataset_XXX_YYY
 *          For now type can be metadata, or data
 * Return:  Success:    a pointer to the malloced string containing the dataset name with the prefix.
 *          Failure:        NULL
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_datasetname_add_prefix(const char* src, const char* type){
    const char* prefix = "__dataset_";
    char* dst;
    long unsigned int size = strlen(prefix) + strlen(type) + 1 + strlen(src)+1;
    dst = (char*) malloc(size*sizeof(char));
    strcpy(dst, "__dataset_");
    strcat(dst, src);
    strcat(dst, "_");
    strcat(dst, type);
    return dst;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_linkname_add_prefix
 * Purpose: Adds prefix to the given link name XXX: __link_XXX
 * Return:  Success:    a pointer to the malloced string containing the link name with the prefix.
 *          Failure:        NULL
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_linkname_add_prefix(const char* src){
    const char* prefix = "__link_";
    char* dst;
    long unsigned int size = strlen(prefix) + strlen(src)+1;
    dst = (char*) malloc(size*sizeof(char));
    strcpy(dst, "__link_");
    strcat(dst, src);
    return dst;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attrname_add_prefix
 * Purpose: Adds prefix to the given attribute XXX within 
 * container object type (group/dataset) TTT : __attr_XXX_TTT
 * Return:  Success:    a pointer to the malloced string containing the attribute name with the prefix.
 *          Failure:        NULL
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_attrname_add_prefix(const char* name, const char* container_obj){
    const char* prefix = "__attr_";
    char* dst;
    long unsigned int size = strlen(prefix) + strlen(name)+1 + strlen(container_obj) +10;
    dst = (char*) malloc(size*sizeof(char));
    memset(dst, 0, size*sizeof(char));
    strcpy(dst, "__attr_");
    strcat(dst, name);
    strcat(dst, "_");
    strcat(dst, container_obj);
    return dst;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_dataspacename_add_prefix
 * Purpose: Adds prefix to the given dataspace XXX: __dataspace_XXX
 * Return:  Success:    a pointer to the malloced string containing the dataspace name with the prefix.
 *          Failure:        NULL
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_dataspacename_add_prefix(const char* name){
    const char* prefix = "__dataspace_";
    char* dst;
    long unsigned int size = strlen(prefix) + 1+  strlen(name) + 1 ;
    dst = (char*) malloc(size*sizeof(char));
    strcpy(dst, "__dataspace_");
    strcat(dst, name);
    return dst;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_groupname_clear_prefix
 * Purpose: Returns the relative group name without the prefix
 * Return:  Success:    a pointer to the malloced string containing the 
 *                      relative group name without the prefix.
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_groupname_clear_prefix(const char* group_db_name){
    //char* group_name;
    unsigned i;
    // make sure that the group starts with __group_
    if ( strstr(group_db_name, "__group_") == NULL){
        printf("Invalid group name! Must start with __group_!\n");
        return NULL;
    }
    /*if ( ( group_name = strstr(group_db_name, file_name)) == NULL){
        printf("Invalid group name! Must contain file name!\n");
        return NULL;
    }*/
    for (i = strlen(group_db_name)-1; i >=0; i--){
	    if (group_db_name[i] == '/' && i != strlen(group_db_name)-1){
		    break;
	    }
    }
    return strdup(group_db_name+i+1);
    //return strdup(group_name + strlen(file_name)+1);
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_linkname_clear_prefix
 * Purpose: Returns the link name without the prefix
 * Return:  Success:    a pointer to the malloced string containing the 
 *                      link name without the prefix.
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_linkname_clear_prefix(const char* link_db_name){
    char* link_name;
    // make sure that the link db name starts with __link_
    if ( (link_name = strstr(link_db_name, "__link_")) == NULL){
        printf("Invalid link name! Must start with __link_!\n");
        return NULL;
    }
    return strdup(link_name+strlen("__link_"));
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_attrname_clear_prefix
 * Purpose: Returns the attribute name without the prefix and suffix
 * Return:  Success:    a pointer to the malloced string containing the 
 *                      attribute name without the prefix and suffix.
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              June, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_attrname_clear_prefix(const char* attr_db_name){
    char* attr_name;
    char* suffix;
    // make sure that the attribute db name starts with __attr_
    if ( (attr_name = strstr(attr_db_name, "__attr_")) == NULL){
        printf("Invalid attribute name! Must start with __attr_!\n");
        return NULL;
    }
    if((suffix = strstr(attr_db_name, "_group")) == NULL  && (suffix = strstr(attr_db_name, "_dataset")) == NULL ){
	printf("Invalid attribute name! Must end either with _group, or with _dataset!\n");
	return NULL;
    }
    return strndup(attr_name+strlen("__attr_"), strlen(attr_db_name) - strlen("__attr_") - strlen(suffix) );
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_groupname_get_prefix
 * Purpose: Gets a prefix of the given group name
 * Return:  Success:    a pointer to the malloced string containing the prefix of the group name.
 *          Failure:        NULL
 * Programmer:  Dimokritos Stamatakis
 *              March, 2016
 *-------------------------------------------------------------------------*/
char* H5RBDB_groupname_get_prefix(const char * db_name){
    size_t i;
    for (i=0; i<strlen(db_name); i++){
        if(db_name[i] == '/'){
            return strndup(db_name, i);
        }
    }
    return NULL;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_name_fix_slashes
 * Purpose: Removes extra slashes in the given group absolute name
 * Return:  Success:    a pointer to the malloced string containing the group absolute name
 *                      without extra slashes
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
const char* H5RBDB_name_fix_slashes(const char* absolute_name){
	const char* result;
	char* absolute_name_cp;
	int orig_indx,cp_indx;
	int i;
	unsigned found_slash;
	unsigned trailing_slashes;
	
	absolute_name_cp = (char*) malloc((strlen(absolute_name)+1) * sizeof(char));
	found_slash=0;
	cp_indx = 0;
	for (orig_indx=0; orig_indx<strlen(absolute_name); orig_indx++){
	    if(absolute_name[orig_indx] == '/' && found_slash == 1){ // skip the slash when we found a consecutive slash before
		    continue;
	    }
	    if(absolute_name[orig_indx] == '/') {
		  found_slash = 1;
	    }
	    else {
		  found_slash = 0;
	    }
	    absolute_name_cp[cp_indx] = absolute_name[orig_indx];
	    cp_indx++;
	}
	trailing_slashes = 0;
	
	for(i=cp_indx-1; i>0; i--){
	    if (absolute_name_cp[i] == '/'){
		trailing_slashes++;
	    }
	    else {
		    break;
	    }
	}

    absolute_name_cp[cp_indx - trailing_slashes] = '\0';
	result = strdup(absolute_name_cp);
    free(absolute_name_cp);
    return result;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_get_name_start_pos
 * Purpose: Returns the position of the group name within the absolute name
 * Return:  Success:    the position of the group name within the absolute name
 *          Failure:    -1
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
int H5RBDB_get_name_start_pos(const char* absolute_name){
    int i;
    unsigned characters_found;
    i=-1;
    characters_found = 0;
    // in case it is the root
    if(strlen(absolute_name) == 1){
	    return 0;
    }
    for(i=strlen(absolute_name)-1; i>=0; i--){
        if (absolute_name[i] == '/' && characters_found > 0 ){ // '/' found and some characters have already found, we are done!
            i+=1;
            break;
        }
        else if (absolute_name[i] != '/' ){ // a character is found 
            characters_found += 1;
        }
    }
    return i;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_get_parent_name_from_absolute
 * Purpose: Returns the parent name of the given group absolute name
 * Return:  Success:    a pointer to the malloced string containing the 
 *                      given group's parent name
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
const char* H5RBDB_get_parent_name_from_absolute(const char* absolute_name){
    const char* parent_name;
    size_t i;
    if (strcmp(absolute_name, "/") == 0){ // this is the root group and has no parent group
        return NULL;
    }
    i = H5RBDB_get_name_start_pos(absolute_name);
    if (i == 1)     // path always starts with '/', so here the parent is the root group 
        parent_name = strndup(absolute_name, i);
    else    // returned position will include the trailing '/', do not include it!
        parent_name = strndup(absolute_name, i-1);
    return parent_name;
}
/*-------------------------------------------------------------------------
 * Function:    H5RBDB_get_name_from_absolute
 * Purpose: Returns the group name of the given group absolute name
 * Return:  Success:    a pointer to the malloced string containing the 
 *                      group name of the given absolute name
 *          Failure:    NULL
 * Programmer:  Dimokritos Stamatakis
 *              May, 2016
 *-------------------------------------------------------------------------*/
const char* H5RBDB_get_name_from_absolute(const char* absolute_name){
    const char* name;
    size_t i;
    if (strcmp(absolute_name, "/") == 0){ // this is the root group, return it as it is
        return absolute_name;
    }
    i = H5RBDB_get_name_start_pos(absolute_name);
    name = strdup(absolute_name+i);
    return name;
}