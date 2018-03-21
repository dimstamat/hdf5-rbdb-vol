/**
 * Saving the Vish network in an HDF5 file
 * Dimokritos Stamatakis
 * Brandeis University
 * April, 2016
 */

#ifndef	__VCREATESCRIPTH5_HPP
#define	__VCREATESCRIPTH5_HPP

#include <ostream>
#include <hdf5.h>
#include "VScriptH5.hpp"

namespace Wizt
{

/**
   Create a Vish Script in HDF5 format from the current status of
   Vish Objects, their parameters and relationships,
   and write it to the given output stream.
 */

bool createVScriptH5(const std::string&ScriptPathName, H5Script::H5FileInfo* h5info);
bool createVScriptExistingH5(H5Script::H5FileInfo* h5fileinfo, string& scriptName, bool snapshot);


extern std::string VScriptEscapeIdentifierH5(const std::string&InternalObjectName);
extern std::string VScriptUnEscapeIdentifierH5(const std::string&ExternalObjectName);

const std::string VScriptH5Type = "Wizt::VScriptH5";

string	SaveObjectName(const string&s);

} // namespace Wizt

#endif	// __VCREATESCRIPTH5_HPP
