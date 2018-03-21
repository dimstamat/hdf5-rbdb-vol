/**
 * Saving the Vish network in an HDF5 file
 * Dimokritos Stamatakis
 * Brandeis University
 * April, 2016
 */
#include <ocean/plankton/VObject.hpp>
#include <memcore/stringutil.hpp>
#include <ocean/plankton/VLoader.hpp>
#include <ocean/vscript/VScriptInterface.hpp>
#include <ocean/vscript/VObjectSystemExecutor.hpp>
#include <ocean/plankton/VDataFlowGraph.hpp>
#include <vishapiversion.h>
#include <ctime>
#include <iostream>
#include <iomanip>

#include <memcore/PathOpen.hpp>
#include <memcore/Verbose.hpp>

#include <hdf5.h>

#include "VCreateScriptH5.hpp"
#include "VScriptH5.hpp"
#include "VScriptH5HID.hpp"
#include "HDF5init.hpp"
#include "H5utils.hpp"
#include "VObjectsToH5.hpp"

#define H5_SAVE_DEBUG 0
#define SAVE_PARAMS_AS_STRINGS 0
// no improvement when storing parameters at their own type, instead of all strings.
// Saving Analytic.vis takes 32ms when storing params at their own type, and 30ms when storing all as strings.

#define PRINT_LATENCIES 0

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

#include <sys/time.h>




struct X { ~X() { Verbose(0) << " Global destructor"; } };
static X XXX;


char* add_prefix(const char* src){
    const char *prefix = "__file_";
    char* dst;
    long unsigned int size = strlen(prefix) + strlen(src) + 1;
    dst = (char*) malloc(size*sizeof(char));
    strcpy(dst, prefix);
    strcat(dst, src);
    return dst;
}


namespace Wizt
{

extern hid_t Vscript_fapl;
extern bool Vscript_withRBDB;
extern enum SnapshotMethod smethod;

map<string, hid_t> VScriptH5_Groups;
pair<hid_t, string> VScriptH5_File;
bool VScriptH5_Saving = false;
bool VScriptH5_Saved = false;



	using namespace std;

#pragma message "TODO: Use a systematic approach for escaping and un-escaping characters here"

// used on saving
std::string VScriptEscapeIdentifierH5(const std::string&InternalObjectName)
{
	// certainly this is not the final solution on how to do things here...
	return subst( InternalObjectName, string("/"), string("_"));
}

// used on reading
std::string VScriptUnEscapeIdentifierH5(const std::string&ExternalObjectName)
{
	return subst(ExternalObjectName, string("_"), string("/"));
}


string	SaveObjectName(const string&s)
{
	return VScriptEscapeIdentifierH5(s);
}

namespace
{



struct	ParentConnection
{
	string	member,
		ParentName,
		ParentSlot;

