// GcHandle ownership against a fake handle table: null and failed roots, moves, resets and weak targets.

#include "il2cpp_test_api.h"

#include <iostream>
#include <stdexcept>
#include <unordered_map>

namespace
{
Il2CppObject                                      object{}, other{};
std::unordered_map<Il2CppGCHandle, Il2CppObject*> targets;
uintptr_t                                         next_handle     = 1;
bool                                              fail_allocation = false;
int                                               frees           = 0;
bool                                              weak            = false;
bool                                              runtime_alive   = true;

void Require(bool condition)
{
  if (!condition)
    throw std::runtime_error("GC ownership regression");
}
} // namespace

API(Il2CppGCHandle, il2cpp_gchandle_new, (Il2CppObject * object, bool pinned))
{
  Require(runtime_alive);
  if (!object || pinned)
    throw std::runtime_error("invalid root");
  if (fail_allocation)
    return nullptr;
  auto handle     = reinterpret_cast<Il2CppGCHandle>(next_handle++);
  targets[handle] = object;
  return handle;
}
END_API
API(Il2CppGCHandle, il2cpp_gchandle_new_weakref, (Il2CppObject * object, bool resurrect))
{
  Require(runtime_alive);
  if (resurrect)
    throw std::runtime_error("unexpected resurrection");
  weak = true;
  return il2cpp_gchandle_new(object, false);
}
END_API
API(Il2CppObject*, il2cpp_gchandle_get_target, (Il2CppGCHandle handle))
{
  Require(runtime_alive);
  return targets.at(handle);
}
END_API
API(void, il2cpp_gchandle_free, (Il2CppGCHandle handle))
{
  Require(runtime_alive);
  if (targets.erase(handle) != 1)
    throw std::runtime_error("handle freed twice");
  ++frees;
}
END_API

int main()
{
  Require(!il2cpp::GcHandle::Strong(nullptr));
  Require(!il2cpp::GcHandle::Weak(nullptr));
  fail_allocation = true;
  Require(!il2cpp::GcHandle::Strong(&object) && targets.empty());
  fail_allocation = false;
  {
    auto root = il2cpp::GcHandle::Strong(&object);
    Require(root.Get() == &object);
    auto moved = std::move(root);
    Require(!root && !root.Get() && moved.Get() == &object);
    root  = il2cpp::GcHandle::Strong(&other);
    moved = std::move(root);
    Require(frees == 1 && !root && moved.Get() == &other);
    auto& same = moved;
    moved      = std::move(same);
    Require(moved.Get() == &other && frees == 1);
    moved.Reset();
    moved.Reset();
    Require(!moved && frees == 2);
    root = il2cpp::GcHandle::Weak(&object);
    Require(weak && root.Get() == &object);
    targets.begin()->second = nullptr; // Managed collection clears a weak target.
    Require(root && !root.Get());
  }
  Require(frees == 3 && targets.empty());
  {
    auto strong = il2cpp::GcHandle::Strong(&object);
    auto weak_root = il2cpp::GcHandle::Weak(&other);
    Require(strong.Get() == &object && weak_root.Get() == &other);
    il2cpp::GcHandle::StopRuntime();
    runtime_alive = false;
    targets.clear(); // Native teardown has reclaimed its table before C++ statics are destroyed.
    Require(!strong && !strong.Get() && !weak_root && !weak_root.Get());
    Require(!il2cpp::GcHandle::Strong(&object) && !il2cpp::GcHandle::Weak(&other));
    auto moved = std::move(strong);
    weak_root = std::move(moved);
    weak_root.Reset();
    weak_root.Reset();
    il2cpp::GcHandle::StopRuntime(); // Terminal and idempotent, not an in-process reload reset.
  }
  Require(frees == 3 && targets.empty()); // No API call touched the reclaimed handle table.
  std::cout << "IL2CPP GC ownership tests passed\n";
}
