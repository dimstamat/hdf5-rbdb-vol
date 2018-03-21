#include "VScriptH5.hpp"
#include "VSnapshots.hpp"
#include <ocean/plankton/VCreator.hpp>
#include "VScriptH5Saver.hpp"

// for creating VObjects
#include <ocean/vscript/VScriptActions.hpp>
#include <ocean/vscript/VScriptInterface.hpp>
#include <ocean/vscript/VObjectSystemExecutor.hpp>
#include <ocean/vscript/VCreateScript.hpp>
#include <ocean/plankton/VObjectUserInfo.hpp>
#include <ocean/plankton/VLoader.hpp>
#include <ocean/plankton/VAcception.hpp>
#include <ocean/plankton/VScheduler.hpp>
#include <ocean/plankton/VModules.hpp>

#include <fiber/field/Field.hpp>
using namespace std;
#include <iostream>
#include <memcore/Verbose.hpp>
#include "VCreateScriptH5.hpp"
#include "VScriptH5Snapshot.hpp"
#include "VObjectsToH5.hpp"
#include "HDF5init.hpp"
#include "VScriptH5HID.hpp"
#include "VScriptH5AppQuitter.hpp"

#include <hdf5.h>
#include "H5utils.hpp"
#include "VScriptUpdates.hpp"

#include <unistd.h>
#include <string>

using namespace Eagle;

namespace Wizt
{

extern hid_t Vscript_fapl;
extern bool Vscript_withRBDB;
extern bool inSnapshotPlayer;
bool saveAsAscii = false;
SnapshotMethod smethod;

	// we store two different data structures to store opened H5 files and groups.
	// one for the case of Vish script loading and one for saving.
	// VScriptH5_Groups and File are for the saving part.
	extern map<string, hid_t> VScriptH5_Groups;
	extern pair<hid_t, string> VScriptH5_File;
	extern bool VScriptH5_Saving;
	extern bool VScriptH5_Saved;

	// required for the Snapshot + Interpolate feature
	SnapAndInterpolate * snapAndInterpolate;
	Snapshots * snapshots;
	unsigned timesteps = 80;
	unsigned manualScripts = 5;

	H5Script h5script;

	struct VObjectsH5Destruct : VManagedObjectIterator {
		bool apply(int priority,
					   const RefPtr<VManagedObject>&VMO,
					   const std::string&ModulePath) override {
				RefPtr<VObject> VObj = VMO;
				if (!VObj)
					return true;
				// check the type of the object!! If it is VScriptH5, we don't need to save it! It will be created every time we load a v5 script
				if (VObj->type_key() == VScriptH5Type){
					return true;
				}
				//printf("VScriptH5::VObjectsH5Destruct: Removing interface of %s\n", VObj->getName().c_str());
				if (RefPtr<VScriptH5HID> vscriptH5HID = interface_cast<VScriptH5HID>(VObj)){
					VObj->removeInterface(typeid(VScriptH5HID));
				}
				return true;
		}
	};

	struct myH5Notifier : VActionNotifier {
			WeakPtr<VScriptH5> Me;
			unsigned	ExecID;

		private:
			myH5Notifier();
			myH5Notifier(const myH5Notifier&);

		public:
			myH5Notifier(const WeakPtr<VScriptH5>&T)
			: Me(T)
			, ExecID(0)
			{
			//printf(" $$$$$$$$$ myH5Notifier\n");
			}


			~myH5Notifier()
			{
			}

			void createVObject(const RefPtr<VObject>&vobj, const Intercube&CreationContext,
					   const WeakPtr<VCreatorBase>&crec) override
			{
			}

			/**
			   The attach() function is called whenever a parameter
			   is attached to the slot of an object. It is called
			   just <b>before</b> this attachment takes place.
			   @param DestSlot	The slot within object VObjDst whose parameter will be modified.
			   @param ProvidedParam The parameter that will replace the one in the slot
			*/
			bool attachPrefix(const RefPtr<VParameter>&ProvidedParam,
					  const RefPtr<VSlot>&DestSlot)
			{
				return true;
			}

			struct	ParamChange
			{
				string	Object, Parameter, Value, Context;

				bool operator==(const ParamChange&P) const
				{
					return Object == P.Object &&
						Parameter == P.Parameter &&
						Value == P.Value &&
						Context == P.Context;
				}
			};

			ParamChange	LastChange;

