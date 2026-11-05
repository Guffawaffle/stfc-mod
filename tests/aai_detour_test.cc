// Real SPUD patching in this fixture process, through the production AAI shim.
// Not a game binding/signature/extent qualification.
#include "aai_detour_target.h"
#include "aai_host_detour.h"

#include <iostream>
#include <stdexcept>

#if __APPLE__
#include <dlfcn.h>
#endif

namespace
{
using Probe  = AaiDetourProbe;
using Target = void (*)(Probe*);

void Intercept(auto original, Probe* probe)
{
  ++probe->hooks;
  original(probe);
}
Target Install(Target target)
{ return SPUD_STATIC_DETOUR(target, Intercept); }
Target InstallMissing(Target target)
{ return SPUD_STATIC_DETOUR(target, Intercept); }
} // namespace
namespace il2cpp
{
void Report(std::string_view value)
{ std::cerr << value << '\n'; }
// This native-pointer fixture must not enter the managed metadata layer.
std::string Method::Name() const
{ throw std::logic_error("AAI detour fixture unexpectedly requested managed metadata"); }
} // namespace il2cpp
int main(int argc, char** argv)
{
  try {
    const auto require = [](bool value, const char* message = "AAI detour ownership/trampoline failure") {
      if (!value)
        throw std::runtime_error(message);
    };
    Probe probe;
#if __APPLE__
    // Zig 0.16 emits an empty LC_FUNCTION_STARTS table. The Apple-linked
    // fixture image supplies real bounds to the unchanged production preflight.
    if (argc != 2)
      throw std::runtime_error("pass the Apple-linked native target dylib");
    void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!image)
      throw std::runtime_error(dlerror());
    // Keep the image loaded for the permanent hook's complete lifetime.
    Target volatile native = reinterpret_cast<Target>(dlsym(image, "AaiDetourTarget"));
    require(native != nullptr);
    require(!il2cpp::CheckedNativeEntry(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(native) + 4)),
            "an interior instruction must not qualify as a native function entry");
    auto* boundary = dlsym(image, "AaiDetourTargetEnd");
    require(boundary && !il2cpp::CheckedNativeEntry(boundary), "the short terminal target must remain refused");
#else
    Target volatile native = &AaiDetourTarget;
#endif
    native(&probe);
    require(probe.calls == 1 && probe.hooks == 0);
    // Retain the real preflight result when a hosted architecture refuses this
    // fixture. Inspection alone does not authorize patching an unbounded entry.
    const auto plan = spud::inspect_detour(native);
    std::cerr << "AAI fixture preflight: checked="
              << (il2cpp::CheckedNativeEntry(reinterpret_cast<void*>(native)) != nullptr)
              << " error=" << static_cast<int>(plan.error) << " entry=" << reinterpret_cast<uintptr_t>(native)
              << " resolved=" << plan.resolved_target << " near=" << plan.required_patch_bytes
              << " far=" << plan.far_required_patch_bytes << " far_jump=" << plan.far_jump_size << '\n';
    const auto trampoline = Install(native);
    require(trampoline && Install(native) == trampoline);
    native(&probe);
    require(probe.calls == 2 && probe.hooks == 1);
    trampoline(&probe);
    require(probe.calls == 3 && probe.hooks == 1);                // Returned pointer is the actual original path.
    require(!InstallMissing(nullptr) && !InstallMissing(native)); // Latched refusal, no retry.
    native(&probe);
    require(probe.calls == 4 && probe.hooks == 2 && !il2cpp::HookStateIndeterminate());
    std::cout << "AAI host detour: real transaction, actual trampoline, permanent custody and no-retry passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
