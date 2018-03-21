/*
 * VSnapshotPlayer.hpp
 *
 *  Created on: Jul 18, 2016
 *      Author: dimos
 */

#ifndef FISH_V5_VSNAPSHOTPLAYER_HPP_
#define FISH_V5_VSNAPSHOTPLAYER_HPP_


#include <ocean/plankton/VActionNotifier.hpp>
#include <ocean/plankton/VObject.hpp>
#include <ocean/shrimp/Action.hpp>

#include <fish/v5/VScriptH5AppQuitter.hpp>

#include <ocean/vscript/VScriptParser.hpp>
#include <fiber/field/Field.hpp>

#include <ocean/Anemonia/VRenderObject.hpp>
#include <ocean/shrimp/TimeDependent.hpp>
#include "VScriptH5.hpp"

#include <hdf5.h>

namespace Wizt
{
	using std::string;


	class Test{
		public:
		bool operator=(const int&) const{ return true;}
		bool operator==(const Test&) const{ return true;}
	};

class VSnapshotPlayerBase {
public:
		virtual bool displayStatic(SnapshotInfo& si, VTime& time)=0;
		virtual bool displayDynamic(SnapshotInfo& si, VTime& time);
		virtual bool getParamValuesFromSnapshot(SnapshotInfo& si, TimeSnap& timeSnapCurrent, double time, bool prev, bool update)=0;
		bool interpolateDynamic(SnapshotInfo& si, TimeSnap& timeSnapCur, TimeSnap& timeSnapPrev, TimeSnap& timeSnapNext, double timeIncrement);
		bool setParamValuesFromMap(SnapshotInfo& si, TimeSnap& timeSnap);
		bool getParamValuesFromH5Group(SnapshotInfo&si, TimeSnap& timeSnapCurrent, hid_t group_id, double time, bool update, bool prev, bool parent);
		virtual const unsigned getSnapshotFromTime(double time)=0;
		virtual ~VSnapshotPlayerBase();
};

class VSnapshotPlayerNativeH5 : public VSnapshotPlayerBase {
	hid_t openSnapshot(const unsigned& snapId, hid_t root_group_id);
	herr_t closeSnapshot(hid_t snap_group_id);
	bool displayStatic(SnapshotInfo& si, VTime& time);

public:
	VSnapshotPlayerNativeH5();
	~VSnapshotPlayerNativeH5();
	const unsigned getSnapshotFromTime(double time);
	bool getParamValuesFromSnapshot(SnapshotInfo& si, TimeSnap& timeSnapCurrent, double time, bool prev, bool update);
};

class VSnapshotPlayerBDB : public VSnapshotPlayerBase {
public:
	VSnapshotPlayerBDB();
	~VSnapshotPlayerBDB();
	bool displayStatic(SnapshotInfo& si, VTime& time);
	bool getParamValuesFromSnapshot(SnapshotInfo& si, TimeSnap& timeSnapCurrent, double time, bool prev, bool update);
	const unsigned getSnapshotFromTime(double time);
};


class	VSnapshotPlayer : public VRenderObject, public TimeDependent
{
		VSnapshotPlayerBase* player;
public:
	in <int> testIn;
	in <SnapshotInfo> snapInfo;
	in <bool> dynamicInterpolation;

	VSnapshotPlayer(const string&name, int N, const RefPtr<VCreationPreferences>&VP);
	~VSnapshotPlayer();

	bool environment_render(VRenderContext& cntx) const override;
	bool update(VRequest&Context, double precision) override;
	void loadH5();
	bool isDynamicInterpolation();
};

} // namespace Wizt
#endif /* FISH_V5_VSNAPSHOTPLAYER_HPP_ */