			void	changeParameter(const WeakPtr<VParameter>&ModifiedParam,
						const RefPtr<ValuePool>&Context,
						const WeakPtr<VValueBase>&NewValue,
						const string&member) override
			{
				string objectStr, paramStr;
				size_t pos;
				//hid_t h5group_id;
				if(inSnapshotPlayer)
					return;
				Verbose(0) << "NewValue: "<< NewValue << ", Context: "<< Context;
				if (!ModifiedParam || !NewValue )
					return;
				Verbose(0) << "VActionNotifier::changeParameter: changed to " << NewValue->Text();
				if (VScriptH5_Saving || h5script.currentlyLoading)
					return;
				if (( pos = ModifiedParam->Name().find(".")) == string::npos)
					return;
				Verbose(0) << " $$$$$$$$$ VScriptH5, VActionNotifier::changeParameter [" << (ModifiedParam? ModifiedParam->Name():"") <<"], member: " << member << ", new val: " <<
					(NewValue? NewValue->Text() : "" ) <<
					", context: " << (Context? Context->Name().c_str(): "");
				RefPtr<VObject> VObj  = ModifiedParam->getOwner();
				if(typeid(VObj) == typeid(VScriptH5))
					return;
				if (!VObj){ // sometimes the owner VObject is NULL, but VObject with that name exists!
					objectStr = ModifiedParam->Name().substr(0, pos);
					paramStr = ModifiedParam->Name().substr(pos+1, ModifiedParam->Name().length()-pos);
					VObj = VObject::find(objectStr);
					//if(VObj)
						//printf("Object %s found!!\n", VObj->getName().c_str());
				}
				//printf("Object Owner: %s\n", VObj->getName().c_str());
				if (smethod == RBDB){
					// no need to check for the VScriptH5HID for now!
					//if (RefPtr<VScriptH5HID> vscriptH5HID = interface_cast<VScriptH5HID>(VObj)){
						//Verbose(0) << "LET'S SEE HOW IS VSCRIPH5HID! (2) ModifiedParam->Name(): "<< ModifiedParam->Name()<< " creator: "<< ModifiedParam->getCreator();
						objectStr = ModifiedParam->Name().substr(0, pos);
						paramStr = ModifiedParam->Name().substr(pos+1, ModifiedParam->Name().length()-pos);
						if (h5script.h5fileInfo != nullptr){
							if (h5script.h5fileInfo->H5groups.find(h5script.h5fileInfo->scriptRootGroupName+"/"+objectStr) != h5script.h5fileInfo->H5groups.end()){
								Verbose(0)<< "Getting H5 group id for: " << h5script.h5fileInfo->scriptRootGroupName+"/"+objectStr;
								hid_t group_id = h5script.h5fileInfo->H5groups[h5script.h5fileInfo->scriptRootGroupName+"/"+objectStr];
								if(group_id > 0){
									Verbose(10) << "LET'S SEE HOW IS VSCRIPH5HID! (3)";
									Verbose(0) << "VSCriptH5: modified param "<<  paramStr <<" from group " << objectStr;
									if(Context){
										Verbose(0)<<" Context: " << Context->Name();
										paramStr+="{"+Context->Name()+"}";
									}

									assert(updateParamToAttrH5(group_id, paramStr, NewValue));
									#if RETRO_BDB_VOL
										if (Vscript_withRBDB) {
										//H5RBDB_txn_commit(Vscript_fapl);
										//H5RBDB_txn_begin(Vscript_fapl);
										}
									#endif
								}
							}
							else{
								Verbose(10) <<"changeParameter: "<< objectStr << " not found in H5groups";
							}
						}
					//}
				}
				/*
				objectStr = ModifiedParam->Name().substr(0, pos);
				paramStr = ModifiedParam->Name().substr(pos+1, ModifiedParam->Name().length()-pos);

				h5group_id = h5script.h5fileInfo->H5groups[h5script.h5fileInfo->scriptRootGroupName+"/"+objectStr];
				if(h5group_id > 0){
					printf("Adding attribute to %ld\n", h5group_id);
					addStringAttr(h5group_id, paramStr.c_str(), NewValue->Text().c_str());
					//if (Vscript_withRBDB)
					//	H5RBDB_txn_commit(Vscript_fapl);
					//if (Vscript_withRBDB)
						//H5RBDB_txn_begin(Vscript_fapl);
				}*/

			}
	};


