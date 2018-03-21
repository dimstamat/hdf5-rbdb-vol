#include "VScriptH5AppQuitter.hpp"

namespace Wizt {

    VScriptH5AppQuitter::VScriptH5AppQuitter(VScriptH5Cleanup H5cleanup) : ApplicationQuitter(){
        cleanup = H5cleanup;
    }
	void VScriptH5AppQuitter::quit(){
        printf("VScriptH5AppQuitter::quit()!\n");
        (*cleanup)();
    }
    
} // namespace Wizt