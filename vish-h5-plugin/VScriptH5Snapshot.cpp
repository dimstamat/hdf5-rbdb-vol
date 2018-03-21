/**
 * The VScriptH5Snapshot that takes a snapshot of the current state of the Vish network, assuming that there is an open HDF5 file
 * derives from VishSaver
 * Dimokritos Stamatakis
 * Brandeis University
 * July, 2016
 */

#include "VScriptH5Snapshot.hpp"
#include "VCreateScriptH5.hpp"
#include <memcore/stringutil.hpp>
#include <fstream>

#ifdef __linux__
#include <sys/stat.h>
#endif

#include <hdf5.h>
#include <sys/time.h>

#include "HDF5init.hpp"



using namespace std;
using namespace MemCore;

namespace Wizt {

VScriptH5Snapshot::~VScriptH5Snapshot()
{}

extern hid_t Vscript_fapl;

bool	VScriptH5Snapshot::save(const string&s)
{
//	printf("struct VishScriptH5Saver::save(%s) EXT=%s\n", s.c_str(), extname(s).c_str() );
	string	filename = s;
	if (extname(s) != "sv5")
		filename += ".sv5";
	{
#if RETRO_BDB_VOL
		static struct timeval begin, end;
		gettimeofday(&begin, NULL);
		printf("Declared snapshot %u\n", H5RBDB_declare_snapshot(Vscript_fapl));
		gettimeofday(&end, NULL);
		unsigned long long latency_us = 1000000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec);
		printf("%llu\n", latency_us);

#endif
	}

#ifdef __linux__
	//chmod(filename.c_str(), 0777);
#endif

	return true;
}

}
