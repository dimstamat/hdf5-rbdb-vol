/*
 * VScriptUpdates.hpp
 *
 * Contains functions for updating the state of the Vish network.
 * Was taken from the Vish ASCII loader and adopted to work with the HDF5 loader.
 *
 *  Created on: Oct 31, 2016
 *      Author: dimos
 */

#ifndef FISH_V5_VSCRIPTUPDATES_HPP_
#define FISH_V5_VSCRIPTUPDATES_HPP_

using namespace std;

namespace Wizt {

	void setupObject(string& objName);
	void addConnection(H5Script::MemberID& fromMember, H5Script::MemberID& toMember);
	void addObjectCreator(string& objectCreator, string& objName);
	void deleteObject(string& objname);
	void addParamLocal(H5Script::MemberID& mID, RefPtr<VValueBase> value, H5Script& h5script);
	void addParamSimple(H5Script::MemberID& mID, RefPtr<VValueBase> value, H5Script& h5script);
	void addParamContext(string& objName, string& paramName, string& contextName, RefPtr<VValueBase> value, H5Script& h5script);
	void addParamProperty(H5Script::MemberID& mID, string& parname, string& propertyType, string& propname, RefPtr<VValueBase> value);
	void addStatementToScriptValue(H5Script& h5script);
	void addScriptValueToDiscardvalue(H5Script& h5script);
	void addParam(string& objName, string& param, RefPtr<VValueBase> value, bool isLocal, H5Script& h5script);
}


#endif /* FISH_V5_VSCRIPTUPDATES_HPP_ */
