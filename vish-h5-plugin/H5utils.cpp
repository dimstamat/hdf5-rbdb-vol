/*
 * H5utils.cpp
 *
 *  Created on: Sep 21, 2016
 *      Author: dimos
 */


#include "H5utils.hpp"
using namespace std;
#include <iostream>
#include <ocean/plankton/VModules.hpp>
//#include "VCreateScriptH5.hpp"

#define H5_LOAD_DEBUG 0

namespace Wizt {

	/**
	 * Gets the attribute with given name from given group, of given type, if it exists. This function is used for non-string attributes
	 * group_id: the location id of the HDF5 object (group) from which we want to get the attribute
	 * attrName: the name of the attribute
	 * attrType: the type of the attribute
	 * value: 	 the location where the attribute's value is written. The caller must ensure to correctly allocate memory for this.
	 * returns 0 on success, 1 if attribute was not found, -1 if there was an error
	 */
	int getAttr(hid_t group_id, const char* attrName, hid_t attrType, void* value){
		hid_t attr;
		herr_t ret;
		int result;
		htri_t exists;

		if ((exists = H5Aexists(group_id, attrName)) > 0){
			attr = H5Aopen(group_id, attrName, H5P_DEFAULT);
			ret  = H5Aread(attr, attrType, value);
			assert(ret >= 0);
			ret =  H5Aclose(attr);
			result = 0;
		}
		else if (exists == 0){
			result = 1;
		}
		else {
			printf("Error reading attribute %s\n", attrName);
			result = -1;
		}
		return result;
	}

	/**
	 * Gets the attribute with given name from given group and returns converted to a string, if it exists.
	 * group_id: the location id of the HDF5 object (group) from which we want to get the attribute
	 * attrName: the name of the attribute
	 * returns the string representation of the requested attribute's value and blank string if attribute is not found.
	 */
	string getAttrAsString(hid_t group_id, const char* attrName){
		hid_t attr, atype;
		H5T_class_t type_class;
		string param_value = "";
		int intval;
		double doubleval;
		herr_t ret;
		htri_t exists;
		short boolval;
		if ((exists = H5Aexists(group_id, attrName)) <= 0){
			printf("!! Attribute %s does not exist!!\n", attrName);
			return "";
		}
		attr = H5Aopen(group_id, attrName, H5P_DEFAULT); // open attr using its name
		atype  = H5Aget_type(attr); // get attr datatype
		type_class = H5Tget_class(atype); // get attr type class
		//printf("Attribute name: %s\n", attrName);
		//printf("Type: %ld\nType class: %d\nString Type class: %d\nFloat Type class: %d\nInt type: %ld\nDouble type: %ld\nHztype: %ld\nEnum type: %ld\n", atype, type_class, H5T_STRING, H5T_FLOAT, H5T_NATIVE_INT, H5T_NATIVE_DOUBLE, H5T_NATIVE_HBOOL, H5T_ENUM);
		switch(type_class){
		case H5T_STRING:
			param_value = getStringAttrNoOpen(attr, atype);
			#if H5_LOAD_DEBUG
			printf("Read string attr %s: %s\n", attrName, param_value.c_str());
			#endif
			break;
		case H5T_INTEGER:
			// HBOOL_T is also categorized under type H5T_INTEGER. Actual type for int and hbool in our case is H5T_STD_I32LE.
			ret = H5Aread(attr, atype, &intval);
			assert(ret >= 0);
			#if H5_LOAD_DEBUG
			printf("Read int attr %s: %d\n", attrName, intval);
			#endif
			//printf("Type equal to H5T_NATIVE_HBOOL? %d\n", H5Tequal(atype, H5T_NATIVE_HBOOL));
			param_value = to_string(intval);
			break;
		case H5T_FLOAT:
			ret = H5Aread(attr, atype, &doubleval);
			assert(ret >= 0);
			#if H5_LOAD_DEBUG
			printf("Read double attr %s: %lf\n", attrName, doubleval);
			#endif
			param_value = to_string(doubleval);
			printf(" !!! IT WAS DOUBLE !!!\n");
			break;
		case H5T_ENUM:
			ret = H5Aread(attr, atype, &boolval);
			assert(ret >= 0);
			#if H5_LOAD_DEBUG
			printf("Read bool attr %s: %u\n", attrName, boolval);
			param_value = boolval? "true" : "false";
			#endif
			break;
		default:
			// not supported!
			printf("Type not supported!\n");
			break;
		}
		printf(" !!! Getting attribute %s as string: %s!\n", attrName, param_value.c_str());
		H5Tclose(atype);
		H5Aclose(attr);
		return param_value;
	}
	char* add_prefix(const char* src){
	    const char *prefix = "__file_";
	    char* dst;
	    long unsigned int size = strlen(prefix) + strlen(src) + 1;
	    dst = (char*) malloc(size*sizeof(char));
	    strcpy(dst, prefix);
	    strcat(dst, src);
	    return dst;
	}

