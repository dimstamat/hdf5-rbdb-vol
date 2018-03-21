/*
 * VScriptH5HID.hpp
 *
 *  Created on: Jul 25, 2016
 *      Author: dimos
 */

#ifndef FISH_V5_VSCRIPTH5HID_HPP_
#define FISH_V5_VSCRIPTH5HID_HPP_


#include <hdf5.h>
#include <memcore/Interface.hpp>

namespace Wizt{

struct VScriptH5HID : MemCore::Interface<VScriptH5HID>{
	hid_t group_id;
	std::string group_name;

	VScriptH5HID(hid_t g_id, std::string group_name);

	~VScriptH5HID();

};

}

#endif /* FISH_V5_VSCRIPTH5HID_HPP_ */