	static void releaseResources(){
		hid_t loadedFile_id;
		if (h5script.releasedResources)
			return;
		// commit transaction because we were probably running retrospective queries before! Retro has an issue if you don't commit the transaction
		// after retrospective queries and then attempt to perfrom updates.
	#if RETRO_BDB_VOL
			if(Vscript_withRBDB){
				H5RBDB_txn_commit(Vscript_fapl);
				H5RBDB_txn_begin(Vscript_fapl);
			}
	#endif
		// first save the time-snap map in a group within the same H5 script.
		snapshots->storeTimeSnapH5();
		// we need to close resources from the h5info->H5groups map since even the VScript saver can initialize them!
		//if (h5script.loaded){ // check if we loaded a script, so that to close the H5 resources we opened!
			//printf("releasing VScriptH5 resources. Now I see %ld groups\n", h5script.h5fileInfo->H5groups.size());
			// first close the snapshot groups!
			int sizeFirst = h5script.h5fileInfo->H5groups.size();
			int removed=0;
			if (smethod == NativeH5){
				//printf("Close snapshot groups (except snapshot root groups)!\n");
				for (map<string,hid_t>::iterator it = h5script.h5fileInfo->H5groups.begin(); it !=h5script.h5fileInfo->H5groups.end(); it++){
					if(it->first.find("/__snap_")== string::npos){
						//printf("Group not from shnapshot! %s\n", it->first.c_str());
						continue;
					}
					unsigned num=0;
					for(unsigned i=0; i<it->first.size(); i++){
						if(it->first[i]=='/')
							num++;
					}
					if (num == 2){
						//printf("That's a snapshot root group! %s\n", it->first.c_str());
						continue;
					}
					// TODO: See why sometimes there are non existent group names!
					if (it->second <= 0){
						//printf("hid_t for this group is not valid! %s\n", it->first.c_str());
						continue;
					}
					//printf("Closing group %s\n", it->first.c_str());
					assert(H5Gclose(it->second) == 0);
					it->second = -1;
					removed++;
				}
				//int sizeAfter = h5script.h5fileInfo->H5groups.size();
				//printf("^^Size after: %d, (removed %d)\n",sizeAfter, removed);
			}
			//printf("Close other groups (including snapshot root groups)!\n");
			for (pair<string, hid_t> group : h5script.h5fileInfo->H5groups){
				// TODO: See why sometimes there are non existent group names!
				if (group.second <= 0){
					continue;
				}
				//printf("Closing group %s\n", group.first.c_str());
				assert(H5Gclose(group.second) == 0);
			}
			// when we save a script as .v5, we also include the root group in the map, since we might want to create a script from another script, and
			// we want to distinguish between the two root groups. Thus, it is closed below when we iterate over the map elements.
			if (h5script.loaded){
				//printf("Closing root group\n");
				assert(H5Gclose(h5script.h5fileInfo->group_id) == 0);
			}
			//printf("Closing Vscript H5 file!\n");
			assert(H5Fclose(h5script.h5fileInfo->file_id) == 0);
			loadedFile_id = h5script.h5fileInfo->file_id;
#if RETRO_BDB_VOL
			if(Vscript_withRBDB){
				H5RBDB_txn_commit(Vscript_fapl);
				H5VL_terminate_vol_RBDB(Vscript_fapl);
			}
#endif
			delete h5script.h5fileInfo;
			h5script.releasedResources = true;
		//}
		if (VScriptH5_Saved) { // check if we saved a script, so that to close the H5 resources we created
			VObjectsH5Destruct vObjectsH5Destruct;
			VObject::traverse(vObjectsH5Destruct);
			//printf("We saved a script, close the groups!\n");
			// we may save VscriptH5 in the same file, which we closed right before. Don't close it if its already closed!
			/*if (loadedFile_id != VScriptH5_File.first){
				assert(H5Fclose(VScriptH5_File.first) == 0);
			}*/
/*#if RETRO_BDB_VOL
			if(Vscript_withRBDB){
				H5RBDB_txn_commit(Vscript_fapl);
				H5VL_terminate_vol_RBDB(Vscript_fapl);
			}
#endif
*/
			//h5script.releasedResources = true;
		}
	}

	map<string, list<string>> interpolatedObjectsStr;
	map<RefPtr<VObject>, list<RefPtr<VParameter> >> interpolatedObjects;

