/*
 * VSnapshotPlayer.cpp
 *
 *  Created on: Oct 27, 2016
 *      Author: dimos
 */

#include <ocean/plankton/VCreator.hpp>
#include <ocean/plankton/VModules.hpp>


#include "VSnapshotPlayer.hpp"
#include "VObjectsToH5.hpp"
#include "VCreateScriptH5.hpp"
#include "VSnapshots.hpp"
#include "VObjectsToH5.hpp"

#include <iostream>
#include <hdf5.h>
#include "HDF5init.hpp"

using namespace std;

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

extern hid_t Vscript_fapl;
extern SnapshotMethod smethod;
bool inSnapshotPlayer = false;

VSnapshotPlayer::VSnapshotPlayer(const string&name, int N, const RefPtr<VCreationPreferences>&VP)
        : VRenderObject(name, N, VP),
		  testIn(this, "myinput", 42),
		  snapInfo(this, "Snapshots info", SnapshotInfo()),
		  dynamicInterpolation(this, "Dynamic Interpolation", true){
	inSnapshotPlayer = true;
	if (smethod == NativeH5)
		player = new VSnapshotPlayerNativeH5();
	else if(smethod == RBDB)
		player = new VSnapshotPlayerBDB();
}

VSnapshotPlayer::~VSnapshotPlayer(){
	inSnapshotPlayer = false;
	delete player;
}

bool VSnapshotPlayer::environment_render(VRenderContext& cntx) const {
	return true;
}

bool VSnapshotPlayer::update(VRequest&Context, double precision){
	SnapshotInfo si;
	bool dynInt;
	snapInfo <<Context >> si;
	dynamicInterpolation << Context >> dynInt;
	VTime time = getTime(Context);
	if (si.interpolatedObjects.empty())
		return true;
	if(dynInt)
		Verbose(45)<< " Dynamic interpolation!";
	else
		Verbose(45) <<" Static interpolation!";
	return (dynInt?  player->displayDynamic(si, time) : player->displayStatic(si, time) );
}

bool VSnapshotPlayer::isDynamicInterpolation() {
	//bool dynInt;
	//dynInt << dynamicInterpolation;
	return false;
}

const unsigned VSnapshotPlayerNativeH5::getSnapshotFromTime(double time){
	// for now we have 1-1 mapping from time to snapshot id
	// TODO: must be max timesteps instead of 80.0!
	return (unsigned) (time == 0? 1 :  (time*(double) 80.0) );
}

const unsigned VSnapshotPlayerBDB::getSnapshotFromTime(double time){
	// for now we have 1-1 mapping from time to snapshot id
	// TODO: must be max timesteps instead of 80.0!
	return (unsigned) (time == 0? 1 :  (time*(double) 80.0) );
}

hid_t VSnapshotPlayerNativeH5::openSnapshot(const unsigned& snapId, hid_t root_group_id){
	hid_t snapGroup;
	snapGroup = H5Gopen2(root_group_id, ("__snap_"+to_string(snapId)).c_str(), H5P_DEFAULT);
	return snapGroup;
}

herr_t VSnapshotPlayerNativeH5::closeSnapshot(hid_t snap_group_id){
	return H5Gclose(snap_group_id);
}
// VSnapshotPlayerNativeH5
VSnapshotPlayerNativeH5::VSnapshotPlayerNativeH5(){}


VSnapshotPlayerBase::~VSnapshotPlayerBase(){}

bool VSnapshotPlayerNativeH5::displayStatic(SnapshotInfo& si, VTime& time){
	INIT_COUNTING
	Verbose(45) << " VSnapshotPlayerNativeH5::displayStatic!";
	if (si.h5info == nullptr){
		Verbose(0)<<" Error: h5info is null";
		return true;
	}
	Verbose(45)<< "Loading from snapshot " << getSnapshotFromTime(time()) << "! Max time steps: "<< time.Tstep(time());
	START_COUNTING
	if((si.h5info->currentSnapshotNativeGroup = openSnapshot(getSnapshotFromTime(time()), si.h5info->group_id)) <=0 )
		return true;
	TimeSnap dummy;
	getParamValuesFromH5Group(si, dummy, si.h5info->currentSnapshotNativeGroup, time(), true, false, true);
	assert(closeSnapshot(si.h5info->currentSnapshotNativeGroup) == 0);
	STOP_COUNTING("__LOAD_SNAPSHOT_STATIC_NATIVE__%u", getSnapshotFromTime(time()))
	return true;
}

