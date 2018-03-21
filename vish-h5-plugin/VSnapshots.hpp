/*
 * VSnapshots.hpp
 *
 *  Created on: Feb 8, 2017
 *      Author: dimos
 */

#ifndef FISH_V5_VSNAPSHOTS_HPP_
#define FISH_V5_VSNAPSHOTS_HPP_



#include <ocean/plankton/VActionNotifier.hpp>
#include <ocean/plankton/VObject.hpp>
#include <ocean/shrimp/Action.hpp>

#include <fish/v5/VScriptH5AppQuitter.hpp>
#include <fish/v5/VCreateScriptH5.hpp>

#include <ocean/vscript/VScriptParser.hpp>
#include <fiber/field/Field.hpp>
#include "VScriptH5.hpp"

namespace Wizt {

#define NUM_INTERPOLATED_PARAMS 2

// class for maintaining manually and automatically created snapshots with the Snap + Interpolate feature
class SnapAndInterpolate {
public:
	// Information about the snap+Interpolate feature.
	// We need to know the number of scripts we are planning to create manually as well as the time steps,
	// so that to create automatically the interpolated scripts between these scripts.
	H5Script& h5script;
	unsigned timestepsTotal;
	unsigned manualScriptsTotal;
	unsigned manualScriptsCreated;
	unsigned timestepCurrent;
	list<RefPtr<VParameter>> interpolatedParams;
	Eagle::PhysicalSpace::point paramDataFirst [NUM_INTERPOLATED_PARAMS];
	SnapAndInterpolate(H5Script& h5script, unsigned timesteps, unsigned mScripts);
	virtual bool doTakeSnapshot(unsigned snap_id) =0;
	bool snapshot();
	bool interpolate(RefPtr<VParameter> Vparam, unsigned paramNum, float relativeTime, Eagle::PhysicalSpace::point* paramDataLatest);
	void addInterpolatedParam(RefPtr<VParameter> param);
	const list<RefPtr<VParameter>>& getInterpolatedParams();
	virtual ~SnapAndInterpolate();
};

class SnapAndInterpolateNativeH5 : public SnapAndInterpolate {
public:
	SnapAndInterpolateNativeH5(H5Script& h5script, unsigned timesteps, unsigned mScripts);
protected:
	bool doTakeSnapshot(unsigned snap_id);
	~SnapAndInterpolateNativeH5();
};

class SnapAndInterpolateBDB : public SnapAndInterpolate {
public:
	SnapAndInterpolateBDB(H5Script& h5script, unsigned timesteps, unsigned mScripts);
protected:
	bool doTakeSnapshot(unsigned snap_id);
	~SnapAndInterpolateBDB();
};

class TimeSnap{
	// first key is the context, which is left for version 2.0. For now we use the same key
	//map<string, map<double, unsigned>> timeSnapIDMap;
	double time; //, timePrev, timeNext; // we must know the time of the previous/next persisted time-snapshots (with an underlying snapshot)
	unsigned snapCurrent;
	// key: interpolated parameter name, value: parameter value)
	map<string, string> paramVals;
	bool paramValsInitialized;
	TimeSnap* prevSnapPersisted, *nextSnapPersisted;
public:
	TimeSnap();
	TimeSnap(double time, unsigned snapID);
	void setTime(double t);
	double getTime();
	unsigned getSnapshot();
	bool addParamVal(string param, string val);
	string getParamVal(string param);
	void setPrevSnapPersisted(TimeSnap* snap);
	void setNextSnapPersisted(TimeSnap* snap);
	TimeSnap* getPrevSnapPersisted();
	TimeSnap* getNextSnapPersisted();
	bool isInitialized();
	void setInitialized();
	map<string, string>& getParamVals();
};

// class for maintaining snapshots
class Snapshots {
protected:
	map<double,TimeSnap> timeSnapMap; // contains time-snapshot mapping. When loading for first time it might be empty, so make sure we first load from snapshots and update it.
	//list<RefPtr<VParameter>> interpolatedParams; // we must know the interpolated params so that to store their current value when taking snapshots!
	H5Script& h5script;
	const map<RefPtr<VObject>, list<RefPtr<VParameter> >> & interpolatedObjs;
public:
	// contextName is required for version 2.0! For now we can treat objects as global.
	virtual bool doTakeSnapshot(string contextName, VTime time)=0;
	bool updateTimeSnapParams(TimeSnap&ts);
	virtual ~Snapshots();
	Snapshots(H5Script& h5script, const map<RefPtr<VObject>, list<RefPtr<VParameter> >> & Objs);
	map<double, TimeSnap>* getTimeSnapMap();
	//void setInterpolatedParams(const list<RefPtr<VParameter>>& interpolatedParams);
	//const list<RefPtr<VParameter>>& getInterpolatedParams();
	const map<RefPtr<VObject>, list<RefPtr<VParameter> >> & getInterpolatedObjs();
	void storeTimeSnapH5();
};

class SnapshotsNativeH5 : public Snapshots{
	unsigned manualScriptsCreated;
public:
	SnapshotsNativeH5(H5Script& h5script, const map<RefPtr<VObject>, list<RefPtr<VParameter> >> & VObjs);
	bool doTakeSnapshot(string contextName, VTime time);
	~SnapshotsNativeH5();
};

class SnapshotsBDB :public Snapshots {
public:
	SnapshotsBDB(H5Script& h5script, const map<RefPtr<VObject>, list<RefPtr<VParameter> >> & VObjs);
	bool doTakeSnapshot(string contextName, VTime time);
	~SnapshotsBDB();
};


}

#endif /* FISH_V5_VSNAPSHOTS_HPP_ */


