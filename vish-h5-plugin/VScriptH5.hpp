/*
 * VscriptH5.hpp
 *
 *  Created on: Jul 18, 2016
 *      Author: dimos
 */

#ifndef FISH_V5_VSCRIPTH5_HPP_
#define FISH_V5_VSCRIPTH5_HPP_


#include <ocean/plankton/VActionNotifier.hpp>
#include <ocean/plankton/VObject.hpp>
#include <ocean/shrimp/Action.hpp>

#include <fish/v5/VScriptH5AppQuitter.hpp>
//#include <fish/v5/VCreateScriptH5.hpp>

#include <ocean/vscript/VScriptParser.hpp>
#include <fiber/field/Field.hpp>
#include <ocean/shrimp/TimeDependent.hpp>

#include <hdf5.h>


namespace Wizt
{

#define H5_LOAD_DEBUG 0
#define TRAVERSE_LINKS_BY_CRT_ORDER 0

enum SnapshotMethod{
	NativeH5,
	RBDB,
	ASCII
} ;
	using std::string;

	// functions for accessing HDF5 file containing Vish scripts
	herr_t parse_group(hid_t loc_id, const char *name, const H5L_info_t *linfo, void *opdata);
	herr_t parse_attr(hid_t loc_id, const char *name, const H5A_info_t *ainfo, void *opdata);
	herr_t parse_attr_timesnap(hid_t loc_id, const char *name, const H5A_info_t *ainfo, void *opdata);

	typedef struct attr_data {
		Wizt::string obj_name;
		int object_type;
		Wizt::string creator;
		std::map <Wizt::string, bool>& localParamsMap;


		attr_data(Wizt::string& obj_name, int object_type, Wizt::string& creator, std::map <Wizt::string, bool>& localParamsMap):
			obj_name(obj_name), object_type(object_type), creator(creator), localParamsMap(localParamsMap){}
	}attr_data_t;

	struct H5Script {
		// added by Dimos for Vscripts in HDF5
		struct H5FileInfo {
			// group containing script is stored under "/"+scriptName
			string scriptName;
			string scriptUrl;
			string scriptRootGroupName;
			hid_t file_id;
			hid_t group_id;
			hid_t currentSnapshotNativeGroup;
			map<string, hid_t> H5groups;
		} ;

		struct	MemberID
		{
			string	obj,
				member;

			MemberID(const string&o, const string&m)
			: obj(o), member(m)
			{}
			MemberID(){}
		};

		list<std::pair<H5Script::MemberID , H5Script::MemberID >> connections;
		list<string> objects;

		struct	Action
		{
			virtual ~Action() {}

			virtual RefPtr<VScriptValue> exec() = 0;
		};

		RefPtr<VInputCreatorBase> 	*inputcreatorPtr;
		type_info	 		*typeID;
		IdentifierWithUrl		*url_identifier;
		struct MemberID		*member;
		RefPtr<VParameter>		*Vparam;
		RefPtr<VScriptParameters>	*scriptparams;
		RefPtr<VScriptValue>		*scriptvalue;
		RefPtr<ValuePool>		*valuepool;
		struct Action			*action;
		NamedIdentifiers		*NamedIds;
		struct H5FileInfo *h5fileInfo;
		bool loaded = false, currentlyLoading = false;
		bool releasedResources = false;
	};

	class TimeSnap; // defined in VSnapshots.hpp

	// this is the class we will pass to the SnapshotPlayer
	class SnapshotInfo{
	public:
		// required for VValueTrait, for connecting the output of VScriptH5 to the input of VSnapshotPlayer
		bool operator=(const int&) const {return true;}
		bool operator==(const SnapshotInfo&) const {return true;}
		map<RefPtr<VObject>, list<RefPtr<VParameter> >> interpolatedObjects;
		H5Script::H5FileInfo* h5info;
		map<double,TimeSnap> * timeSnapMap;
		SnapshotInfo();
		SnapshotInfo(map<RefPtr<VObject>, list<RefPtr<VParameter> >> objects, H5Script::H5FileInfo* h5info, map<double,TimeSnap>* timeSnap);
	};

	template <>
	class VValueTrait<SnapshotInfo>
	{
	public:

		static  bool setValueFromText(SnapshotInfo&snapInfo, const string&s) {
			return false;
		}

		static  string Text(const SnapshotInfo&snapInfo) {
			return "";
		}
	};

	class	VScriptH5 : public VObject, public TimeDependent
	{
		TypedSlot<Action>	inTakeSnapshot;
		TypedSlot<Action>	inSnapAndInterpolate;
		//out<int> testOut;
		out<SnapshotInfo> snapInfo;

	public:
		RefPtr<VActionNotifier> MyH5Notifier;
		WeakPtr<VScriptH5> VScriptH5loaded = nullptr;
		RefPtr<VScriptH5AppQuitter> appQuitter = nullptr;

		VScriptH5(const string&name, int N, const RefPtr<VCreationPreferences>&VP);
		VScriptH5(const string&fileName, const string&name, int N, const RefPtr<VCreationPreferences>&VP);

		~VScriptH5();

		bool	update(VRequest&Context, double precision);

		void loadH5();
	};

} // namespace Wizt


#define OBJ_TYPE_PROCEDURAL 1
#define OBJ_TYPE_IMPLICITLY_CREATED 2
#define OBJ_TYPE_DELETE_IF_EXISTS 3
#define OBJ_TYPE_SNAPSHOT_GROUP 4
#define OBJ_TYPE_UNDEFINED 5

/* object types:
 * 1: procedural object (no creator)
 * 2: Implicitly created object
 * 3: DELETE IF EXISTS
 *
 *
 * special attributes:
 * __line_feeds_converted_to_vertical_tabs:   1 if line feeds have been converted to vertical tabs
 * __local_params__:					    contains the parameter names of all parameters that are local within an object, separated by \n (attributes within a group)
 * __comments: 								  the user info comments, separated by \n
 * __creator:								  the creator of the object
 * __object_type__:							  one of the above
*/

#define ATTR_LOCAL_PARAMS "__local_params__"
#define ATTR_LINE_FEEDS_TO_VTABS "__line_feeds_converted_to_vertical_tabs__"
#define ATTR_GNAME_ESCAPED "__group_name_escaped__"
#define ATTR_COMMENTS "__comments__"
#define ATTR_CREATOR "__creator__"
#define ATTR_OBJECT_TYPE "__object_type__"
#define ATTR_FILE_LOAD_OBJ_NAME "__file_load_obj_name__"
#define ATTR_FILE_LOAD_CREATION_URL "__file_load_obj_crt_url__"
#define ATTR_FILE_LOAD_SCRIPT_PATH_URL "__file_load_script_path_url__"
#define GROUP_TIME_SNAP "__group_time_snapshot_map__"


#endif /* FISH_V5_VSCRIPTH5_HPP_ */