bool VSnapshotPlayerBDB::displayStatic(SnapshotInfo& si, VTime& time){
	TimeSnap dummy;
	INIT_COUNTING
	Verbose(10) << "Loading from snapshot " << getSnapshotFromTime(time());
	START_COUNTING
	assert(H5RBDB_as_of_snapshot(Vscript_fapl, getSnapshotFromTime(time())) == 0);
	getParamValuesFromH5Group(si, dummy, si.h5info->group_id, time(), true, false, false);
	H5RBDB_retro_free(Vscript_fapl);
	STOP_COUNTING("__LOAD_SNAPSHOT_STATIC_BDB_%u", getSnapshotFromTime(time()));
	return true;
}

// displays a frame by checking if it should load from a snapshot, interpolate, or get values directly from time-snap map
bool VSnapshotPlayerBase::displayDynamic(SnapshotInfo& si, VTime& time){
	Verbose(45) << " VSnapshotPlayerBase displayDynamic!";
	if (si.h5info == nullptr)
		return true;
	if (si.timeSnapMap == nullptr){
		Verbose(0) << "displayDynamic: time SnapID map is null!";
		return true;
	}
	double timeCurrent, timePrev, timeNext;
	timeCurrent = time();
	TimeSnap timeSnapCurrent, timeSnapPrev, timeSnapNext;
	if (si.timeSnapMap->find(timeCurrent) == si.timeSnapMap->end()){ // we didn't find snapshot corresponding to this time step! Do dynamic interpolation!
		timeSnapCurrent.setTime(timeCurrent);
		Verbose(45)<< " Snapshot for time " << timeCurrent<< " does not exist. Must dynamically interpolate!";
		// find previous and next time values!
		map<double, TimeSnap>::iterator timeSnapIter = si.timeSnapMap->lower_bound(timeCurrent);
		// next time step
		timeNext = timeSnapIter->first;
		timeSnapNext = timeSnapIter->second;
		Verbose(45)<< "! Found in time-snap: timeNext: "<< timeNext<< ", timeSnapNext { time: "<< timeSnapNext.getTime() << ", snap: "<< timeSnapNext.getSnapshot() << ", observer{MonoViewer}: " << timeSnapNext.getParamVal("observer{MonoViewer}") << "}";
		timeSnapIter--;
		// previous time step
		timePrev = timeSnapIter->first;
		timeSnapPrev = timeSnapIter->second;
		Verbose(45)<< "! Found in time-snap: timePrev: "<< timePrev<< ", timeSnapPrev { time: "<< timeSnapPrev.getTime() << ", snap: "<< timeSnapPrev.getSnapshot() << ", observer{MonoViewer}: " << timeSnapNext.getParamVal("observer{MonoViewer}") <<"}";
		if (si.h5info == nullptr){
			Verbose(0) << "displayDynamic: Error! H5 file info is null!";
			return true;
		}
		unsigned snapshotPrev = timeSnapPrev.getSnapshot();
		if (snapshotPrev == 0) { // we don't have a valid snapshot for the previous TimeSnap entry! This means it is interpolated!
			Verbose(45) << "Previous TimeSnap entry is interpolated!";
			// get prev known persisted entry and store it to current entry's prev persisted entry!
			timeSnapCurrent.setPrevSnapPersisted(timeSnapPrev.getPrevSnapPersisted());
		}
		else { // we have a valid snapshot for this entry!
			Verbose(45) << "Previous TimeSnap entry has a snapshot!";
			getParamValuesFromSnapshot(si, timeSnapCurrent, timePrev, true, false);
		}
		unsigned snapshotNext = timeSnapNext.getSnapshot();
		if (snapshotNext == 0) { // we don't have a valid snapshot for the next TimeSnap entry! This means it is interpolated!
			Verbose(45) << "Next TimeSnap entry is interpolated!";
			// get next known persisted entry and store it to current entry's next persisted entry!
			timeSnapCurrent.setNextSnapPersisted(timeSnapNext.getNextSnapPersisted());
		}
		else { // we have a valid snapshot for this entry!
			Verbose(45) << "Next TimeSnap entry has a snapshot!";
			getParamValuesFromSnapshot(si, timeSnapCurrent, timeNext, false, false);
		}
		// now we are ready to interpolate for each time step from prevNext to timeNext!
		double timeIncrement = 0.0125;
		interpolateDynamic(si, timeSnapCurrent, timeSnapPrev, timeSnapNext, timeIncrement);
		//cout << " (END)! prev snap persisted: " << (*si.timeSnapMap)[timeCurrent].getPrevSnapPersisted() <<endl;
		//cout << " (END)! next snap persisted: " << (*si.timeSnapMap)[timeCurrent].getNextSnapPersisted() <<endl;
	}
	else{ // there is a TimeSnap entry for this time step!
		timeSnapCurrent = (*si.timeSnapMap)[timeCurrent];
		Verbose(45)<< " displayDynamic: Found time-snap entry for time " << timeCurrent;
		if (timeSnapCurrent.getSnapshot() == 0){ // current entry is not persisted (no corresponding snapshot)
			Verbose(45)<< " displayDynamic: current time-snap entry is interpolated!";
			setParamValuesFromMap(si, timeSnapCurrent);
		}
		else {
			Verbose(45) << "displayDynamic: Found Snapshot ID for time "<< timeCurrent <<": " << timeSnapCurrent.getSnapshot();
			getParamValuesFromSnapshot(si, timeSnapCurrent, timeCurrent, false, true);
		}
	}
	return true;
}