	/* adds an attribute with given name, type and value in given object id.
	 *obj_id: the location id of the HDF5 object to which we want to add the attribute
	 *attr_name: the name of the attribute
	 *type_id: 	the type of the attribute
	 *attr_value: the value of the attribute
	 */
	void addAttr(hid_t obj_id, const char * attr_name, hid_t type_id, void* attr_value){
		hid_t ads;   /* attribute dataspace id */
		hid_t attr;	 /* attribute id */
		herr_t ret;	 /* return value */

		//printf("Writing attribute %s to object %d\n", attr_name, obj_id);
		ads = H5Screate(H5S_SCALAR);
		if (type_id == H5T_NATIVE_INT){
			attr = H5Acreate2(obj_id, attr_name, H5T_NATIVE_INT, ads, H5P_DEFAULT, H5P_DEFAULT);
			assert(attr >= 0);
			ret = H5Awrite(attr, type_id, (int*)attr_value );
		}
		else if(type_id == H5T_NATIVE_DOUBLE){
			attr = H5Acreate2(obj_id, attr_name, H5T_NATIVE_DOUBLE, ads, H5P_DEFAULT, H5P_DEFAULT);
			assert(attr >= 0);
			ret = H5Awrite(attr, type_id, (double*)attr_value );
		}
		else if(type_id == H5T_NATIVE_HBOOL){ // when storing HBOOL store it as enum, since by default HBOOL is stored as integer!
			hid_t bool_enum = H5Tcreate(H5T_ENUM, sizeof(short));
			herr_t status;
			short val;
			hid_t space;
			hsize_t dim[]= {1};
		    space = H5Screate_simple (1, dim, NULL);
			status = H5Tenum_insert(bool_enum, "true", (val=1, &val));
			assert(status >=0);
			status = H5Tenum_insert(bool_enum, "false", (val=0, &val));
			assert(status >=0);
			attr = H5Acreate2(obj_id, attr_name, bool_enum, space, H5P_DEFAULT, H5P_DEFAULT);
			assert(attr >= 0);
			ret = H5Awrite(attr, bool_enum, (short*)attr_value );
		}
		assert(ret >= 0);
		ret = H5Sclose(ads);
		assert(ret >= 0);
		ret = H5Aclose(attr);
		assert(ret >= 0);
	}
	/* updates the value of an attribute with given name, type in given object id.
	 * obj_id: the location id of the HDF5 object to which we want to update the attribute
	 * attr_name: the name of the attribute
	 * type_id: 	the type of the attribute
	 * attr_value: the value of the attribute
	 */
	void updateAttr(hid_t obj_id, const char * attr_name, hid_t type_id, void* attr_value){
		hid_t attr;	 /* attribute id */
		herr_t ret;	 /* return value */

		attr = H5Aopen(obj_id, attr_name, H5P_DEFAULT);
		if (type_id == H5T_NATIVE_INT){
			ret = H5Awrite(attr, type_id, (int*)attr_value );
		}
		else if(type_id == H5T_NATIVE_DOUBLE){
			ret = H5Awrite(attr, type_id, (double*)attr_value );
		}
		else if(type_id == H5T_NATIVE_HBOOL){
			//ret = H5Awrite(attr, type_id, (hbool_t*)attr_value );
			// when storing HBOOL store it as enum, since by default HBOOL is stored as integer!
			hid_t bool_enum = H5Tcreate(H5T_ENUM, sizeof(short));
			herr_t status;
			short val;
			hid_t space;
			hsize_t dim[]= {1};
			space = H5Screate_simple (1, dim, NULL);
			status = H5Tenum_insert(bool_enum, "true", (val=1, &val));
			assert(status >=0);
			status = H5Tenum_insert(bool_enum, "false", (val=0, &val));
			assert(status >=0);
			ret = H5Awrite(attr, bool_enum, (short*)attr_value );
		}
		assert(ret >= 0);
		ret = H5Aclose(attr);
		assert(ret >= 0);
	}
	/* adds a string attribute with given name and value in given object id.
	 *obj_id: the location id of the HDF5 object to which we want to add the attribute
	 *attr_name: the name of the attribute
	 *attr_value: the value of the attribute
	 */
	void addStringAttr(hid_t obj_id, const char * attr_name, const char * attr_value){
		hid_t ads;   /* attribute dataspace id */
		hid_t atype; /* attribute type id */
		hid_t attr;	 /* attribute id */
		herr_t ret;	 /* return value */

		//printf("Writing attribute %s to object %d\n", attr_name, obj_id);
		ads = H5Screate(H5S_SCALAR);
		atype = H5Tcopy(H5T_C_S1);
		H5Tset_size(atype, strlen(attr_value)+1);
		H5Tset_strpad(atype,H5T_STR_NULLTERM);
		attr = H5Acreate2(obj_id, attr_name, atype, ads, H5P_DEFAULT, H5P_DEFAULT);
		assert(attr >= 0);
		ret = H5Awrite(attr, atype, attr_value);
		assert(ret >= 0);
		ret = H5Sclose(ads);
		assert(ret >= 0);
		ret = H5Tclose(atype);
		assert(ret >= 0);
		ret = H5Aclose(attr);
		assert(ret >= 0);
	}