	VScriptH5::VScriptH5(const string&name, int N, const RefPtr<VCreationPreferences>&VP)
	: VObject(name, N, VP)
	, inTakeSnapshot(this,"Take Snapshot", new VAction() )
	, inSnapAndInterpolate(this, "Snapshot + Interpolate", new VAction() )
	, snapInfo(Deferred() )
	{
		list<string> params;
		params.push_back("observer");
		params.push_back("headpoint");
		interpolatedObjectsStr.insert(pair<string, list<string>>("Camera", params));
		params.clear();
		params.push_back("min");
		params.push_back("max");
		//interpolatedObjectsStr.insert(pair<string, list<string>>("Range100_for_HeightDots <- GridOfWindTunnelGalaxy < _grid_colorrange", params));
		interpolatedObjectsStr.insert(pair<string, list<string>>("Range100_for_HeightDotsGridOfWindTunnelGalaxygrid_colorrange", params));
		if (Vscript_withRBDB)
			smethod = RBDB;
		else if(saveAsAscii)
			smethod = ASCII;
		else
			smethod = NativeH5;
		for (auto const& obj : interpolatedObjectsStr){
			RefPtr<VObject> VObj = VObject::find(obj.first);
			if (VObj){
				list<RefPtr <VParameter>> Vparams;
				for (auto const& param: obj.second){
					RefPtr<VParameter> Vparam = VObj->getParameter( param );
					if (Vparam)
						Vparams.push_back(Vparam);
					else
						Verbose(0) << " VScript::VScript: Cannot find parameter "<< param;
				}
				interpolatedObjects.insert(pair<RefPtr<VObject>, list<RefPtr<VParameter> >> (VObj,Vparams));
			}
			else {
				Verbose(0) << " VScript::VScript: Cannot find object "<< obj.first;

			}
		}

		Verbose(0)<<" OK 1!!";
		h5script.h5fileInfo = new H5Script::H5FileInfo();
		h5script.h5fileInfo->file_id = 0;
		// "Camera" is the object of which the parameters we want to interpolate
		if(smethod == RBDB){ // take a BDB snapshot of VScript's current state
			snapAndInterpolate = new SnapAndInterpolateBDB(h5script, timesteps, manualScripts);
			snapshots = new SnapshotsBDB(h5script, interpolatedObjects);
			//Verbose(0) << "Saving in BDB VOL is not yet implemented!";
		} else if (smethod == ASCII){ // save the VScript current state in ASCII format using the VScriptSaver
			Verbose(0) << "Saving as ASCII is not yet implemented!";
		} else if (smethod == NativeH5){ // native HDF5: store VScript current state as a seprate group in same HDF5 file containing the script
			snapAndInterpolate = new SnapAndInterpolateNativeH5(h5script, timesteps, manualScripts);
			snapshots = new SnapshotsNativeH5(h5script, interpolatedObjects);
		}
		//snapshots->setInterpolatedParams(snapAndInterpolate->getInterpolatedParams());
		VScriptH5loaded = self();
		Verbose(45) <<" $$$$$$$$$ VScriptH5::VScriptH5, name: " << name;
		MyH5Notifier = new myH5Notifier(this);
		MyH5Notifier->activate();
		addSaver( "HDF5 Vish script", new VishScriptH5Saver(h5script.h5fileInfo) );
#if RETRO_BDB_VOL
		if (Vscript_withRBDB)
			addSaver( "Snapshot", new VScriptH5Snapshot() );
#endif 
		Verbose(0)<<" OK 2!!";
		appQuitter = new VScriptH5AppQuitter(&releaseResources);
		VGlobals::theGlobals().ApplicationQuitters.insert(appQuitter);
		// reset age to avoid early triggering
		inTakeSnapshot->getParameter()->val()->age() = Ageable::InfinitelyOld();
		inTakeSnapshot->setProperty<string>("shortcut", "s");
		inSnapAndInterpolate->getParameter()->val()->age() = Ageable::InfinitelyOld(); 
		inSnapAndInterpolate->setProperty<string>("shortcut", "i");
		if (snapAndInterpolate != nullptr && snapshots != nullptr) // SnapshotInfo is the class we will pass to the SnapshotPlayer
			snapInfo = out<SnapshotInfo>(this, "Snapshots Info", SnapshotInfo(interpolatedObjects, h5script.h5fileInfo, snapshots->getTimeSnapMap()));
		Verbose(45) << "Interpolated objects: " ;
		for(auto obj : interpolatedObjects)
			Verbose(45) << obj.first->Name();
		Verbose(0)<<" OK 3!!";
	}