// sets interpolated parameter values from the time-snap map without reading from disk.
bool VSnapshotPlayerBase::setParamValuesFromMap(SnapshotInfo& si, TimeSnap& timeSnap){
	unsigned paramNum=0;
	for (const auto obj : si.interpolatedObjects){
		for(RefPtr<VParameter > VParam : obj.second){ // for each Vparam, set the value as the one we got from the corresponding snapshot of current time!
			if (VParam){
				bool found = false;
				struct VSI : ValueShadowIterator {
					SnapshotInfo& si;
					TimeSnap& ts;
					RefPtr<VObject>VObj;
					RefPtr<VParameter> param;
					bool& found;
					VSI(SnapshotInfo& si, TimeSnap& ts, RefPtr<VObject>VObj, RefPtr<VParameter>Vparam, bool& found):
						si(si),
						ts(ts),
						VObj(VObj),
						param(Vparam),
						found(found){}
					bool apply(const RefPtr<ValuePool>&ShadowPool, const RefPtr<VValueBase>&ShadowValue) override
					{
						assert( ShadowPool );
						assert( ShadowValue );
						string	value = ShadowValue->Text();
						// a complete parameter interpolation would take into account the parameter type.
						// However for the purpose of this experiment we only need to interpolate parameters of type Eagle::PhysicalSpace::point
						if (value.find('\n') == string::npos){
							found = true;
							//printf("Iterating over shadows...\nShadow pool: %s\nShadow value: %s\n",ShadowPool->Name().c_str(), ShadowValue->Text().c_str());
							if (RefPtr<VValue<Eagle::PhysicalSpace::point>> pointValue = ShadowValue){
								string paramNameFull = VObj->getSlotName(param)+"{"+ShadowPool->Name()+"}";
								Verbose(10) << "setParamValuesFromMap: Adding parameter " << paramNameFull <<": "<< value;
								Verbose(10) << " Now setting " << paramNameFull <<" to " <<ts.getParamVal(paramNameFull);
								param->setValueFromText(ts.getParamVal(paramNameFull), ShadowPool, "", false, nullptr);
							}
						}
						return true;
					}
				}
				GetValueFromSnapForAllValuePools(si, timeSnap, obj.first, VParam, found);
				VParam->iterateShadows(GetValueFromSnapForAllValuePools);
				if(!found){
					if(RefPtr<VValue<double>> doubleVal = VParam->getGlobalValue()){
						size_t pos = VParam->Name().rfind(".");
						if (pos == string::npos){
							Verbose(0) << " Error: Could not extract parameter name from "<< VParam->Name();
						}
						string paramName = VParam->Name().substr(pos+1, VParam->Name().length() - (pos+1) );
						Verbose(10) << "setParamValuesFromMap: Adding parameter " << paramName <<": "<< doubleVal;
						Verbose(10) << " Now setting " << paramName <<" to " <<timeSnap.getParamVal(paramName);
						RefPtr<ValuePool> VP;
						VParam->setValueFromText(timeSnap.getParamVal(paramName), VP, "", false, nullptr);
					}
				}
			}
			else {
				Verbose(0) << "Error while updating: Cannot find parameter " << VParam->Name();
			}
			paramNum++;
		}
	}

	return true;
}

