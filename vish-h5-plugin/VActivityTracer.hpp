#ifndef __VActivityTracer_HPP
#define	__VActivityTracer_HPP

#include <ocean/plankton/VActionNotifier.hpp>
#include <ocean/plankton/VObject.hpp>
#include <fish/v5/VCreateScriptH5.hpp>

#include <hdf5.h>

namespace Wizt
{
	using std::string;

/**
   Logs activities in Vish.
 */

class	VActivityTracer : public VObject
{
public:
	RefPtr<VActionNotifier> MyNotifier;

	VActivityTracer(const string&name, int N, const RefPtr<VCreationPreferences>&VP);

	~VActivityTracer();
};

} // namespace Wizt

#endif	//  __VActivityTracer_HPP
