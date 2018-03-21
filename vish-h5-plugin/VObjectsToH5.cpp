/*
 * VObjectsToH5.cpp
 *
 *  Created on: Sep 22, 2016
 *      Author: dimos
 */


#include "VObjectsToH5.hpp"
#include "H5utils.hpp"
#include <memcore/Verbose.hpp>

#define PRINT_LATENCIES 0
#define H5_SAVE_DEBUG 0

#if PRINT_LATENCIES == 1
	#define INIT_COUNTING \
            unsigned long long latency_us;  \
            struct timeval begin_counting, end_counting;
#else
	#define INIT_COUNTING
#endif

#if PRINT_LATENCIES == 1
    #define START_COUNTING gettimeofday(&begin_counting, NULL);
#else
    #define START_COUNTING
#endif

#if PRINT_LATENCIES == 1
    #define STOP_COUNTING(...) gettimeofday(&end_counting, NULL); \
        latency_us = 1000000 * (end_counting.tv_sec - begin_counting.tv_sec) + (end_counting.tv_usec - begin_counting.tv_usec); \
        printf(__VA_ARGS__); \
        printf(": %llu\n", latency_us);
#else
    #define STOP_COUNTING(...)
#endif

namespace Wizt{

	string getParamFromAttrH5AsStr(hid_t loc_id, const string& name){
		string valStr = getAttrAsString(loc_id, name.c_str());
		return valStr;
	}

	RefPtr<VValueBase> getParamFromAttrH5(hid_t loc_id, const string& name){
		int a;
		RefPtr<VValueBase> value;
		hid_t attr, atype;
		H5T_class_t type_class;
		string strval = "";
		int intval;
		double doubleval;
		herr_t ret;
		htri_t exists;
		short shortval;
		//cout <<" getting param from attr !"<<endl;
		if ((exists = H5Aexists(loc_id, name.c_str())) <= 0){
			Verbose(0) <<" Attribute " << name << " doesn't existtt!!!";
			return nullptr;
		}
		attr = H5Aopen(loc_id, name.c_str(), H5P_DEFAULT);
		atype  = H5Aget_type(attr); // get attr datatype
		type_class = H5Tget_class(atype); // get attr type class
		switch(type_class){
		case H5T_STRING:
			strval = getStringAttrNoOpen(attr, atype);
			#if H5_LOAD_DEBUG
			printf("Read string attr %s: %s\n", attrName, param_value.c_str());
			#endif
			value = new VValue<string>(strval);
			break;
		case H5T_INTEGER:
			ret = H5Aread(attr, atype, &intval);
			assert(ret >= 0);
			#if H5_LOAD_DEBUG
			printf("Read int attr %s: %d\n", name.c_str(), intval);
			#endif
			value = new VValue<int>(intval);
			break;
		case H5T_FLOAT:
			ret = H5Aread(attr, atype, &doubleval);
			assert(ret >= 0);
			#if H5_LOAD_DEBUG
			printf("Read double attr %s: %lf\n", name.c_str(), doubleval);
			#endif
			value = new VValue<double>(doubleval);
			break;
		case H5T_ENUM:
			ret = H5Aread(attr, atype, &shortval);
			assert(ret >= 0);
			#if H5_LOAD_DEBUG
			printf("Read bool attr %s: %u\n", name.c_str(), boolval);
			param_value = boolval? "true" : "false";
			#endif
			value = new VValue<bool>((shortval == 1)? true: false);
			break;
		default:
			// not supported!
			printf("Type not supported!\n");
			break;
		}
		H5Tclose(atype);
		H5Aclose(attr);
		return value;
	}