// gets interpolated parameter values from H5 group at time time
// update: whether we want to perform an update to the current Viz network parameters
// prev: whether this is a previous TimeSnap entry
// parent: whether we provide the parent group's hid. If yes, we have to open the parent and then get that H5 attribute, otherwise directly get the H5 attribute from the specified hid.
bool VSnapshotPlayerBase::getParamValuesFromH5Group(SnapshotInfo&si, TimeSnap& timeSnapCurrent, hid_t group_id, double time, bool update, bool prev, bool parent){
	TimeSnap ts = (*si.timeSnapMap)[time];
	for(const auto obj: si.interpolatedObjects){
		RefPtr<VObject> VObj = obj.first;
		// we need to open this object's group only if we are in nativeH5. Otherwise, group is already open and we can directly get the H5 attribute!
		hid_t obj_group;
		if (parent){
			obj_group = H5Gopen2(group_id, SaveObjectName(VObj->Name()).c_str(), H5P_DEFAULT);
			Verbose(0) <<" Opening group "<< SaveObjectName(VObj->Name()) << " (parent=true)";
		}
		else{
			obj_group = si.h5info->H5groups[si.h5info->scriptRootGroupName+"/"+SaveObjectName(VObj->Name())];
			Verbose(0) <<" Group already open: "<< si.h5info->scriptRootGroupName+"/"+SaveObjectName(VObj->Name()) << " (parent=false)";
		}
		if (obj_group <=0){
			Verbose(0) << "getParamValuesFromH5Group: Error opening group "<< SaveObjectName(VObj->Name());
			return false;
		}
		Verbose(0)<< " Looking for object "<< VObj->Name();
		unsigned paramNum=0;
		for(RefPtr<VParameter > VParam : obj.second){ // for each Vparam, set the value as the one we got from the corresponding snapshot of current time!
			if (VParam){
				Verbose(0) <<" Found parameter "<< VParam->Name();
				// get parameter value from specific snapshot!
				Verbose(0)<< " Getting shadow values of "<< VParam->Name() << " from Viz network!";
				bool found = false;
				struct VSI : ValueShadowIterator {
					SnapshotInfo& si;
					TimeSnap& ts;
					RefPtr<VObject> VObj;
					RefPtr<VParameter> param;
					const hid_t& obj_group;
					double time;
					bool update;
					bool& found;
					VSI(SnapshotInfo& si, TimeSnap& ts, RefPtr<VObject> VObj, RefPtr<VParameter> Vparam, const hid_t& obj_group, double time, bool update, bool& found):
						si(si),
						ts(ts),
						VObj(VObj),
						param(Vparam),
						obj_group(obj_group),
						time(time),
						update(update),
						found(found){}
					bool apply(const RefPtr<ValuePool>&ShadowPool, const RefPtr<VValueBase>&ShadowValue) override
					{
						assert( ShadowPool );
						assert( ShadowValue );
						string	value = ShadowValue->Text();
						Verbose(0)<< " Value: "<< value;
						// a complete parameter interpolation would take into account the parameter type.
						// However for the purpose of this experiment we only need to interpolate parameters of type Eagle::PhysicalSpace::point
						if (value.find('\n') == string::npos){
							Verbose(0) << "Found value: " << value << " (shadow pool: "<<ShadowPool->Name()<<")";
							found = true;
							if (RefPtr<VValue<Eagle::PhysicalSpace::point>> pointValue = ShadowValue){
								string paramNameFull = VObj->getSlotName(param)+"{"+ShadowPool->Name()+"}";
								// TODO (2017): replace getParamFromAttrH5AsStr with getAttrAsString (or get it as its actual type)
								string paramVal = getParamFromAttrH5AsStr(obj_group, paramNameFull);
								Verbose(0) << "Got value from attr H5: " << paramNameFull <<"= "<< paramVal;
								ts.addParamVal(paramNameFull, paramVal);
								if (update){
									Verbose(0) << "Now setting in Viz network, " << paramNameFull <<" to " <<paramVal;
									param->setValueFromText(paramVal, ShadowPool, "", false, nullptr);
								}
							}
						}
						return true;
					}
				}
				GetValueFromSnapForAllValuePools(si, ts, VObj, VParam, obj_group, time, update, found);
				VParam->iterateShadows(GetValueFromSnapForAllValuePools);
				// we didn't find any shadow pool values, get global value instead!
				if(!found){
					if (RefPtr<VValue<double>> doubleVal = VParam->getGlobalValue()){
						Verbose(0) << " Global value: " << (double)(*doubleVal);
						size_t pos = VParam->Name().rfind(".");
						if (pos == string::npos){
							Verbose(0) << " Error: Could not extract parameter name from "<< VParam->Name();
						}
						string paramName = VParam->Name().substr(pos+1, VParam->Name().length() - (pos+1) );
						string paramVal = getParamFromAttrH5AsStr(obj_group, paramName);
						Verbose(0) << "Got value from attr H5: " << paramName <<"= "<< paramVal;
						// change ts to include parameter name including the object!
						ts.addParamVal(paramName, paramVal);
						if (update){
							Verbose(0) << "Now setting in Viz network, " << paramName <<" to " <<paramVal;
							RefPtr<ValuePool> VP;
							VParam->setValueFromText(paramVal, VP, "", false, nullptr);
							//VValue<double>valFromH5 = VValue(1.5);
							//VParam->setGlobalValue(valFromH5, "min");
						}
					}
				}

			}
			else {
				Verbose(0) << "Error while updating: Cannot find parameter " << VParam->Name();
			}
			paramNum++;
		}
		if(parent)
			assert(H5Gclose(obj_group) == 0);
	}
	if (!update){
		if (prev){
			Verbose(10) << " ^^^^^ Setting previous of "<< timeSnapCurrent.getTime() << " to be: " << time;
			timeSnapCurrent.setPrevSnapPersisted( & (*si.timeSnapMap)[time]  );
		}
		else {
			Verbose(10)<< " ^^^^^ Setting next of "<< timeSnapCurrent.getTime() << " to be: " << time;
			timeSnapCurrent.setNextSnapPersisted( & (*si.timeSnapMap)[time]  );
		}
	}
	(*si.timeSnapMap)[time] = ts;
	return true;
}
Eagle::PhysicalSpace::point stringToPoint(string val){
	Eagle::PhysicalSpace::point p;
	size_t curCommaPos = val.find(",");
	p[0] = atof(val.substr(1, curCommaPos-1).c_str());
	size_t nextCommaPos = val.find(",", curCommaPos+1);
	p[1] = atof(val.substr(curCommaPos+1, nextCommaPos-curCommaPos-1).c_str());
	p[2] = atof(val.substr(nextCommaPos+1, val.length()-nextCommaPos-1).c_str());
	return p;
}

