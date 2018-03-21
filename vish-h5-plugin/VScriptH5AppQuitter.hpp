#ifndef __VSCRIPTH5APPQUITTER_HPP__
#define __VSCRIPTH5APPQUITTER_HPP__

#include "ocean/plankton/VGlobals.hpp"

namespace Wizt {

    typedef void (* VScriptH5Cleanup) ();
	struct VScriptH5AppQuitter : ApplicationQuitter {
        VScriptH5Cleanup cleanup;

        VScriptH5AppQuitter(VScriptH5Cleanup H5cleanup);
        void quit();
    };
    
} // namespace Wizt

#endif // __VSCRIPTH5APPQUITTER_HPP__
