/**
 * The VScriptH5Saver that saves the Vish network in an HDF5 file
 * derives from VishSaver
 * Dimokritos Stamatakis
 * Brandeis University
 * April, 2016
 */

#include "VScriptH5Saver.hpp"
#include "VCreateScriptH5.hpp"
#include <memcore/stringutil.hpp>
#include <fstream>

#ifdef __linux__
#include <sys/stat.h> 
#endif

#include <sys/time.h>

using namespace Wizt;
using namespace std;
using namespace MemCore;


VishScriptH5Saver::VishScriptH5Saver(H5Script::H5FileInfo* h5info): Wizt::VishSaver("v5"), h5info(h5info)
	{}

VishScriptH5Saver::~VishScriptH5Saver()
{}

bool	VishScriptH5Saver::save(const string&s)
{
	static struct timeval begin, end;
//	printf("struct VishScriptH5Saver::save(%s) EXT=%s\n", s.c_str(), extname(s).c_str() );
	string	filename = s;
	if (extname(s) != "v5")
		filename += ".v5";
	{
		printf("Saving %s\n", filename.c_str());
		gettimeofday(&begin, NULL);
		createVScriptH5(filename, h5info);
		gettimeofday(&end, NULL);
		unsigned long long latency_us = 1000000 * (end.tv_sec - begin.tv_sec) + (end.tv_usec - begin.tv_usec);
		printf("%llu\n", latency_us);
	}

#ifdef __linux__ 
	//chmod(filename.c_str(), 0777);
#endif

	return true;
}