string pointToString(Eagle::PhysicalSpace::point p){
	string valStr = "";
	valStr+="{"+to_string(p[0])+","+to_string(p[1])+","+to_string(p[2])+"}";
	return valStr;
}


bool VSnapshotPlayerBase::interpolateDynamic(SnapshotInfo& si, TimeSnap& timeSnapCur, TimeSnap& timeSnapPrev, TimeSnap& timeSnapNext, double timeIncrement){
	if( timeSnapCur.getPrevSnapPersisted() == nullptr || timeSnapCur.getNextSnapPersisted() == nullptr){
		Verbose(0)<< "interpolateDynamic: hmmm, previous or next persisted timeSnap is null, something went wrong!";
		return false;
	}
	double timeCurrent = timeSnapCur.getTime();
	unsigned paramNum=0;
	double steps =  (timeSnapCur.getNextSnapPersisted()->getTime() - timeSnapCur.getPrevSnapPersisted()->getTime()) / timeIncrement;
	double stepCurr = (timeCurrent - timeSnapCur.getPrevSnapPersisted()->getTime()) / timeIncrement;
	double relativeTime = 1 - (double)((steps - stepCurr) / steps);
	Verbose(45)<< "!! Time: "<< timeCurrent << ", Time Increment: "<< timeIncrement << ", Relative Time: "<< relativeTime<< ", Steps: " << steps<< ", Current Step: "<< stepCurr << ", Relative Time: " << relativeTime;
	if( timeSnapCur.getPrevSnapPersisted() != nullptr ){
		Verbose(10)<< "@@ Previous persisted time: " << timeSnapCur.getPrevSnapPersisted()->getTime();
	}
	if(timeSnapCur.getNextSnapPersisted() != nullptr ){
		Verbose(10)<< "@@ Next persisted time: " << timeSnapCur.getNextSnapPersisted()->getTime();
	}


	for(const auto obj: si.interpolatedObjects){
		for(RefPtr<VParameter > VParam : obj.second){ // for each Vparam, set the value as the one we got from the corresponding snapshot of current time!
			if (VParam){
				Verbose(10)<< "Iterating over shadow values of parameter "<< VParam->Name();
				bool found = false;
				// get parameter value from specific snapshot!
				struct VSI : ValueShadowIterator {
					SnapshotInfo& si;
					RefPtr<VObject> VObj;
					RefPtr<VParameter> param;
					double time, relativeTime;
					TimeSnap &timeSnapCur, & timeSnapPrev, &timeSnapNext;
					bool &found;
					VSI(SnapshotInfo& si, RefPtr<VObject> VObj, RefPtr<VParameter> Vparam, double time, double relativeTime, TimeSnap& tsNew, TimeSnap& timeSnapPrev, TimeSnap& timeSnapNext, bool& found):
						si(si),
						VObj(VObj),
						param(Vparam),
						time(time),
						relativeTime(relativeTime),
						timeSnapCur(tsNew),
						timeSnapPrev(timeSnapPrev),
						timeSnapNext(timeSnapNext),
						found(found){}
					bool apply(const RefPtr<ValuePool>&ShadowPool, const RefPtr<VValueBase>&ShadowValue) override
					{
						assert( ShadowPool );
						assert( ShadowValue );
						string	value = ShadowValue->Text();
						// a complete parameter interpolation would take into account the parameter type.
						// However for the purpose of this experiment we only need to interpolate parameters of type Eagle::PhysicalSpace::point
						if (value.find('\n') == string::npos){
							Verbose(10)<<" Found shadow value: "<< ShadowValue->Text()<<" (pool: "<< ShadowPool->Name()<<")";
							// for now we deal with Eagle::PhysicalSpace parameters only! Should implement a generic interpolation function to support any type!
							found = true;
							if (RefPtr<VValue<Eagle::PhysicalSpace::point>> pointValue = ShadowValue){
								string paramNameFull = VObj->getSlotName(param)+"{"+ShadowPool->Name()+"}";
								Eagle::PhysicalSpace::point prevVal, nextVal, interpolatedVal;
								Verbose(10) << "Time of previous time-snap entry: " << timeSnapPrev.getTime() <<", snapshot: "<< timeSnapPrev.getSnapshot();
								// must convert string to point!
								// This is how we should get the previous value ! It should be the one from snapshot, NOT the interpolated!!
								//timeSnapCur.getPrevSnapPersisted()
								string prevValStr = timeSnapCur.getPrevSnapPersisted()->getParamVal(paramNameFull);
								prevVal = stringToPoint(prevValStr);
								string nextValStr = timeSnapCur.getNextSnapPersisted()->getParamVal(paramNameFull);
								nextVal = stringToPoint(nextValStr);
								for(int i=0; i<prevVal.size(); i++){
									interpolatedVal[i] = (1-relativeTime) * prevVal[i] + relativeTime * nextVal[i];
								}
								Verbose(10) << " Now setting current val to " << pointToString(interpolatedVal);
								Verbose(10) << " Now setting prev val to " << prevValStr;
								Verbose(10) << " Now setting next val to " << nextValStr;
								param->setValue(interpolatedVal, ShadowPool, "", false, nullptr);
								timeSnapCur.addParamVal(paramNameFull, pointToString(interpolatedVal));
								//timeSnapCur.addPrevSnapParamVal(paramNameFull, prevValStr);
								//timeSnapCur.addNextSnapParamVal(paramNameFull, nextValStr);
								timeSnapCur.setInitialized();
							}
						}
						return true;
					}
				}
				GetValueFromSnapForAllValuePools(si, obj.first, VParam, timeCurrent, relativeTime, timeSnapCur, timeSnapPrev, timeSnapNext, found);
				VParam->iterateShadows(GetValueFromSnapForAllValuePools);
				if (!found){ // we didn't find any shadow pool values, get global value instead!
					// for now we only need to support double type.
					if(RefPtr<VValue<double>> doubleVal = VParam->getGlobalValue()){
						size_t pos = VParam->Name().rfind(".");
						if (pos == string::npos){
							Verbose(0) << " Error: Could not extract parameter name from "<< VParam->Name();
						}
						string paramName = VParam->Name().substr(pos+1, VParam->Name().length() - (pos+1) );
						double prevVal, nextVal, interpolatedVal;
						Verbose(10) << "Time of previous time-snap entry: " << timeSnapPrev.getTime() <<", snapshot: "<< timeSnapPrev.getSnapshot();
						string prevValStr = timeSnapCur.getPrevSnapPersisted()->getParamVal(paramName);
						prevVal = atof(prevValStr.c_str());
						string nextValStr = timeSnapCur.getNextSnapPersisted()->getParamVal(paramName);
						nextVal = atof(nextValStr.c_str());
						interpolatedVal = (1-relativeTime) * prevVal + relativeTime * nextVal;
						Verbose(0) << " Now setting current val to " << interpolatedVal;
						Verbose(0) << " Now setting prev val to " << prevValStr;
						Verbose(0) << " Now setting next val to " << nextValStr;
						RefPtr<ValuePool> VP;
						VParam->setValue(interpolatedVal, VP, "", false, nullptr);
						timeSnapCur.addParamVal(paramName, to_string(interpolatedVal));
						timeSnapCur.setInitialized();
					}

				}
			}
			else {
				Verbose(0) << "Error while updating: Cannot find parameter " << VParam->Name();
			}
			paramNum++;
		}

	}
	//cout << " ******* HERE Storing previous persisted: " << &timeSnapPrev <<endl;
	//cout << " ******* HERE Storing next persisted: " << &timeSnapNext <<endl;
	//tsNew.setPrevSnapPersisted(&timeSnapPrev);
	//tsNew.setNextSnapPersisted(&timeSnapNext);
	//if (tsNew.getPrevSnapPersisted() == nullptr)
		//cout << " It is NULL!!" <<endl;
	//cout << " Now trying to access it!: " << tsNew.getPrevSnapPersisted()->getTime() <<endl;
	//cout << " ******* HERE prev snap persisted: " << (*si.timeSnapMap)[timeCurrent].getPrevSnapPersisted() <<endl;
	// store current time-snap mapping to the map!
	(*si.timeSnapMap)[timeCurrent] = timeSnapCur;
	if (si.timeSnapMap->find(0.012500)!= si.timeSnapMap->end()){
		TimeSnap ts = (*si.timeSnapMap)[timeCurrent];
		for (map<string, string>::iterator it = ts.getParamVals().begin(); it!= ts.getParamVals().end(); it++){
			Verbose(45) << " END !!! Param "<< it->first << ", val: "<<it->second;
		}
	}
	return true;
}