	//TODO: If we load a .v5 script and we want to save another .v5, we need to make another H5Script*, since they will interfere!
	/**
	 *  constructor that loads a given script
	 */
	VScriptH5::VScriptH5(const string&scriptUrl, const string&name, int N, const RefPtr<VCreationPreferences>&VP)
	: VScriptH5(name, N, VP)
	{
		//const RefPtr<VObject> VObj = VObject::find(interpolatedObject);
		std::size_t pos = scriptUrl.rfind('/');
		if (pos == string::npos)
			pos = -1;
		//h5script.h5fileInfo = new H5Script::H5FileInfo();
		h5script.h5fileInfo->scriptUrl = scriptUrl;
		h5script.h5fileInfo->scriptName = scriptUrl.substr(pos+1, string::npos);
		h5script.h5fileInfo->scriptRootGroupName = "/"+h5script.h5fileInfo->scriptName;
		if (h5script.h5fileInfo == nullptr)
			Verbose(0)<< "H5File Info is null!";
	}

	bool	VScriptH5::update(VRequest&Context, double precision)
	{
		VTime time = getTime(Context);
		//printf("Time %lf\n", time());
		if (hasChanged( inTakeSnapshot, Context))
		{
			Verbose(10) << "VScriptH5::update() - Take Snapshot Action!";
			VTime time = getTime(Context);
			printf("Time %lf, max time: %lf, max steps: %lf\n", time(), time.Tmax(), time.maxTsteps());
			snapshots->doTakeSnapshot(Context.PoolName(), time);
			Verbose(10)<< " Now the root group hid is "<< h5script.h5fileInfo->group_id;
		}
		if (hasChanged (inSnapAndInterpolate, Context)) {
			Verbose(10) << "VScriptH5::update() - SnapAndInterpolate Action!";
			snapAndInterpolate->snapshot();
		}
		return true; 
	}

	VScriptH5::~VScriptH5()
	{
		//printf("Destructor of VSCriptH5 starting...\n");
		releaseResources();
		delete snapAndInterpolate;
		delete snapshots;
		//printf("Destructor of VSCriptH5 done!\n");
	}

	/**
	* loads the object with given object name from given file and object creation url
	* objName: the name of the object to load
	* objCreationUrl: the name of the creator
	* scriptPathUrl: the path of the script
	*/
	void loadObjFromFile(string& objName, string& objCreationUrl, string& scriptPathUrl){
		/* from ASCII Vscript parser
		 * namedload: IDENTIFIER '@' IDENTIFIER */
		/*VScriptParser*VSP = (VScriptParser*)VScriptParseParameter;

		VScriptParser::InputUrls_t::const_iterator id_url = VSP->InputUrls.find( objName ); 
		if (id_url != VSP->InputUrls.end() )
		{
			objCreationUrl = id_url->second;
		}*/
		if (objCreationUrl == scriptPathUrl)
		{
			printf("Recursive load [%s] --> [%s] detected, will be ignored,\n\n ** CHECK SCRIPT [%s] FOR ERRORS! **\n\n", objCreationUrl.c_str(), scriptPathUrl.c_str(), scriptPathUrl.c_str() );
		}
		else
		{
			printf("Load file [%s] from [%s]\n", objCreationUrl.c_str(), scriptPathUrl.c_str() );
			// TODO: include name of current vish script
			RefPtr<VObject> LoadedObject = VLoader::LoadWithWarning("Loaded from Vish Script", objCreationUrl, objName);
			if (!LoadedObject)
			{
				printf("   No object loaded from [%s] (Script [%s])\n", objCreationUrl.c_str(), scriptPathUrl.c_str() );
			}
		}
	}