	bool storeParamToAttrH5(hid_t loc_id, const string& name, const RefPtr<VValueBase>& value){
		int i;
		double d;
		short b;
		hbool_t hbool;
		string valStr;
		INIT_COUNTING
		if (RefPtr<VValue<int>> intValue = value){ // type int: HDF5 type: H5T_NATIVE_INT, C type: int
			i = *intValue;
			#if H5_SAVE_DEBUG
			printf(" ## Writing %s (as int): %d\n", name.c_str(), i);
			#endif
			START_COUNTING
			addAttr(loc_id, name.c_str(), H5T_NATIVE_INT, &i);
			STOP_COUNTING("add int attr")
		}
		else if (RefPtr<VValue<double>> doubleValue = value){ // type double: HDF5 type: H5T_NATIVE_DOUBLE, C type: double
			d = *doubleValue;
			#if H5_SAVE_DEBUG
			printf(" ## Writing %s (as double): %lf\n", name.c_str(), d);
			#endif
			START_COUNTING
			printf(" @@@@ Saving parameter as H5 attribute (double) : %s: %lf\n", name.c_str(), d);
			addAttr(loc_id, name.c_str(), H5T_NATIVE_DOUBLE, &d);
			STOP_COUNTING("add double attr")
		}
		else if (RefPtr<VValue<bool>> boolValue = value){ // type bool: HDF5 type: H5T_NATIVE_INT, C type: hbool_t
			b = (short) *boolValue;
			#if H5_SAVE_DEBUG
			printf(" ## Writing %s (as hbool): %d\n", name.c_str(), b);
			#endif
			START_COUNTING
			addAttr(loc_id, name.c_str(), H5T_NATIVE_HBOOL, &b);
			STOP_COUNTING("add bool attr")
		}
		else { // all other types will be stored as string
			valStr = value->Text();
			#if H5_SAVE_DEBUG
			printf(" ## Writing %s (as string): %s\n", name.c_str(), valStr.c_str());
			#endif
			START_COUNTING
			printf(" @@@@ Saving parameter as H5 attribute (string) : %s: %s\n", name.c_str(), valStr.c_str());
			addStringAttr(loc_id, name.c_str(), valStr.c_str());
			STOP_COUNTING("add str attr")
		}
		return true;
	}

	bool updateParamToAttrH5(hid_t loc_id, const string& name, const RefPtr<VValueBase>& value){
		int i;
		double d;
		bool b;
		hbool_t hbool;
		string valStr;
		INIT_COUNTING
		//cout<< "updateParamToAttrH5: Writing attribute: name: "<< name << ", value: "<< value->Text()<<endl;
		if (RefPtr<VValue<int>> intValue = value){ // type int: HDF5 type: H5T_NATIVE_INT, C type: int
			i = *intValue;
			#if H5_SAVE_DEBUG
			//printf(" #1: (Context!) Writing (as int) slot name, shadowpool name and value: %s= %d\n", (CurrentSlotName+"{"+SaveObjectName(ShadowPool->Name())+"}").c_str(), i);
			#endif
			START_COUNTING
			updateAttr(loc_id, name.c_str(), H5T_NATIVE_INT, &i);
			STOP_COUNTING("add int attr")
		}
		else if (RefPtr<VValue<double>> doubleValue = value){ // type double: HDF5 type: H5T_NATIVE_DOUBLE, C type: double
			d = *doubleValue;
			#if H5_SAVE_DEBUG
			//printf(" #1: (Context!) Writing (as double) slot name, shadowpool name and value: %s= %.10lf\n", (CurrentSlotName+"{"+SaveObjectName(ShadowPool->Name())+"}").c_str(), d);
			#endif
			START_COUNTING
			printf(" Updating attribute %s value to %lf\n", name.c_str(), d);
			updateAttr(loc_id, name.c_str(), H5T_NATIVE_DOUBLE, &d);
			STOP_COUNTING("add double attr")
		}
		else if (RefPtr<VValue<bool>> boolValue = value){ // type bool: HDF5 type: H5T_NATIVE_INT, C type: hbool_t
			b = *boolValue;
			hbool = b;
			#if H5_SAVE_DEBUG
			//printf(" #1: (Context!) Writing (as bool) slot name, shadowpool name and value: %s= %d\n", (CurrentSlotName+"{"+SaveObjectName(ShadowPool->Name())+"}").c_str(), b);
			#endif
			START_COUNTING
			updateAttr(loc_id, name.c_str(), H5T_NATIVE_HBOOL, &hbool);
			STOP_COUNTING("add bool attr")
		}
		else { // all other types will be stored as string
			valStr = value->Text();
			#if H5_SAVE_DEBUG
			cout<< "updateParamToAttrH5: Writing (as string): name: "<< name << ", value: "<< value->Text()<<endl;
			#endif
			START_COUNTING
			updateStringAttr(loc_id, name.c_str(), valStr.c_str());
			STOP_COUNTING("add str attr")
		}
		return true;
	}
}
