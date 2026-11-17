// Link every pinned adapter and the full core without invoking runtime behavior.
// Volatile pointer observations retain the symbol references under optimization.
#include <stfc_profiles/catalog.h>
#include <stfc_profiles/community_mod_adapter.h>
#include <stfc_profiles/installation.h>

#include <iostream>
#include <stdexcept>

int main()
{
  using namespace stfc::profiles;
  using namespace stfc::profiles::community_mod;
  auto volatile admission = &InstallProcessAdmission;
  auto volatile prepare = &PrepareProfile;
  auto volatile hooks = &InstallProfileHooks;
  auto volatile browser = &LaunchIsolatedBrowser;
  auto volatile catalog = &ExecuteCatalogRequest;
  auto volatile installation = &ExecuteInstallationRequest;
  if (!admission || !prepare || !hooks || !browser || !catalog || !installation)
    throw std::runtime_error("Profiles full-core/adapter link contract is incomplete");
  std::cout << "Profiles core and all adapters linked; no initializer, hook or browser was invoked\n";
}