	/**
	 * overrided function that loads a HDF5 format Vish script
	 */
	void VScriptH5::loadH5(){
#if RETRO_BDB_VOL
		if (Vscript_withRBDB){
			assert(H5RBDB_txn_begin(Vscript_fapl) ==0 );
			//assert(H5RBDB_as_of_snapshot(Vscript_fapl, 2) == 0);
		}
#endif
		h5script.currentlyLoading = true;

		//hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
		//H5Pset_fclose_degree(fapl, H5F_CLOSE_SEMI);

		h5script.h5fileInfo->file_id = H5Fopen(h5script.h5fileInfo->scriptUrl.c_str(), H5F_ACC_RDWR, Vscript_fapl);
		//h5script.h5fileInfo->file_id = H5Fopen(h5script.h5fileInfo->scriptUrl.c_str(), H5F_ACC_RDWR, fapl);
		// open group containing current script (/ScriptName) and iterate over all sub-groups
		Verbose(10)<< " Opening group "<< h5script.h5fileInfo->scriptRootGroupName << " from script url " << h5script.h5fileInfo->scriptUrl;
		h5script.h5fileInfo->group_id = H5Gopen2(h5script.h5fileInfo->file_id, (h5script.h5fileInfo->scriptRootGroupName).c_str(), H5P_DEFAULT);
#if TRAVERSE_LINKS_BY_CRT_ORDER
		H5Literate(h5script.h5fileInfo->group_id, H5_INDEX_CRT_ORDER, H5_ITER_INC, NULL, parse_group, NULL);
#else
		H5Literate(h5script.h5fileInfo->group_id, H5_INDEX_NAME, H5_ITER_INC, NULL, parse_group, NULL);
#endif
		//if (Vscript_withRBDB)
		//	H5RBDB_txn_commit(Vscript_fapl);
		// establish connections
		for (pair<H5Script::MemberID, H5Script::MemberID> connection : h5script.connections){
			addConnection(connection.first, connection.second);
		}
		// setup objects
		for (string obj : h5script.objects){
			//setupObject(obj);
		}
#if RETRO_BDB_VOL
		if (Vscript_withRBDB){
			//H5RBDB_retro_free(Vscript_fapl);
			//H5RBDB_txn_commit(Vscript_fapl);
		}
#endif
		h5script.loaded = true;
		h5script.currentlyLoading = false;
	}

	/**
	 * parses an attribute. It is called by H5Aiterate for all attributes within the timeSnap group!
	 */
	herr_t parse_attr_timesnap(hid_t loc_id, const char *name, const H5A_info_t *ainfo, void *opdata){
		auto timeSnapMap = static_cast<map<double, TimeSnap>* > (opdata);
		double time;
		unsigned snap;
		snap = stoul(string(name));
		Verbose(45)<< " Looking for attribute " << name;
		getAttr(loc_id, name, H5T_NATIVE_DOUBLE, &time);
		Verbose(45)<< " Read from timeSnapH5: time: "<<time << ", snap: "<<snap;
		TimeSnap ts(time, snap);
		(*timeSnapMap)[time] = ts;
		return 0;
	}


