/**
 * The VScriptH5Loader that loads the Vish network from an HDF5 file
 * derives from VLoader
 * Dimokritos Stamatakis
 * Brandeis University
 * April, 2016
 */

#ifndef __VSCRIPTH5LOADER_HPP
#define __VSCRIPTH5LOADER_HPP

#include "ocean/plankton/VLoader.hpp"
#include "VActivityTracer.hpp"

#include <string>

namespace Wizt
{
    using std::string;

/**
   Data loading: objects derived from the VLoader class are
   intrinsically known by the VISH system.
   The static Load() function may then query all loaders that
   are presently existent.

   Usually a loader is a static object.
 */
class   VBASE_API VScriptH5Loader : public VLoader
{
public:
    	//VObject activityTracer;

		VScriptH5Loader(const char*name, int priority, bool c);
		~VScriptH5Loader();
protected:
		LoadSuccess	loadable(const string&url) override;

		RefPtr<VManagedObject> load(LoadSuccess&LE, const string&script_url, const string&) override;
};
}
#endif