	/* updates the value of a string attribute with given name in given object id.
	 *obj_id: the location id of the HDF5 object to which we want to add the attribute
	 *attr_name: the name of the attribute
	 *attr_value: the value of the attribute
	 */
	int updateStringAttr(hid_t obj_id, const char * attr_name, const char * attr_value){
		hid_t attr, atype;
		herr_t ret;
		  htri_t exists;
		  if ((exists = H5Aexists(obj_id, attr_name)) > 0){
			//printf("updating attribute %s to value %s\n", attr_name, attr_value);
			atype = H5Tcopy(H5T_C_S1);
			attr = H5Aopen(obj_id, attr_name, H5P_DEFAULT);
			H5Tset_size(atype, strlen(attr_value)+1);
			H5Tset_strpad(atype,H5T_STR_NULLTERM);
			ret  = H5Awrite(attr, atype, attr_value);
			assert(ret >= 0);
			ret = H5Tclose(atype);
			assert(ret >= 0);
			ret =  H5Aclose(attr);
			return 0;
		  }
		  else if (exists == 0){
			printf("Attribute %s does not exist!\n", attr_name);
			return -1;
		  }
		  else {
			printf("Error reading attribute %s\n", attr_name);
			return -2;
		  }
	}
	/**
	 * returns the string attribute with given name from given attribute, assuming that attribute is already opened.
	 * Caller is responsible for closing the attribute when done.
	 * attr_id: the location id of the desired HDF5 attribute
	 * atype: the type of the attribute
	 * returns the value of the attribute on success, or assertion error on failure
	 */
	string getStringAttrNoOpen(hid_t attr_id, hid_t atype){
		hid_t atype_mem;
		hsize_t attr_val_size;
		char* attr_val;
		herr_t ret;
		string attrValStr;

		atype_mem = H5Tget_native_type(atype, H5T_DIR_ASCEND);
		attr_val_size = H5Aget_storage_size(attr_id);
		// TODO: Fix the attr to update the store size if new string value we added is larger than the one already there!
		attr_val = static_cast<char*>(malloc((attr_val_size+100)*sizeof(char) ));
		assert((ret = H5Aread(attr_id, atype_mem, attr_val)) >=0);
		attrValStr = string(attr_val);
		free(attr_val);
		assert(H5Tclose(atype_mem) >=0);
		return attrValStr;
	}
	/**
	 * returns the string attribute with given name from given group, if exists
	 * group_id: the location id of the HDF5 object (group) from which we want to get the attribute
	 * attrName: the name of the attribute
	 * returns the value of the attribute on success, or empty string if the attribute cannot be found, or if there was an error
	 */
	string getStringAttr(hid_t group_id, const char* attrName){
		hid_t attr, atype;
		herr_t ret;
		string attrValStr;
		htri_t exists;

		if ((exists = H5Aexists(group_id, attrName)) > 0){
			attr = H5Aopen(group_id, attrName, H5P_DEFAULT);
			atype = H5Aget_type(attr);
			attrValStr = getStringAttrNoOpen(attr, atype);
			ret = H5Aclose(attr);
			ret = H5Tclose(atype);
			assert(ret == 0);
		}
		else if (exists == 0){
			attrValStr = "";
		}
		else {
			printf("Error reading attribute %s!\n", attrName);
		}
		return attrValStr;
	}
}
