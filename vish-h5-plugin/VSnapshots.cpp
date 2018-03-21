/*
 * VSnapAndInterpolate.cpp
 *
 *  Created on: Feb 8, 2017
 *      Author: dimos
 */

#include "VSnapshots.hpp"
#include "HDF5init.hpp"
#include "VObjectsToH5.hpp"
#include "sys/time.h"
#include <unistd.h>
#include "H5utils.hpp"

#define PRINT_LATENCIES 1
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


namespace Wizt {

using namespace std;

extern hid_t Vscript_fapl;
extern bool inSnapshotPlayer;

SnapAndInterpolate::SnapAndInterpolate(H5Script& h5script, unsigned timesteps, unsigned mScripts):
		h5script(h5script),
		timestepsTotal(timesteps),
		manualScriptsTotal(mScripts),
		manualScriptsCreated(0),
		timestepCurrent(0){}

SnapAndInterpolate::~SnapAndInterpolate(){}

void SnapAndInterpolate::addInterpolatedParam(RefPtr<VParameter> param){
	this->interpolatedParams.push_back(param);
}

const list<RefPtr<VParameter>>& SnapAndInterpolate::getInterpolatedParams(){
	return this->interpolatedParams;
}


bool SnapAndInterpolate::interpolate(RefPtr<VParameter>Vparam, unsigned paramNum, float relativeTime, Eagle::PhysicalSpace::point* paramDataLatest){
	// store the current values of the parameters we want to interpolate in paramData, so that to calculate next values based on these first values.
	struct VSI : ValueShadowIterator
	{
		RefPtr<VParameter> param;
		unsigned ParamNum;
		Eagle::PhysicalSpace::point * paramDataFirst;
		Eagle::PhysicalSpace::point * paramDataLatest;
		Eagle::PhysicalSpace::point paramDataCur [NUM_INTERPOLATED_PARAMS];
		float relativeTime;

		VSI(RefPtr<VParameter> Vparam, unsigned paramNum, Eagle::PhysicalSpace::point* paramDataFirst, Eagle::PhysicalSpace::point* paramDataLatest, float relativeTime):
			param(Vparam),
			ParamNum(paramNum),
			paramDataFirst(paramDataFirst),
			paramDataLatest(paramDataLatest),
			relativeTime(relativeTime) {}
		bool apply(const RefPtr<ValuePool>&ShadowPool, const RefPtr<VValueBase>&ShadowValue) override
		{
			assert( ShadowPool );
			assert( ShadowValue );
			string	value = ShadowValue->Text();
			// a complete parameter interpolation would take into account the parameter type.
			// However for the purpose of this experiment we only need to interpolate parameters of type Eagle::PhysicalSpace::point
			if (value.find('\n') == string::npos){
				if (RefPtr<VValue<Eagle::PhysicalSpace::point>> pointValue = ShadowValue){
					paramDataCur[ParamNum] = *pointValue;
					Eagle::PhysicalSpace::point interpolatedData;
					for (int i=0; i<paramDataFirst->size(); i++){
						interpolatedData[i] = (1-relativeTime) * paramDataFirst[ParamNum].get(i) + relativeTime * paramDataLatest[ParamNum].get(i);
						//printf("relative time: %f\nparam first[%d]: %lf\nparam latest[%d]: %lf\nInterpolated[%d]: %lf\n",
							//	relativeTime, i, paramDataFirst[ParamNum][i], i, paramDataLatest[ParamNum][i], i, interpolatedData[i]);
					}
					param->setValue(interpolatedData, ShadowPool, "", false, nullptr);
				}
			}
			return true;
		}
	}
	InterpolateVariableValueForAllValuePools(Vparam, paramNum, this->paramDataFirst, paramDataLatest, relativeTime);
	Vparam->iterateShadows(InterpolateVariableValueForAllValuePools);
	//Vparam->setValue(observer, VP, "", false, nullptr);
	return true;
}


// Native HDF5
SnapAndInterpolateNativeH5::SnapAndInterpolateNativeH5(H5Script& h5script, unsigned timesteps, unsigned mScripts): SnapAndInterpolate(h5script, timesteps, mScripts) {}

SnapAndInterpolateNativeH5::~SnapAndInterpolateNativeH5(){}

bool SnapAndInterpolateNativeH5::doTakeSnapshot(unsigned snap_id){
	INIT_COUNTING
	START_COUNTING
	string snapName = "__snap_"+to_string(snap_id);
	string groupName = this->h5script.h5fileInfo->scriptRootGroupName+"/"+snapName;
	//printf("Creating snapshot for script %s (root group: %s)\n", this->h5script.h5fileInfo->scriptName.c_str(), this->h5script.h5fileInfo->scriptRootGroupName.c_str());
	createVScriptExistingH5( this->h5script.h5fileInfo, groupName, true);

	assert(H5Fflush(this->h5script.h5fileInfo->file_id, H5F_SCOPE_LOCAL) == 0);
	STOP_COUNTING("__CREATE_SNAPSHOT_STATIC_NATIVE__%u", snap_id)
	return true;
}

bool SnapAndInterpolate::snapshot(){
	bool resultOK;
	// save the current values of the interpolated parameters!
	unsigned i=0;
	// store the current values of the parameters we want to interpolate in paramData, so that to calculate next values based on these first values.
	Eagle::PhysicalSpace::point paramDataLatest [NUM_INTERPOLATED_PARAMS];
	if (this->manualScriptsCreated >= this->manualScriptsTotal)
		return false;
	for(RefPtr<VParameter >Vparam : this->getInterpolatedParams()){
		if (Vparam){
			Verbose(45) << "Parameter ", Vparam->Name();
			struct VSI : ValueShadowIterator
			{
				int ParamNum;
				Eagle::PhysicalSpace::point* ParamData;
				VSI(int i, Eagle::PhysicalSpace::point* paramData): ParamNum(i), ParamData(paramData) {}
				bool apply(const RefPtr<ValuePool>&ShadowPool, const RefPtr<VValueBase>&ShadowValue) override
				{
					assert( ShadowPool );
					assert( ShadowValue );
					string	value = ShadowValue->Text();
					// a complete parameter interpolation would take into account the parameter type.
					// However for the purpose of this experiment we only need to interpolate parameters of type Eagle::PhysicalSpace::point
					if (value.find('\n') == string::npos){
						if (RefPtr<VValue<Eagle::PhysicalSpace::point>> pointValue = ShadowValue){
							ParamData[ParamNum] = *pointValue;
						}
					}
					return true;
				}
			}
			InterpolateVariableValueForAllValuePools(i, paramDataLatest);
			Vparam->iterateShadows(InterpolateVariableValueForAllValuePools);
			//Vparam->setValue(observer, VP, "", false, nullptr);
		}
		else {
			Verbose(0) << "Error while interpolating: Cannot find parameter " << Vparam->Name();
		}
		i++;
	}
	if(this->manualScriptsCreated == 0){ // This is the first script. Just save the current state and do not interpolate!
		doTakeSnapshot(this->manualScriptsCreated+1);
		// TODO: See what happens with the strange behavior when having delay between snapshot creations!!
		// It was the software RAID 0 from Interl Rapid Storage Technology! Never use it again!
		resultOK = true;
	}
	else if (this->manualScriptsCreated < this->manualScriptsTotal){
		// if its the second manual script we create, we must create one less snapshot because there was one created by the first manual snapshot creation.
		// eg: for 80 total timesteps and 5 manual scripts: first manual snapshot: 1. second manual snapshot: 2,3,4,...,20. third manual snapshot: 21-30.... fifth: 61-80.
		unsigned timesteps = this->manualScriptsCreated == 1?  (this->timestepsTotal / (this->manualScriptsTotal-1))-1 : this->timestepsTotal / (this->manualScriptsTotal-1);
		//printf("Timesteps total: %u\n", this->timestepsTotal);
		//printf("Timesteps for now: %u\n", timesteps);
		for (unsigned time=1; time<= timesteps; time++ ){
			this->timestepCurrent++;
			//printf("Interpolating timestep %u\n", time);
			float relativeTime = 1 - ((float)(timesteps - time ) / (float)timesteps);
			unsigned paramNum=0;
			for(RefPtr<VParameter> Vparam : this->getInterpolatedParams()){
				interpolate(Vparam, paramNum, relativeTime, paramDataLatest);
				paramNum++;
			}
			doTakeSnapshot(this->timestepCurrent+1);
		}
		resultOK = true;
	}
	for (int i=0; i<NUM_INTERPOLATED_PARAMS; i++){
		this->paramDataFirst[i] = paramDataLatest[i];
		//printf("%lf, %lf, %lf\n", paramDataLatest[i].x(), paramDataLatest[i].y(), paramDataLatest[i].z());
	}
	this->manualScriptsCreated++;
	return resultOK;
}


// BDB
SnapAndInterpolateBDB::SnapAndInterpolateBDB(H5Script& h5script, unsigned timesteps, unsigned mScripts): SnapAndInterpolate(h5script, timesteps, mScripts) {}

bool SnapAndInterpolateBDB::doTakeSnapshot(unsigned snap_id){
	INIT_COUNTING
	START_COUNTING
	unsigned snapID = H5RBDB_declare_snapshot(Vscript_fapl);
	STOP_COUNTING("__CREATE_SNAPSHOT_STATIC_BDB__%u", snapID)
	// there is one-to-one mapping from snapshots we take to time step, thus we don't need to know the snapshot id!
	return true;
}
SnapAndInterpolateBDB::~SnapAndInterpolateBDB(){}

// Snapshots
Snapshots::Snapshots(H5Script& h5script, const map<RefPtr<VObject>, list<RefPtr<VParameter> >> & Objs): h5script(h5script), interpolatedObjs(Objs){}
Snapshots::~Snapshots(){
	cout << "Destructor of Snapshots!" <<endl;
}

map<double,TimeSnap>* Snapshots::getTimeSnapMap(){
	return &timeSnapMap;
}

void Snapshots::storeTimeSnapH5(){
	htri_t exists;
	hid_t timeSnap_gid;
	string groupName;
	herr_t status;

	Verbose(0) <<" storing current time-snap map to H5";
	groupName = h5script.h5fileInfo->scriptRootGroupName+"/"+GROUP_TIME_SNAP;
	exists = H5Lexists(h5script.h5fileInfo->group_id, groupName.c_str(), H5P_DEFAULT);
	if (exists > 0){ // for now we do not need to support overwriting of time-snap group in H5.
		return;
	}
	else{
		Verbose(0)<< "Creating group "<< groupName;
		timeSnap_gid = H5Gcreate2(h5script.h5fileInfo->file_id, groupName.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		assert(timeSnap_gid >= 0);
		for (auto it = timeSnapMap.begin(); it != timeSnapMap.end(); it++){
			unsigned snap = it->second.getSnapshot();
			double time = it->first;
			addAttr(timeSnap_gid, to_string(snap).c_str(), H5T_NATIVE_DOUBLE, (void*) &time);
		}
		status = H5Gclose(timeSnap_gid);
		assert(status==0);
	}
}

const map<RefPtr<VObject>, list<RefPtr<VParameter> >> & Snapshots::getInterpolatedObjs(){
	return interpolatedObjs;
}

TimeSnap::TimeSnap(double time, unsigned snapID):
		time(time),
		snapCurrent(snapID),
		paramValsInitialized(false),
		prevSnapPersisted(nullptr),
		nextSnapPersisted(nullptr){}
TimeSnap::TimeSnap():TimeSnap(0, 0){}

unsigned TimeSnap::getSnapshot(){
	return snapCurrent;
}

void TimeSnap::setTime(double t){
	time = t;
}
double TimeSnap::getTime(){
	return time;
}

bool TimeSnap::addParamVal(string param, string val){
	paramVals[param] = val;
	return true;
}
string TimeSnap::getParamVal(string param){
	if (paramVals.find(param) == paramVals.end())
		return "";
	else
		return paramVals[param];
}
map<string, string>& TimeSnap::getParamVals(){
	return paramVals;
}

void TimeSnap::setPrevSnapPersisted(TimeSnap* snap){
	prevSnapPersisted = snap;
}
void TimeSnap::setNextSnapPersisted(TimeSnap* snap){
	nextSnapPersisted = snap;
}
TimeSnap* TimeSnap::getPrevSnapPersisted(){
	return prevSnapPersisted;
}
TimeSnap* TimeSnap::getNextSnapPersisted(){
	return nextSnapPersisted;
}
bool TimeSnap::isInitialized(){
	return paramValsInitialized;
}
void TimeSnap::setInitialized(){
	paramValsInitialized=true;
}
// updates the param values of the given ts, by getting values of all interpolated parameters
bool Snapshots::updateTimeSnapParams(TimeSnap&ts){
	unsigned paramNum=0;
	for(const auto obj : getInterpolatedObjs()){ // for each Vparam, store the current value to the time-snap map!
		RefPtr<VObject> VObj = obj.first;
		if (obj.first){
			const auto VParams  = obj.second;
			for (const RefPtr<VParameter> VParam : VParams){
				if (VParam){
					Verbose(10)<<"Iterating over shadow values of "<<VParam->Name();
					// get parameter value from specific snapshot!
					struct VSI : ValueShadowIterator {
						const RefPtr<VObject> VObj;
						RefPtr<VParameter> param;
						TimeSnap& ts;
						VSI(const RefPtr<VObject> VObj, RefPtr<VParameter> Vparam, TimeSnap& ts):
							VObj(VObj),
							param(Vparam),
							ts(ts){}
						bool apply(const RefPtr<ValuePool>&ShadowPool, const RefPtr<VValueBase>&ShadowValue) override
						{
							assert( ShadowPool );
							assert( ShadowValue );
							string	value = ShadowValue->Text();
							// a complete parameter interpolation would take into account the parameter type.
							// However for the purpose of this experiment we only need to interpolate parameters of type Eagle::PhysicalSpace::point
							Verbose(10) << "Found Value: " << value ;
							if (value.find('\n') == string::npos){
								if (RefPtr<VValue<Eagle::PhysicalSpace::point>> pointValue = ShadowValue){
									string paramNameFull = VObj->getSlotName(param)+"{"+ShadowPool->Name()+"}";
									ts.addParamVal(paramNameFull, value);
								}
							}
							return true;
						}
					}
					InterpolateVariableValueForAllValuePools(VObj, VParam, ts);
					VParam->iterateShadows(InterpolateVariableValueForAllValuePools);
				}
				else {
					Verbose(0) << "Error while updating: Cannot find parameter " << VParam->Name();
					return false;
				}
			}
		}
		else {
			Verbose(0) << "Error while updating: Cannot find object " << obj.first->Name();
			return false;
		}
		paramNum++;
	}
	return true;
}

// native H5
bool SnapshotsNativeH5::doTakeSnapshot(string contextName, VTime time){
	INIT_COUNTING
	// contextName is required for version 2.0! For now we can treat objects as global.
	START_COUNTING
	Verbose(45) << "Taking snapshot for context " << contextName << " at time " << time();
	string snapName = "__snap_"+to_string(manualScriptsCreated+1);
	string groupName = h5script.h5fileInfo->scriptRootGroupName+"/"+snapName;
	//printf("Creating snapshot for script %s (root group: %s)\n", this->h5script.h5fileInfo->scriptName.c_str(), this->h5script.h5fileInfo->scriptRootGroupName.c_str());
	bool ok = createVScriptExistingH5( this->h5script.h5fileInfo, groupName, true);
	if (!ok){
		Verbose(0) <<"SnapshotsNativeH5::doTakeSnapshot: Error creating snapshot!";
		return false;
	}
	// maybe don't take snapshot, there is already a snapshot for this time value.
	// store current values of interpoalted parameters!
	TimeSnap ts(time(), manualScriptsCreated+1);
	updateTimeSnapParams(ts);
	timeSnapMap[time()] = ts;
	manualScriptsCreated++;
	STOP_COUNTING("__CREATE_SNAPSHOT_DYNAMIC_NATIVE__%u", manualScriptsCreated)
	return true;
}

SnapshotsNativeH5::SnapshotsNativeH5(H5Script& h5script, const map<RefPtr<VObject>, list<RefPtr<VParameter> >> & VObjs):
		Snapshots(h5script, VObjs),
		manualScriptsCreated(0){}

SnapshotsNativeH5::~SnapshotsNativeH5(){}


// BDB
bool SnapshotsBDB::doTakeSnapshot(string contextName, VTime time){
	Verbose(10)<< "SnapshotsBDB::doTakeSnapshot! ";
	INIT_COUNTING
#if RETRO_BDB_VOL
		START_COUNTING
		unsigned snapID = H5RBDB_declare_snapshot(Vscript_fapl);
		STOP_COUNTING("ACTUAL TAKE SNAPSHOT!")
		START_COUNTING
		//printf("Declared snapshot %u\n", snapID);
		if (snapID == 0){
			return false;
		}
		TimeSnap ts(time(), snapID);
		//updateTimeSnapParams(ts);
		timeSnapMap[time()] = ts;
		hid_t gid = h5script.h5fileInfo->H5groups[h5script.h5fileInfo->scriptRootGroupName+"/Camera"];
		string paramVal = getParamFromAttrH5AsStr(gid, "headpoint{MonoViewer}");
		Verbose(0)<<"headpoint{Monoviewer} @"<< time() << ": "<< paramVal;
		/*if (time() == 1){
			hid_t gid = h5script.h5fileInfo->H5groups[h5script.h5fileInfo->scriptRootGroupName+"/Camera"];
			for (unsigned i=1; i<=5; i++){
				#if RETRO_BDB_VOL
				START_COUNTING
				assert(H5RBDB_as_of_snapshot(Vscript_fapl, i) == 0);
				STOP_COUNTING("------>> As_of")
				START_COUNTING
				string paramVal = getParamFromAttrH5AsStr(gid, "headpoint{MonoViewer}");
				STOP_COUNTING("--->> Get attr")
				Verbose(0)<<"headpoint{Monoviewer} @"<< i << ": "<< paramVal;
				START_COUNTING
				paramVal = getParamFromAttrH5AsStr(gid, "observer{MonoViewer}");
				STOP_COUNTING("--->> Get attr")
				Verbose(0)<<"observer{Monoviewer} @"<< i << ": "<< paramVal;
				START_COUNTING
				assert(H5RBDB_retro_free(Vscript_fapl) == 0);
				STOP_COUNTING("retro free")
				//H5RBDB_txn_commit(Vscript_fapl);
				//H5RBDB_txn_begin(Vscript_fapl);
				#endif
			}
		}*/
		STOP_COUNTING("__CREATE_SNAPSHOT_DYNAMIC_BDB__%u", snapID)
		return true;
#endif
	return false;
}
SnapshotsBDB::SnapshotsBDB(H5Script& h5script, const map<RefPtr<VObject>, list<RefPtr<VParameter> >> & VObjs): Snapshots(h5script, VObjs){}
SnapshotsBDB::~SnapshotsBDB(){}


}



