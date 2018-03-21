/*
 * VScriptH5Snapshot.hpp
 *
 *  Created on: Jul 20, 2016
 *      Author: dimos
 */

#ifndef FISH_V5_VSCRIPTH5SNAPSHOT_HPP_
#define FISH_V5_VSCRIPTH5SNAPSHOT_HPP_


/**
 * The VScriptH5Snapshot that takes a snapshot of the current state of the Vish network, assuming that there is an open HDF5 file
 * derives from VishSaver
 * Dimokritos Stamatakis
 * Brandeis University
 * July, 2016
 */

#include <ocean/plankton/VLoader.hpp>
#include <string>

namespace Wizt{


struct	VScriptH5Snapshot : Wizt::VishSaver
{
	VScriptH5Snapshot()
	: Wizt::VishSaver("sv5")
	{}

	~VScriptH5Snapshot();

	bool save(const std::string&s) override;

};


}

#endif /* FISH_V5_VSCRIPTH5SNAPSHOT_HPP_ */
