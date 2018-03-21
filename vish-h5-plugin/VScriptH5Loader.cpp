/**
 * The VScriptH5Loader that loads the Vish network from an HDF5 file
 * derives from VLoader
 * Dimokritos Stamatakis
 * Brandeis University
 * April, 2016
 */

#define _FILE_OFFSET_BITS 64

#include "VScriptH5Loader.hpp"
//#include "VActionNotifier.hpp"
#include "VScriptH5.hpp"
#include <memcore/stringutil.hpp>
#include <memcore/Verbose.hpp>



#ifdef	_WINDOWS
#include <windows.h>
#endif

#include <stdio.h>


namespace Wizt
{


VScriptH5Loader::VScriptH5Loader(const char*name, int priority, bool c)
: VLoader(name, priority, c)
{}

VScriptH5Loader::~VScriptH5Loader()
{}


VLoaderInfo::VLoaderInfo(const string&url, const WeakPtr<VLoader>&ld)
: CreationURL(url)
, Loader(ld)
{}

VLoaderInfo::~VLoaderInfo()
{}

VLoader::LoadSuccess VScriptH5Loader::loadable(const string& url) {
	int	l = url.length();

	if (l<3)
		return "File extension mismatch";

	if (url.substr(l-3, l)==".v5") {
		printf("VScriptLoader::loadable( %s ) I think this should be a HDF5 Vish Script.\n", url.c_str());
		return true;
	}
	return VLoader::LoadSuccess("File extension mismatch");
}

RefPtr<VManagedObject>
	VScriptH5Loader::load(VLoader::LoadSuccess&Ok, const string&url, const string&) {
	printf("Loading HDF5 Vish script %s...\n", url.c_str());
	RefPtr<VScriptH5> vscriptH5 = new VScriptH5(url.c_str(), "VScriptH5", 42, nullptr);
	vscriptH5->loadH5();
	// this will call setup to register the object as Time dependent
	vscriptH5->initialize();
	vscriptH5->enableRequestProcessing();
	//return nullptr;
	return vscriptH5;
}

static OmegaRef<VScriptH5Loader> TheLoader("HDF5 VISH Script", 0, false);

}


