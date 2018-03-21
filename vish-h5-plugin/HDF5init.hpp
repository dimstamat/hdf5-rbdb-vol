#ifndef __HDF5INIT_HPP__
#define __HDF5INIT_HPP__

using namespace std;
#include <string>
#include <hdf5.h>

#define RETRO_BDB_VOL 1

namespace Wizt {

class HDF5init {
public:
	HDF5init();
	~HDF5init();

private:
	herr_t parseSettings(string settings);


};
}


#endif
// __HDF5INIT_HPP__
