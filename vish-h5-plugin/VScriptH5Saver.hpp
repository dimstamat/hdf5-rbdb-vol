/**
 * The VScriptH5Saver that saves the Vish network in an HDF5 file
 * derives from VishSaver
 * Dimokritos Stamatakis
 * Brandeis University
 * April, 2016
 */

#include <ocean/plankton/VLoader.hpp>
#include "VScriptH5.hpp"
#include <string>

using namespace Wizt;

struct	VishScriptH5Saver : Wizt::VishSaver
{
	H5Script::H5FileInfo* h5info;
	VishScriptH5Saver(H5Script::H5FileInfo* h5info);

	~VishScriptH5Saver();

	bool save(const std::string&s) override;

};
