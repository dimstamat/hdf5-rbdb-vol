#include <ocean/plankton/VInit.hpp>
#include "HDF5init.hpp"

//VISH_DEFAULT_INIT

namespace Wizt {
VISH_INITIALIZER bool VISH_INIT_PROCEDURE(VISH_API_VERSION,VISH_INIT_NAME_SEPARATOR,VISH_MODULE)(VInitialization*VInit) {
	HDF5init init;
    return VInit->IsCompatible() && ( (!(0[VISH_APPLICATION_DOMAIN])) || (VInit->applications.find(VISH_APPLICATION_DOMAIN) != VInit->applications.end()));
}
}
