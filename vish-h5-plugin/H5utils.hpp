/*
 * H5utils.h
 *
 *  Created on: Sep 21, 2016
 *      Author: dimos
 */

#ifndef FISH_V5_H5UTILS_HPP_
#define FISH_V5_H5UTILS_HPP_

#include <hdf5.h>

using namespace std;
#include <iostream>

namespace Wizt {
	int getAttr(hid_t group_id, const char* attrName, hid_t attrType, void* value);
	string getAttrAsString(hid_t group_id, const char* attrName);
	void addStringAttr(hid_t obj_id, const char * attr_name, const char * attr_value);
	int updateStringAttr(hid_t obj_id, const char * attr_name, const char * attr_value);
	string getStringAttrNoOpen(hid_t attr_id, hid_t atype);
	string getStringAttr(hid_t group_id, const char* attrName);
	void addAttr(hid_t obj_id, const char * attr_name, hid_t type_id, void* attr_value);
	void updateAttr(hid_t obj_id, const char * attr_name, hid_t type_id, void* attr_value);
}


#endif /* FISH_V5_H5UTILS_HPP_ */