	/**
	 * parses a group, it is called by H5Literate for all sub-groups found in a given group
	 * loc_id: the location id of the current group
	 * name: the name of the current group
	 * linfo: the type of the link (soft, hard, etc.)
	 * opdata: the parameters
	 */
	herr_t parse_group(hid_t loc_id, const char *name, const H5L_info_t *linfo, void *opdata){
		hid_t group_id;
		herr_t ret;
		int result;
		int objType;
		//group_iterate_t group_iterate;
		char* linkval_buff;
		char* obj_name;
		string objName, localParam, localParams, objCreator, linkName, linkValue;
		map<string, bool> localParamsMap;
		size_t newlinePos, curPos, arrowPos, slashPos;
		attr_data_t* attr_data;
		H5Script::MemberID fromMember, toMember;
		string fromObj, toObj, fromMemberStr, toMemberStr, objNameInFile, creationUrl, scriptPathUrl;

		// parse the special group for timeSnap and load timeSnap entries!
		if (strcmp(name, GROUP_TIME_SNAP ) == 0){
			Verbose(0)<< " group is for timeSnap!!";
			herr_t ret;
			hid_t group_time_snap_id;
			group_time_snap_id = H5Gopen2(loc_id, name, H5P_DEFAULT);
			ret = H5Aiterate2(group_time_snap_id, H5_INDEX_NAME, H5_ITER_INC, NULL, parse_attr_timesnap, static_cast<void*>(snapshots->getTimeSnapMap()));
			assert(ret==0);
			return 0;
		}
		// retrieve file id and level from the opdata
		//group_iterate.file_id = ((group_iterate_t*)opdata)->file_id;
		//group_iterate.level = ((group_iterate_t*)opdata)->level;
		switch (linfo->type){
			case H5L_TYPE_SOFT:
				// TODO: print link target!
				//printf("VAL SIZE~ %d\n", linfo->u.val_size);
				//linkval_buff = static_cast<char*>(malloc ((linfo->u.val_size +8) * sizeof(char)));
				//H5Lget_val(loc_id, name, linkval_buff, linfo->u.val_size+8, H5P_DEFAULT );
				//TODO: get link value size!! Must implement in Berkeley DB before using it here!
				linkval_buff = static_cast<char*>(malloc (200 * sizeof(char)));
				H5Lget_val(loc_id, name, linkval_buff, 200, H5P_DEFAULT );
				#if H5_LOAD_DEBUG
				printf("Link name: %s, target: %s\n", name, linkval_buff);
				#endif
				linkName = string(name);
				linkValue = string(linkval_buff);
				fromObj = string(static_cast<char*>(opdata));
				arrowPos = linkName.find("=>");
				fromMemberStr = linkName.substr(0, arrowPos);
				slashPos = linkValue.rfind("/");
				toObj = linkValue.substr(slashPos+1, string::npos);
				toMemberStr = linkName.substr(arrowPos+2, string::npos);
				#if H5_LOAD_DEBUG
				printf("from obj: %s, from member: %s, to obj: %s, to member: %s\n", fromObj.c_str(), fromMemberStr.c_str(), toObj.c_str(), toMemberStr.c_str());
				#endif
				fromMember.obj = fromObj;
				fromMember.member = fromMemberStr;
				toMember.obj = toObj;
				toMember.member = toMemberStr;
				// establish all connections at the end! If we do here, some Objects might not exist yet!
				h5script.connections.push_back(make_pair(fromMember, toMember));
				free (linkval_buff);
				break;
			case H5L_TYPE_HARD:
				#if H5_LOAD_DEBUG
				printf("group %s\n", name);
				#endif
				localParamsMap.clear();
				// check whether we need to unescape group name!
				htri_t exists;
				if ((exists = H5Aexists(loc_id, ATTR_GNAME_ESCAPED)) > 0){ // must unescape!
					objName = VScriptUnEscapeIdentifierH5(string(name));
				}
				else{
					objName = string(name);
				}
				group_id = H5Gopen2(loc_id, name, H5P_DEFAULT);
				//printf("********** Opened group %s, name in h5groups map: %s!\n", name, (h5script.h5fileInfo->scriptRootGroupName+"/"+string(name)).c_str());
				h5script.h5fileInfo->H5groups[h5script.h5fileInfo->scriptRootGroupName+"/"+string(name)] = group_id; // store the escaped group name, so that to access it via HDF5 if needed.
				// first get special attributes:
				// 1. __local_parameters__: all the local parameters of an object
				// 2. __object_type__ :		the type of the current object
				// 3. if object_type is DELETE_IF_EXISTS, then __creator__: the creator of that object
				// __local_parameters__
				localParams = getStringAttr(group_id, ATTR_LOCAL_PARAMS);
				if(localParams != ""){
					curPos = 0;
					while(( newlinePos = localParams.find("\n", curPos)) != string::npos){
						localParam = localParams.substr(curPos, newlinePos-curPos);
						//printf(" -----> Local param: %s\n", localParam.c_str());
						localParamsMap[localParam] = true;
						curPos = newlinePos+1;
					}
				}
				// __object_type__
				result = getAttr(group_id, ATTR_OBJECT_TYPE, H5T_NATIVE_INT, &objType);
				assert(result == 0);
				#if H5_LOAD_DEBUG
				printf(" object type: %d\n", objType);
				#endif
				if (objType == OBJ_TYPE_SNAPSHOT_GROUP){
					return 0;
				}
				// delete object if type is DELETE_IF_EXISTS
				if (objType == OBJ_TYPE_DELETE_IF_EXISTS){
					#if H5_LOAD_DEBUG
					printf("Delete if exists!\n");
					#endif
					deleteObject(objName);
					// __creator__
					objCreator = getStringAttr(group_id, ATTR_CREATOR);
					printf("Object Creator: %s\n", objCreator.c_str());
					if (objCreator != ""){
						addObjectCreator(objCreator, objName);
					}
				}
				// check if object must be loaded from file
				objNameInFile = getStringAttr(group_id, ATTR_FILE_LOAD_OBJ_NAME);
				if(objNameInFile != ""){
					creationUrl = getStringAttr(group_id, ATTR_FILE_LOAD_CREATION_URL);
					scriptPathUrl = getStringAttr(group_id, ATTR_FILE_LOAD_SCRIPT_PATH_URL);
					loadObjFromFile(objNameInFile, creationUrl, scriptPathUrl);
				}
				// needed for parsing attribute information
				attr_data = new attr_data_t(objName, objType, objCreator, localParamsMap);
				/*
				* Get attribute info using iteration function.
				*/
				//ret = H5Aiterate2(group_id, H5_INDEX_CRT_ORDER, H5_ITER_INC, NULL, parse_attr, static_cast<void*>(attr_data));
				ret = H5Aiterate2(group_id, H5_INDEX_NAME, H5_ITER_INC, NULL, parse_attr, static_cast<void*>(attr_data));
				assert(ret >= 0);
				delete attr_data;
				// setup all objects at the end! If we do here, some Objects might not exist yet!
				h5script.objects.push_back(objName);
				// for now pass name as opdata. We need to create a new string and pass it, since if we do objName.c_str() it is a const char* and we cannot convert to void*
				obj_name = static_cast<char*>(malloc((objName.size()+1) * sizeof(char)));
				memset(obj_name, 0, objName.size()+1);
				memcpy(obj_name, objName.c_str(), objName.size());
				//printf("Test: obj_name: %s\n", obj_name);
#if TRAVERSE_LINKS_BY_CRT_ORDER
				H5Literate(group_id, H5_INDEX_CRT_ORDER, H5_ITER_INC, NULL, parse_group, static_cast<void*>(obj_name));
#else
				H5Literate(group_id, H5_INDEX_NAME, H5_ITER_INC, NULL, parse_group, static_cast<void*>(obj_name));
#endif
				// don't close opened groups right now! Close them at the end!
				//status = H5Gclose(group_id);
				//assert(status >= 0);
				free(obj_name);
				break;
			case H5L_TYPE_ERROR:
				printf("There was an error getting link type\n");
				break;
			case H5L_TYPE_EXTERNAL:
				printf("Link type is external link, not yet implemented!\n");
				break;
			case H5L_TYPE_MAX:
				printf("Link type is max, not yet implemented!\n");
				break;
		}
		return 0;
	}