	ParentConnection(const string&A, const string&B, const string&ParentSlotName)
	: member(A)
	, ParentName(B)
	, ParentSlot(ParentSlotName)
	{}
};

}

#define _MakeString(x) #x
#define MakeString(x) _MakeString(x)


#define IS_ATLEAST_GNUC_VERSION(Major, Minor) (defined(__GNUC__) && (__GNUC__ > Major || (__GNUC__ == Major && __GNUC_MINOR__ >= Minor)))

// we need to pass the ScriptRootGroupName and not use the one within h5info, since we might call this function to create a nativeH5 snapshot!
bool createVScriptExistingH5(H5Script::H5FileInfo* h5info, string& ScriptRootGroupName, bool snapshot){
	hid_t gcpl;
	int objType;
	VScriptH5_File = make_pair(h5info->file_id, h5info->scriptUrl);
	INIT_COUNTING

#if TRAVERSE_LINKS_BY_CRT_ORDER
	gcpl = H5Pcreate (H5P_GROUP_CREATE);
	assert(H5Pset_link_creation_order( gcpl, H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED ) >= 0) ;
#else
	gcpl = H5P_DEFAULT;
#endif

	AppVerbose("VScriptH5", 22) <<"== Creating VScript in existing H5 file. Now I see " << h5info->H5groups.size() <<" h5groups!";
	//status = H5Pset_local_heap_size_hint(gcpl, 100000 + 2);
	//assert(status >= 0);
	// create the group containing the groups representing the Vish objects of the specified script.
	START_COUNTING
	hid_t root_group_id;
	root_group_id = H5Gcreate2(h5info->file_id, ScriptRootGroupName.c_str(), H5P_DEFAULT, gcpl, H5P_DEFAULT);
	STOP_COUNTING("create group")
	assert(root_group_id >= 0);
	if (snapshot){ // add special attribute specifying that this group represents a snapshot
		objType = OBJ_TYPE_SNAPSHOT_GROUP;
		START_COUNTING
		addAttr(root_group_id, ATTR_OBJECT_TYPE, H5T_NATIVE_INT, &objType);
		STOP_COUNTING("add int attr")
	}
	Verbose(10)<<"Created script root group with ID: "<<root_group_id;

	std::size_t pos = h5info->scriptUrl.rfind('/');
	if (pos == string::npos)
		pos = -1;
	std::string ScriptName = h5info->scriptUrl.substr(pos+1, h5info->scriptUrl.length());
	Verbose(10) << "Creating script " << ScriptName << " in path " << h5info->scriptUrl << " with root group: " << ScriptRootGroupName;
	// the root group can be different than the one we started. We might create another Vscript in an existing HDF5 file!
	Verbose(10) << "Inserting group "<< ScriptRootGroupName;
	bool inserted = h5info->H5groups.insert(pair<string, hid_t>(ScriptRootGroupName, root_group_id)).second;
	assert(inserted);
	/// Iterate over all objects available
	struct ObjectSaver : VManagedObjectIterator
	{
		H5Script::H5FileInfo* h5info;
		const string& ScriptName;
		string& ScriptRootGroupName;
		hid_t H5faplID;
		const bool& snapshot;

		typedef multimap<string, ParentConnection>	ParentConnections_t;
		ParentConnections_t				ParentConnections;

		//(h5info, Vscript_fapl, DirName(ScriptName), snapshot)
		ObjectSaver(H5Script::H5FileInfo* h5info, hid_t h5fapl_id, const string&theScriptName, string& scriptRootGroupName, const bool& snapshot)
		: h5info(h5info), ScriptName(theScriptName), ScriptRootGroupName(scriptRootGroupName), H5faplID(h5fapl_id), snapshot(snapshot)
		{}
		void	saveConnections()
		{
			hid_t group_id;
			herr_t link_status;
			INIT_COUNTING
			for(ParentConnections_t::const_iterator
					cit = ParentConnections.begin();
				cit != ParentConnections.end(); cit++)
			{
				if (cit->second.ParentSlot.empty()){
					//printf("%s.%s => %s\n", cit->first.c_str(), cit->second.member.c_str(), cit->second.ParentName.c_str());
					// make sure that the group is already created
					assert(h5info->H5groups.count(ScriptRootGroupName+"/"+(cit->first)) > 0);
					group_id = h5info->H5groups[ScriptRootGroupName+"/"+(cit->first)];
					//printf("Creating link %s", cit->second.member.c_str());
					START_COUNTING
					link_status = H5Lcreate_soft(getAbsoluteGroupName(cit->second.ParentName).c_str(), group_id, cit->second.member.c_str(), H5P_DEFAULT,H5P_DEFAULT);
					STOP_COUNTING("add link")
					assert(link_status >= 0);
					//outfile << cit->first << "." << cit->second.member << "=>" << cit->second.ParentName << endl;
				}
				else{
					// make sure that the group is already created
					assert(h5info->H5groups.count(ScriptRootGroupName+"/"+(cit->first)) > 0);
					group_id = h5info->H5groups[ScriptRootGroupName+"/"+(cit->first)];
					//printf("%s.%s => %s.%s\n", cit->first.c_str(), cit->second.member.c_str(), cit->second.ParentName.c_str(), cit->second.ParentSlot.c_str());
					START_COUNTING
					link_status = H5Lcreate_soft(getAbsoluteGroupName(cit->second.ParentName).c_str(), group_id, (cit->second.member+ "=>"+cit->second.ParentSlot).c_str(), H5P_DEFAULT,H5P_DEFAULT);
					STOP_COUNTING("add link")
					//outfile << cit->first << "." << cit->second.member << "=>" << cit->second.ParentName << "." << cit->second.ParentSlot << endl;
					assert(link_status >= 0);
				}
			}
		}

		string getAbsoluteGroupName(string group){
			return ScriptRootGroupName+"/"+group;
		}

		/**
		 * releases the HDF5 resources and asserts that everything is successful
		 * snapshot_only: if true, it closes only groups associated with the newly created snasphot
		 * 				  if false, it closes all groups!
		 */
		void releaseH5Resources(hid_t root_group_id, bool snapshot_only){
			herr_t status;
			INIT_COUNTING
			AppVerbose("VScriptH5", 22)<< "VCreateScriptH5: Releasing H5 groups!...";
			for(auto& group : h5info->H5groups){
				// TODO: find a better way of identifying a snapshot group! Maybe make the map <string, pair<hid_t, bool>>.
				if (group.second <=0 || (snapshot_only && group.first.find("/__snap_")== string::npos) )
					continue;
				AppVerbose("VScriptH5", 22)<< "Closing group "<< group.first;
				START_COUNTING
				status = H5Gclose(group.second);
				STOP_COUNTING("VScriptH5: close group")
				assert(status >= 0);
				// invalidate this group, so that to not close it!
				// This is more correct than removing it while iterating and faster than having to maintain two maps!
				group.second = -1;
			}
			if (!snapshot_only)
				status = H5Gclose(root_group_id);
			// flush on every snapshot creation to prevent the 'superblock dirty' error
			if(snapshot_only)
				status = H5Fflush(h5info->file_id, H5F_SCOPE_LOCAL);
			AppVerbose("VScriptH5", 22) <<"Done!";
#if RETRO_BDB_VOL
			if (Vscript_withRBDB) {
				//START_COUNTING
				//H5RBDB_txn_commit(H5faplID);
				//STOP_COUNTING("VCreateScriptH5: commit txn")
				//H5VL_terminate_vol_RBDB(H5faplID);
			}
#endif
		}

		bool apply(int priority,
			   const RefPtr<VManagedObject>&VMO,
			   const std::string&ModulePath) override
		{
			RefPtr<VObject> VObj = VMO;
			hid_t group_id, gcpl;
			int escaped;
			INIT_COUNTING

			// check the type of the object!! If it is VScriptH5, we don't need to save it! It will be created every time we load a v5 script
			if (VObj->type_key() == VScriptH5Type){
				return true;
			}
			/// If the object is loaded from a file, then load it here
			if (RefPtr<VLoaderInfo> VL = interface_cast<VLoaderInfo>(VMO) )
			{
			string	SystemExec;
				if (VObj)
				{
					if (RefPtr<Fiber::VObjectSystemExecutor> VSO = interface_cast<Fiber::VObjectSystemExecutor>(*VObj))
					{
						SystemExec = "@\"" + VSO->ExecutionCommand + "\"";
					}
				}

			string	CreationUrl = crop_prefix(VL->CreationURL, h5info->scriptUrl);
			//printf("########### ScriptPathName: %s, VL->CreationURL: %s\n", h5info->scriptUrl.c_str(), VL->CreationURL.c_str());

			/**  CreationURL is possibly the full path to the file and scriptPathName is the absolut path. crop_prefix
				stripes of the full path of the script and therefore we need to check for a leading slash. */
			if (CreationUrl != VL->CreationURL && CreationUrl.length()>0)
			{
				if (CreationUrl.back() == '/') // probably obsolete (werner&doeby 2016)
					CreationUrl.pop_back();

				if (CreationUrl.front() == '/')
					CreationUrl.erase(0,1);
			}
			//outfile << "\"" + VMO->Name() + "\""
			string ObjectName = VMO->Name();
			string	ObjectNameInFile = SaveObjectName( ObjectName );
			#if H5_SAVE_DEBUG
				printf("Dimos prints: Object %s is loaded from file!\n", VObj->Name().c_str());
			#endif
				/*
				outfile << "\n# Loading object of type: [" << VObj->CreatorName().c_str() << "]" << endl;
				outfile << "# DELETE IF EXISTS " << ObjectNameInFile << endl
					<< "<>" << ObjectNameInFile << endl
					<< "\"" +
					ObjectNameInFile
					+ "\""
					+ "@\"" << CreationUrl << "\""
					<< SystemExec
					<< endl;
				*/


#if TRAVERSE_LINKS_BY_CRT_ORDER
				gcpl = H5Pcreate (H5P_GROUP_CREATE);
				assert( H5Pset_link_creation_order( gcpl, H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED ) >= 0);
#else
				gcpl = H5P_DEFAULT;
#endif
				//status = H5Pset_local_heap_size_hint(gcpl, 100000 + 2);
				//assert(status >= 0);
				//save object in HDF5 as group under /ScriptName, and store its id to the H5groups map!
				START_COUNTING
				group_id = H5Gcreate2(h5info->file_id,  getAbsoluteGroupName(ObjectNameInFile).c_str(), H5P_DEFAULT, gcpl, H5P_DEFAULT);
				STOP_COUNTING("create group")
				// we have to check if the group name is escaped and if so, add an appropriate attribute to say so.
				if (ObjectName != ObjectNameInFile){
					escaped = 1;
					addAttr(group_id, ATTR_GNAME_ESCAPED, H5T_NATIVE_INT, &escaped);
				}
				#if H5_SAVE_DEBUG
				printf(" Saving (1)  %s\n", getAbsoluteGroupName(ObjectNameInFile).c_str());
				#endif
				bool inserted = h5info->H5groups.insert(pair<string, hid_t> (getAbsoluteGroupName(ObjectNameInFile), group_id)).second;
				assert(inserted);
				if (!snapshot) // only store its H5 group info if it is not a snapshot group!
					VObj->addInterface(new VScriptH5HID(group_id, getAbsoluteGroupName(ObjectNameInFile)));
				Verbose(10)<< " adding string attribute! Creator Name: " << VObj->CreatorName();
				START_COUNTING
				addStringAttr(group_id, ATTR_CREATOR, VObj->CreatorName().c_str());
				STOP_COUNTING("add str attr")
				int intval = OBJ_TYPE_DELETE_IF_EXISTS;
				START_COUNTING
				addAttr(group_id, ATTR_OBJECT_TYPE, H5T_NATIVE_INT, &intval);
				STOP_COUNTING("add int attr")
				START_COUNTING
				addStringAttr(group_id, ATTR_FILE_LOAD_OBJ_NAME, ObjectNameInFile.c_str());
				STOP_COUNTING("add str attr")
				START_COUNTING
				addStringAttr(group_id, ATTR_FILE_LOAD_CREATION_URL, CreationUrl.c_str());
				STOP_COUNTING("add str attr")
				START_COUNTING
				addStringAttr(group_id, ATTR_FILE_LOAD_SCRIPT_PATH_URL, h5info->scriptUrl.c_str());
				STOP_COUNTING("add str attr")
				//SavableObjects.insert( ObjectNameInFile );
				RememberObjectStatus(VObj, ObjectNameInFile);
				return true;
			}
			if (!VObj)
				return true;

			string ObjectName = VObj->Name();
			string	ObjectNameInFile = SaveObjectName( ObjectName );
			//printf("Type of VScriptH5: %s\n", typeid(VScriptH5).name());
			//SavableObjects.insert( ObjectNameInFileEscaped );
#if TRAVERSE_LINKS_BY_CRT_ORDER
			gcpl = H5Pcreate (H5P_GROUP_CREATE);
			assert( H5Pset_link_creation_order( gcpl, H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED ) >= 0);
#else
			gcpl = H5P_DEFAULT;
#endif
			//status = H5Pset_local_heap_size_hint(gcpl, 100000 + 2);
			//assert(status >= 0);
			//save object in HDF5 as group under /ScriptName, and store its id to the H5groups map!
			START_COUNTING
			group_id = H5Gcreate2(h5info->file_id,  getAbsoluteGroupName(ObjectNameInFile).c_str(), H5P_DEFAULT, gcpl, H5P_DEFAULT);
			STOP_COUNTING("create group")
			// we have to check if the group name is escaped and if so, add an appropriate attribute to say so.
			if (ObjectName != ObjectNameInFile){
				escaped = 1;
				addAttr(group_id, ATTR_GNAME_ESCAPED, H5T_NATIVE_INT, &escaped);
			}
			#if H5_SAVE_DEBUG
			printf(" Saving (2) %s\n", getAbsoluteGroupName(ObjectNameInFile).c_str());
			#endif
			bool inserted = h5info->H5groups.insert(pair<string, hid_t> (getAbsoluteGroupName(ObjectNameInFile), group_id)).second;
			assert(inserted);
			if (!snapshot) // only store its H5 group info if it is not a snapshot group!
				VObj->addInterface(new VScriptH5HID(group_id, getAbsoluteGroupName(ObjectNameInFile)));
			// print the object graph
			//printf("Object graph of %s\n", VObj->Name().c_str());
			//VDataFlowGraph(VObj).print();
			/*if ( strcmp( VObj->Name().c_str(), "Camera") == 0){
				printf("!!!!!! Graph of Camera object\n------------------------------\n");
				for (VDataFlowGraph::Edge e : VDataFlowGraph(VObj).Edges){
					printf("%s -> (%s) -> %s\n", e.Source->first->getName().c_str(), e.mySlot->getSlotName().c_str(), e.Sink->first->getName().c_str());
				}
			}*/

			//
			// Object creation
			//
			if (VObj->CreatorName().length() < 1)
			{
				/*
				outfile << "\n# Expecting procedural object (no creator): "
					<< ObjectNameInFileEscaped << endl;
				*/
				int intval = OBJ_TYPE_PROCEDURAL;
				START_COUNTING
				addAttr(group_id, ATTR_OBJECT_TYPE, H5T_NATIVE_INT, &intval);
				STOP_COUNTING("add int attr")
			}
			else
			{
				if (RefPtr<VScriptInterface> VI = interface_cast<VScriptInterface>(VObj) )
				{
					/*
					outfile << "\n# Expecting Implicitly created object: \n# "
						<< VObj->CreatorName().c_str() << " " << ObjectNameInFileEscaped << endl;
					*/
					//cout << " Implicitly created object!"<<endl;
					int intval = OBJ_TYPE_IMPLICITLY_CREATED;
					START_COUNTING
					addAttr(group_id, ATTR_OBJECT_TYPE, H5T_NATIVE_INT, &intval);
					STOP_COUNTING("add int attr")
				}
				else
				{
					/*
					outfile << endl
						<< "# DELETE IF EXISTS " << ObjectNameInFileEscaped << endl
						<< "<>" << ObjectNameInFileEscaped << endl;
					*/

					// add object type attribute in HDF5
					int intval = OBJ_TYPE_DELETE_IF_EXISTS;
					START_COUNTING
					addAttr(group_id, ATTR_OBJECT_TYPE, H5T_NATIVE_INT, &intval);
					STOP_COUNTING("add int attr")
					/*
					  Writing user info as double comments just before object creation,
					  since the reading parser expect it this way.
					  There is no way currently to modify the user info in a script
					  after creation, thus procedural and implicit objects cannot
					  have a user info set from a script.
					 */
					if (VObj->UserInfo.length()>0)
					{
						list<string>	Comments;
						string comments_str = "";
						split(VObj->UserInfo, Comments, '\n', false);
						for(list<string>::const_iterator it = Comments.begin();
							it != Comments.end(); it++)
						{
							comments_str+=*it;
							comments_str+="\n";
							/*
							outfile << "## " << ltrim(*it) << endl;
							*/
						}
						//TODO: write user info in HDF5
						START_COUNTING
						addStringAttr(group_id, ATTR_COMMENTS, comments_str.c_str());
						STOP_COUNTING("add str attr")
					}
					//printf("~~~~ ObjectNameInFileEscaped: %s, VObj->CreatorName(): %s\n", ObjectNameInFileEscaped.c_str(), VObj->CreatorName().c_str());
					//write creator name in HDF5
					#if H5_SAVE_DEBUG
					printf(")))))))) Writing attribute creator %s\n", ATTR_CREATOR);
					#endif
					START_COUNTING
					addStringAttr(group_id, ATTR_CREATOR, VObj->CreatorName().c_str());
					STOP_COUNTING("add str attr")
					/*
					outfile << VObj->CreatorName().c_str() << " " << ObjectNameInFileEscaped << endl;
					*/
				}
			}
			RememberObjectStatus(VObj, ObjectNameInFile);
			return true;
		}

		void	RememberObjectStatus(const RefPtr<VObject>&VObj, const string&ObjectNameInFile)
		{
			INIT_COUNTING
			//
			// Remember connections of this object
			//
			struct ConnectToParents : VObjectIterator
			{
				RefPtr<VObject>	    ChildObject;
				ParentConnections_t&ParentConnections;
				string	self;

				ConnectToParents(const RefPtr<VObject>&theChildObject, ParentConnections_t&theParentConnections, const string&selfname)
				: ChildObject( theChildObject )
				, ParentConnections(theParentConnections)
				, self(selfname)
				{}

				bool	apply(const RefPtr<VObject>&ParentObject,
						  const string&ChildSlotName,
						  const type_info&type)
				{
					if (ParentObject != ChildObject)
					{
					RefPtr<VSlot> FromChildToParent = ChildObject->getSlot(ChildSlotName);

					RefPtr<VParameter> ConnectionParameter = FromChildToParent->getParameter();
					RefPtr<VSlot>	ParentSlot = ParentObject->findOutputSlot( ConnectionParameter );

						if (!ParentSlot)
						{
							Verbose(0) << "could not find Parameter " << ConnectionParameter->Name() << " in object " << ParentObject->BaseName();

							ParentConnections.insert(
								ParentConnections_t::value_type( self,
												 ParentConnection(ChildSlotName,
														  SaveObjectName(ParentObject->Name() ),
														  "")
												)
								);
						}
						else
						{
							ParentConnections.insert(
								ParentConnections_t::value_type( self,
												 ParentConnection(ChildSlotName,
														  SaveObjectName(ParentObject->Name() ),
														  ParentSlot->getSlotName())
									)
								);
						}
					}
					return true;
				}
			}	CP(VObj, ParentConnections, ObjectNameInFile);

			VObj->iterateParents(CP);

			// remember the local parameters for an object, until we store all together as a single newline separated attribute when done for that object!
			// this optimizes the storage of the parameter type without requiring to query whether each parameter is local, and without storing an extra field for each parameter!
			list<string> localParams;

			//
			// Determine object parameter values
			//
			struct	Meep : VObjectInputIterator
			{
				H5Script::H5FileInfo* h5info;
				string ScriptRootGroupName;
				RefPtr<VObject>	    myCurrentSaveableObject;
				string	self;
				list<string>& localParams;
				INIT_COUNTING

				Meep(const RefPtr<VObject>&theCurrentSaveableObject, string rootGroupName, const string&selfname, H5Script::H5FileInfo* h5info, list<string>& localParams)
				: h5info(h5info)
				, ScriptRootGroupName(rootGroupName)
				, myCurrentSaveableObject( theCurrentSaveableObject )
				, self(selfname)
				, localParams(localParams)
				{}

				bool apply(const RefPtr<VSlot>&what, int expertLevel)
				{
					#if H5_SAVE_DEBUG
					printf("----------> Found parameter %s\n", what->getSlotName().c_str());
					#endif
					if (!what) 	  return true;
					if (!what->getParameter() ) return true;

					//
					// Values that come from a parent will be ignored
					//
					if (RefPtr<VObject> Parent = what->getSource() )
					{
						if (Parent != myCurrentSaveableObject)
							return true;
					}
					if (what->dontSave)
					{
						/*
						outfile << "# Note: " << self << "." << what->SlotName() << " is not saved." << endl;
						*/
						return true;
					}

					struct VSI : ValueShadowIterator
					{
						string	CurrentObjectName,
							CurrentSlotName, ScriptRootGroupName;
						map<string, hid_t>& H5groups;

						VSI(const string&ON, const string&SN, string ScriptRootGroupName, map<string, hid_t>& H5groups)
						: CurrentObjectName(ON)
						, CurrentSlotName(SN)
						, ScriptRootGroupName(ScriptRootGroupName)
						, H5groups(H5groups)
						{}

						bool apply(const RefPtr<ValuePool>&ShadowPool, const RefPtr<VValueBase>&ShadowValue) override
						{
							assert( ShadowPool );
							assert( ShadowValue );

							string	value = ShadowValue->Text();
							hid_t group_id;
							INIT_COUNTING
							#if H5_SAVE_DEBUG
								printf("&&&& Value of parameter %s: %s\n", CurrentSlotName.c_str(), value.c_str());
							#endif


							if (value.find('\n') == string::npos){
								// make sure the group is created!
								assert(H5groups.count(ScriptRootGroupName+"/"+CurrentObjectName)>0);
								group_id = H5groups[ScriptRootGroupName+"/"+CurrentObjectName];
								#if H5_SAVE_DEBUG
									printf("~~~~ CurrentObjectName: %s, current slot name: %s, shadowpool name: %s, value: %s\n", CurrentObjectName.c_str(), CurrentSlotName.c_str(), ShadowPool->Name().c_str(), value.c_str());
								#endif
								// Context! example: visibility{MonoViewer}= true
								//write slot name, shadowpool name and value in HDF5
								#if SAVE_PARAMS_AS_STRINGS
								printf(" #1: (Context!) Writing (as string) slot name, shadowpool name and value: %s= %s\n", (CurrentSlotName+"{"+SaveObjectName(ShadowPool->Name())+"}").c_str(), value.c_str());
								START_COUNTING
								addStringAttr(group_id, (CurrentSlotName+"{"+SaveObjectName(ShadowPool->Name())+"}").c_str(), value.c_str());
								STOP_COUNTING("add str attr")
								#else
								assert(storeParamToAttrH5(group_id, CurrentSlotName+"{"+SaveObjectName(ShadowPool->Name())+"}", ShadowValue));
								#endif
							}
							return true;
						}
					}
					WriteVariableValueForAllValuePools(self, what->SlotName(), ScriptRootGroupName, h5info->H5groups );

//				int	valuesWritten =
					what->getParameter()->iterateShadows( WriteVariableValueForAllValuePools );
//					if (valuesWritten>0)
//						outfile << "# This variable has " << valuesWritten << " values, its global value is:" << endl;


					{
					RefPtr<ValuePool> VP;
					string	value = what->getParameter()->getValueAsText(VP, "", false);
					hid_t group_id;
					INIT_COUNTING

					#if H5_SAVE_DEBUG
						printf("Looking for %s in H5groups...\n", self.c_str());
					#endif
					assert( h5info->H5groups.count(ScriptRootGroupName+"/"+self) >0);
					group_id = h5info->H5groups[ScriptRootGroupName+"/"+self];

					if (value.find('\n') != string::npos)
					{
						//
						// Note: could parse in the script file for this comment to detect
						//       if there should be some string conversion when reading.
						//
						//write that line feeds have been converted to vertical tabs for specific slot name in HDF5
						START_COUNTING
						addStringAttr(group_id, ATTR_LINE_FEEDS_TO_VTABS, what->SlotName().c_str());
						STOP_COUNTING("add str attr")
						/*
						outfile << "# " << self << "." << what->SlotName() << ": Line feeds have been converted to vertical tabs." << endl;
						*/
						value = subst(value, '\n', '\v');
					}

					if (what->isLocal()){
						/*
						outfile << "=>";
						*/
						//printf("%s is local!\n", self.c_str());
						//write that the specific object is local
						localParams.push_back(what->SlotName());
					}
					// No dot! example: nearity= 10000
					// write object, slot name in HDF5
					//RefPtr<VValue<int>> V = what->getParameter()->getValue(VP, "");
					//what->getParameter()->getValue(what->getParameter()->get, VP, "");
					#if SAVE_PARAMS_AS_STRINGS
					#if H5_SAVE_DEBUG
					printf(" #2: (Parameter, not property!) Writing (as type string) object, slot name and attribute: %s= %s\n", what->SlotName().c_str(), value.c_str());
					#endif
					START_COUNTING
					addStringAttr(group_id, what->SlotName().c_str(), value.c_str());
					STOP_COUNTING("add str attr")
					#else
					const RefPtr<VValueBase>&valueBase = what->getValueBase(VP);
					assert(storeParamToAttrH5(group_id, what->SlotName(), valueBase));
					#endif
					/*
					outfile << self << "." << what->SlotName() << "=\"" << value << "\"" << endl;
					*/
					}

					{
						struct	VM : ValueMap::iterator
						{
							string	Slotname, rootGroupName;
							map<string, hid_t>& H5groups;

							VM(string rootGroupName, map<string, hid_t>& H5groups)
							: rootGroupName(rootGroupName),
							  H5groups(H5groups)
							{}

							bool	apply(const string&name, const VValueBasePtr_t&Component) override
							{
								string objectName;
								string attributeName;
								int pos;
								hid_t group_id;
								INIT_COUNTING
								RefPtr<ValuePool> VP;

								pos = Slotname.find('.');
								attributeName = Slotname.substr(pos+1, Slotname.length());
								objectName = Slotname.substr(0, pos);
								assert(H5groups.count(rootGroupName+"/"+objectName) > 0);
								group_id = H5groups[rootGroupName+"/"+objectName];
								//write slot name, type name and component in HDF5

								#if SAVE_PARAMS_AS_STRINGS
								#if H5_SAVE_DEBUG
								printf(" #3: Writing property (as string) slot name, type name and component: %s= %s\n", (attributeName+""+name+"("+Typename(Component->getType())+")").c_str(), Component->Text().c_str());
								#endif
								START_COUNTING
								addStringAttr(group_id, (attributeName+""+name+"("+Typename(Component->getType())+")").c_str(), Component->Text().c_str());
								STOP_COUNTING("add str attr");
								#else
								assert(storeParamToAttrH5(group_id, attributeName+""+name+"("+Typename(Component->getType())+")", Component));
								#endif
								/*
								(*outfile) << Slotname << Typename(Component->getType()) << "(" << name << ")=\"" << Component->Text() << "\"" << endl;
								*/
								return true;
							}

						}	PropIt(ScriptRootGroupName, h5info->H5groups);

						PropIt.Slotname = self + "." + what->SlotName() + ".";
						what->getParameter()->iterateProperties(PropIt);
					}
					return true;
				}

			} Meep(VObj, ScriptRootGroupName, ObjectNameInFile, h5info, localParams);

			// do not save VScriptH5!
			if (VObj->type_key() == VScriptH5Type)
				return;
			#if H5_SAVE_DEBUG
			printf("Starting iterating parameters for %s\n", (ScriptRootGroupName+"/"+SaveObjectName(VObj->Name())).c_str());
			#endif
			localParams.clear();
			VObj->iterateParameters(MAX_EXPERT_LEVEL, Meep);
			string localParamsAttr = "";
			for (string param : localParams){
				localParamsAttr+=param+"\n";
				#if H5_SAVE_DEBUG
				printf("!!! %s is local !!!\n", param.c_str());
				#endif
			}
			if (localParamsAttr != ""){
				START_COUNTING
				addStringAttr(h5info->H5groups[ScriptRootGroupName+"/"+SaveObjectName(VObj->Name())], ATTR_LOCAL_PARAMS, localParamsAttr.c_str());
				STOP_COUNTING("add str attr")
			}
			#if H5_SAVE_DEBUG
			printf("Finished iterating parameters for %s\n", (ScriptRootGroupName+"/"+SaveObjectName(VObj->Name())).c_str());
			#endif
		}
	};
	ObjectSaver	OS(h5info, Vscript_fapl, DirName(ScriptName), ScriptRootGroupName, snapshot);
	VObject::traverse(OS);

	/*
		outfile << "\n#\n# Setup Objects\n#\n";
	*/
	//OS.setupObjects();

	/*
	outfile << "\n#\n# Establish Connections\n#\n";
	*/
	OS.saveConnections();

	/*
	outfile << endl << endl;
	*/

	// close all groups we used for the current snapshot.
	// if we leave open we get the super_dirty error when closing HDF5 file.
	if (snapshot)
		OS.releaseH5Resources(root_group_id, true);
	// set the root group_id to be the one we just created only when we create a regular script, and not NativeH5 snapshot.
	// NativeH5 snapshots should not edit the script root group!
	if (!snapshot)
		h5info->group_id = root_group_id;

	VScriptH5_Saving = false;
	VScriptH5_Saved = true;
	//printf("!!== Created VScript in existing H5 file. Now I see %ld h5groups!\n", h5info->H5groups.size());
	return true;
}

bool	createVScriptH5(const std::string&ScriptPathName, H5Script::H5FileInfo* h5info)
{
	//TODO: Save vish version and vish version number in HDF5
	// " -VishVersion=" MakeString(VISH_API_VERSION) " -VishVersionNumber=" << VISH_API_VERSION_NUMBER
	//outfile << "# VERBOSE"  << endl;
	//std::size_t pos = ScriptPathName.rfind('.');
	//std::string H5ScriptPathName = ScriptPathName.substr(0, pos);

	if (h5info == nullptr){
		Verbose(0)<< "ERROR in createVScriptH5: h5info is null!";
		return false;
	}
	VScriptH5_Saving = true;
	//std::size_t pos = ScriptPathName.rfind('/');
	//if (pos == string::npos)
		//pos = -1;
	//std::string ScriptName = ScriptPathName.substr(pos+1, ScriptPathName.length());
	//printf("Creating script %s in path %s\n", ScriptName.c_str(), ScriptPathName.c_str());

	// For single session: We need to initialize whatever the VScriptH5 would initialize when loading a .v5 script!
	//const RefPtr<VObject> VObj = VObject::find(interpolatedObject);
	std::size_t pos = ScriptPathName.rfind('/');
	if (pos == string::npos)
		pos = -1;
	h5info->scriptUrl = ScriptPathName;
	h5info->scriptName = ScriptPathName.substr(pos+1, string::npos);
	h5info->scriptRootGroupName = "/"+h5info->scriptName;

#if RETRO_BDB_VOL
	if (Vscript_withRBDB)
		H5RBDB_txn_begin(Vscript_fapl);
#endif
	h5info->file_id = H5Fcreate(h5info->scriptUrl.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, Vscript_fapl);
	assert(h5info->file_id >= 0);
	return createVScriptExistingH5(h5info, h5info->scriptRootGroupName, false);
}

} // namespace Wizt

