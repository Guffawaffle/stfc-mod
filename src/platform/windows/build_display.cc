// The entry module already owns the immutable CI invocation defines used by the
// Windows version resource. Keep that per-build value out of the core mod units.
#include "build_identity.h"
#include "version.h"

std::string_view build_identity::CompiledInvocation()
{ return STFC_BUILD_INVOCATION_ID; }

std::string_view build_identity::CompiledDevelopmentTag()
{ return STFC_DEVELOPMENT_TAG; }