bool VSnapshotPlayerNativeH5::getParamValuesFromSnapshot(SnapshotInfo& si, TimeSnap& timeSnapCurrent, double time, bool prev, bool update){
	INIT_COUNTING
	if (si.timeSnapMap->find(time) == si.timeSnapMap->end()){
		Verbose(0)<< "getInterpolatedValuesFromSnapshot: Error: Did not find time "<< time << " in time-snap map!";
		return false;
	}
	unsigned snapshot = (*si.timeSnapMap)[time].getSnapshot();
	Verbose(10)<< "Loading from snapshot " << snapshot;
	START_COUNTING
	if((si.h5info->currentSnapshotNativeGroup = openSnapshot(snapshot, si.h5info->group_id)) <=0 )
	{
		Verbose(0) << "getInterpolatedValuesFromSnapshot: Error opening snapshot "<< snapshot;
		return false;
	}
	getParamValuesFromH5Group(si, timeSnapCurrent, si.h5info->currentSnapshotNativeGroup, time, update, prev, true);
	closeSnapshot(si.h5info->currentSnapshotNativeGroup);
	STOP_COUNTING("__LOAD_SNAPSHOT_DYNAMIC_NATIVE__%u", snapshot);
	return true;
}
VSnapshotPlayerNativeH5::~VSnapshotPlayerNativeH5(){}

