/*
 * VscriptH5.hpp
 *
 *  Created on: Sep 22, 2016
 *      Author: dimos
 */

#ifndef FISH_V5_VOBJECTSTOH5_HPP_
#define FISH_V5_VOBJECTSTOH5_HPP_

#include <ocean/plankton/VObject.hpp>
#include <hdf5.h>
#include <sys/time.h>

namespace Wizt {

RefPtr<VValueBase> getParamFromAttrH5(hid_t loc_id, const string& name);
string getParamFromAttrH5AsStr(hid_t loc_id, const string& name);
bool storeParamToAttrH5(hid_t loc_id, const string& name, const RefPtr<VValueBase>& value);
bool updateParamToAttrH5(hid_t loc_id, const string& name, const RefPtr<VValueBase>& value);

}


#endif // FISH_V5_VOBJECTSTOH5_HPP_