	/**
	 * Function callback for parsing attributes. It is called by H5Aiterate2()
	 * loc_id: the location id of the HDF5 object that we want to get its attributes
	 * name: the name of the current attribute
	 * ainfo: information about current attribute
	 * opdata: parameters, in this case we pass attr_data_t
	 * returns: 0 on success, negative number on failure
	 */
	herr_t
	parse_attr(hid_t loc_id, const char *name, const H5A_info_t *ainfo, void *opdata)
	{
		string obj_name, param_name, attrName;
		RefPtr<VValueBase> param_value;
		attr_data_t* attr_data;
		bool isLocal;

		attrName = string(name);
		// if attribute is one of the special attributes, do not parse it!
		if (attrName == ATTR_LOCAL_PARAMS || attrName == ATTR_LINE_FEEDS_TO_VTABS || attrName == ATTR_COMMENTS || attrName == ATTR_CREATOR || attrName == ATTR_OBJECT_TYPE ||
				attrName == ATTR_GNAME_ESCAPED || attrName == ATTR_FILE_LOAD_OBJ_NAME || attrName == ATTR_FILE_LOAD_CREATION_URL || attrName == ATTR_FILE_LOAD_SCRIPT_PATH_URL) {
			return 0;
		}
		#if H5_LOAD_DEBUG
		printf("Name : %s\n", name);
		#endif

		param_name = string(name);
		param_value = getParamFromAttrH5(loc_id, param_name);
		attr_data = static_cast<attr_data_t*>(opdata);
		obj_name = attr_data->obj_name;
		isLocal = attr_data->localParamsMap.find(param_name) == attr_data->localParamsMap.end()? false :  attr_data->localParamsMap[param_name] ;
		#if H5_LOAD_DEBUG
		std::cout << "=========== Param " << param_name << " is local?? " << isLocal << endl;
		#endif
		addParam(obj_name , param_name, param_value, isLocal, h5script);
		return 0;
	}

	SnapshotInfo::SnapshotInfo() : interpolatedObjects{}, h5info(nullptr), timeSnapMap(nullptr){}
	SnapshotInfo::SnapshotInfo(map<RefPtr<VObject>, list<RefPtr<VParameter> >> objects, H5Script::H5FileInfo* h5info, map<double,TimeSnap>* timeSnap) : interpolatedObjects(objects),
	h5info(h5info),
	timeSnapMap(timeSnap){
		for (const auto & obj : interpolatedObjects){
			Verbose(10)<< "^^^^^^^^^^ interpolated obj: "<< obj.first->Name();
		}
	}

static	Ref<VCreator<VScriptH5> >
	MyCreator("Tools/VScriptH5", ObjectQuality::EXPERIMENTAL);

} // namespace Wizt
