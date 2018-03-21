/*
 * VScriptH5HID.cpp
 *
 *  Created on: Jul 25, 2016
 *      Author: dimos
 */

#include "VScriptH5HID.hpp"


namespace Wizt {

extern std::map<std::string, hid_t> VScriptH5_Groups;

VScriptH5HID::VScriptH5HID(hid_t g_id, std::string group_name): group_id(g_id), group_name(group_name){ }

// TODO: solution of using interfaces to store group hid_t of each VObject: We need to fix the destruction problem when closing Vish window in Linux!
// for now just iterate over all vish objects and call the destructor!
VScriptH5HID::~VScriptH5HID(){
	//printf("VScriptH5HID: Destructor for group %s\n", group_name.c_str());
	// close HDF5 group
	if (VScriptH5_Groups.count(group_name) != 0){
		//printf("Closing group...\n");
		//assert(H5Gclose(VScriptH5_Groups[group_name]) == 0);
		//VScriptH5_Groups.erase(group_name);
	}
}

}
