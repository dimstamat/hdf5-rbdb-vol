#include "HDF5init.hpp"
#include <stdio.h>
#include <iostream>
#include <string>
#include "elementary/memcore/PathOpen.hpp"
#include <hdf5.h>


namespace Wizt {


hid_t Vscript_fapl = 0;
bool Vscript_withRBDB = false;


HDF5init::HDF5init(){
	string settings;
	settings = MemCore::FileRead("/home/dimos/projects/hdf5/vish/fish/v5/HDF5init.ini", "rb");
	parseSettings(settings);
}

herr_t HDF5init::parseSettings(string settings){
	hid_t fapl;
	herr_t status;
	size_t newlinepos, equalpos, curpos;
	string linePrefix, param, value, dbpath, dbfile;
	bool useRBDB = false;
	bool libverlatest = false;

	newlinepos = -1;
	curpos = 0;
	while (( newlinepos = settings.find("\n", newlinepos+1)) != string::npos){
		linePrefix = settings.substr(curpos, 1);
		if (linePrefix == "#"){ // comments, ignore!
			curpos = newlinepos+1;
			continue;
		}
		equalpos = settings.find("=", curpos);
		if (equalpos == string::npos)
			break;
		param = settings.substr(curpos, equalpos - curpos);
		value = settings.substr(equalpos+1, newlinepos - equalpos -1);
		curpos = newlinepos+1;
		//printf("param: %s, val: %s\n", param.c_str(), value.c_str());
		if (param == "HDF5plugin"){
			if (value == "RBDB")
				useRBDB = true;
		} else if (param == "RBDBpath"){
			dbpath = value;
		} else if (param == "RBDBfilename"){
			dbfile = value;
		} else if (param == "libverlatest") {
			if (value == "true"){
				libverlatest = true;
			}
			else if (value == "false"){

			}
			else {
				printf("Wrong value for libverlatest. Available values are true/false\n");
				return -1;
			}
		}
		else {
			printf("Wrong HDF5 init parameter!");
			return -1;
		}
	}

	if (useRBDB){
	#if RETRO_BDB_VOL
		fapl = H5Pcreate(H5P_FILE_ACCESS);
		status = H5VL_set_vol_RBDB(fapl, dbpath.c_str(), dbfile.c_str());
		Vscript_fapl = fapl;
		Vscript_withRBDB = true;
	#endif
	}
	else if (libverlatest){
		fapl = H5Pcreate (H5P_FILE_ACCESS);
		status = H5Pset_libver_bounds (fapl, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST);
		Vscript_fapl = fapl;
	}
	else {
		Vscript_fapl = H5P_DEFAULT;
	}
	return status;

}

HDF5init::~HDF5init(){
	//printf(" ^^^^^^^ Destructor of HDF5init!\n");
}

}