// VSnapshotPlayerBDB
VSnapshotPlayerBDB::VSnapshotPlayerBDB(){

}

bool VSnapshotPlayerBDB::getParamValuesFromSnapshot(SnapshotInfo& si, TimeSnap& timeSnapCurrent, double time, bool prev, bool update){
	INIT_COUNTING
	if (si.timeSnapMap->find(time) == si.timeSnapMap->end()){
		Verbose(0)<< "getInterpolatedValuesFromSnapshot: Error: Did not find time "<< time << " in time-snap map!";
		return false;
	}
	unsigned snapshot = (*si.timeSnapMap)[time].getSnapshot();
	Verbose(10)<< "VSnapshot player BDB: Loading from snapshot " << snapshot << " (time "<< time<<")";
#if RETRO_BDB_VOL
	START_COUNTING
	assert(H5RBDB_as_of_snapshot(Vscript_fapl, snapshot) == 0);
	STOP_COUNTING("as_of")
#endif
	START_COUNTING
	getParamValuesFromH5Group(si, timeSnapCurrent, si.h5info->group_id, time, update, prev, false);
	STOP_COUNTING("__LOAD_SNAPSHOT_DYNAMIC_BDB__%u", snapshot)
#if RETRO_BDB_VOL
	START_COUNTING
	assert(H5RBDB_retro_free(Vscript_fapl) == 0);
	STOP_COUNTING("retro free")
#endif
	return true;
}
VSnapshotPlayerBDB::~VSnapshotPlayerBDB(){}

static	Ref<VCreator<VSnapshotPlayer> >
	MyCreator("Tools/VSnapshotPlayer", ObjectQuality::EXPERIMENTAL);
}
