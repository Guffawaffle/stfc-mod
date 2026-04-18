/**
 * @file probe.h
 * @brief IL2CPP runtime introspection toolkit.
 *
 * Reusable utilities for exploring game classes at runtime. Enumerates methods,
 * fields, properties, events, interfaces, nested types, and parent hierarchy
 * for any IL2CPP class — without requiring offline dumps or pre-built headers.
 *
 * Usage:
 *   probe::dump_class("Assembly-CSharp", "Digit.Prime.Ships", "FleetLocalViewController");
 *   probe::dump_methods("Assembly-CSharp", "Digit.Prime.HUD", "FleetBarViewController");
 *   probe::dump_namespace("Assembly-CSharp", "Digit.Prime.Ships");
 *   probe::search_methods("RequestSelect");  // search ALL classes
 */
#pragma once

#include <il2cpp/il2cpp_helper.h>
#include <spdlog/spdlog.h>

#include <string>
#include <string_view>

namespace probe {

// ─── Helpers ──────────────────────────────────────────────────────────────────

namespace detail {

  /** @brief Format an IL2CPP type as a human-readable string. Caller must free. */
  inline std::string type_name(const Il2CppType* type)
  {
    if (!type)
      return "???";
    char* raw = il2cpp_type_get_name(type);
    if (!raw)
      return "???";
    std::string s(raw);
    il2cpp_free(raw);
    return s;
  }

  /** @brief Build a method signature string: ReturnType Name(ParamType paramName, ...) */
  inline std::string method_signature(const MethodInfo* m)
  {
    std::string sig;

    // Static marker
    if (!il2cpp_method_is_instance(m))
      sig += "static ";

    // Return type
    sig += type_name(il2cpp_method_get_return_type(m));
    sig += ' ';
    sig += il2cpp_method_get_name(m);
    sig += '(';

    auto count = il2cpp_method_get_param_count(m);
    for (uint32_t i = 0; i < count; ++i) {
      if (i > 0)
        sig += ", ";
      sig += type_name(il2cpp_method_get_param(m, i));
      auto pname = il2cpp_method_get_param_name(m, i);
      if (pname && pname[0]) {
        sig += ' ';
        sig += pname;
      }
    }

    sig += ')';
    return sig;
  }

  /** @brief Resolve a class helper; returns null cls if not found. */
  inline IL2CppClassHelper resolve(const char* assembly, const char* ns, const char* cls)
  {
    return il2cpp_get_class_helper(assembly, ns, cls);
  }

} // namespace detail

// ─── Method Enumeration ──────────────────────────────────────────────────────

/**
 * @brief Log all methods on a class, including full signatures.
 * @param assembly Assembly name (e.g. "Assembly-CSharp").
 * @param ns       Namespace (e.g. "Digit.Prime.HUD").
 * @param cls      Class name (e.g. "FleetBarViewController").
 */
inline void dump_methods(const char* assembly, const char* ns, const char* cls)
{
  auto helper = detail::resolve(assembly, ns, cls);
  auto* klass = helper.get_cls();
  if (!klass) {
    spdlog::warn("[probe] {}.{} — class not found in {}", ns, cls, assembly);
    return;
  }

  spdlog::info("[probe] === {}.{} methods ===", ns, cls);
  void* iter = nullptr;
  int   n    = 0;
  while (const MethodInfo* m = il2cpp_class_get_methods(klass, &iter)) {
    spdlog::info("[probe]   {:>3}. {} @ {:p}", n++, detail::method_signature(m), (const void*)m->methodPointer);
  }
  spdlog::info("[probe] === {}.{} — {} methods total ===", ns, cls, n);
}

// ─── Field Enumeration ───────────────────────────────────────────────────────

/**
 * @brief Log all fields on a class with types and offsets.
 */
inline void dump_fields(const char* assembly, const char* ns, const char* cls)
{
  auto helper = detail::resolve(assembly, ns, cls);
  auto* klass = helper.get_cls();
  if (!klass) {
    spdlog::warn("[probe] {}.{} — class not found in {}", ns, cls, assembly);
    return;
  }

  spdlog::info("[probe] === {}.{} fields ===", ns, cls);
  void* iter = nullptr;
  int   n    = 0;
  while (FieldInfo* f = il2cpp_class_get_fields(klass, &iter)) {
    auto offset = il2cpp_field_get_offset(f);
    auto tname  = detail::type_name(il2cpp_field_get_type(f));
    spdlog::info("[probe]   {:>3}. {} {} (offset: 0x{:x})", n++, tname, il2cpp_field_get_name(f), offset);
  }
  spdlog::info("[probe] === {}.{} — {} fields total ===", ns, cls, n);
}

// ─── Property Enumeration ────────────────────────────────────────────────────

/**
 * @brief Log all properties on a class with getter/setter info.
 */
inline void dump_properties(const char* assembly, const char* ns, const char* cls)
{
  auto helper = detail::resolve(assembly, ns, cls);
  auto* klass = helper.get_cls();
  if (!klass) {
    spdlog::warn("[probe] {}.{} — class not found in {}", ns, cls, assembly);
    return;
  }

  spdlog::info("[probe] === {}.{} properties ===", ns, cls);
  void* iter = nullptr;
  int   n    = 0;
  while (const PropertyInfo* p = il2cpp_class_get_properties(klass, &iter)) {
    auto* getter = il2cpp_property_get_get_method(const_cast<PropertyInfo*>(p));
    auto* setter = il2cpp_property_get_set_method(const_cast<PropertyInfo*>(p));
    std::string access;
    if (getter)
      access += "get";
    if (getter && setter)
      access += "/";
    if (setter)
      access += "set";

    std::string ret_type = "???";
    if (getter)
      ret_type = detail::type_name(il2cpp_method_get_return_type(getter));
    else if (setter && il2cpp_method_get_param_count(setter) > 0)
      ret_type = detail::type_name(il2cpp_method_get_param(setter, 0));

    spdlog::info("[probe]   {:>3}. {} {} {{ {} }}", n++, ret_type, il2cpp_property_get_name(const_cast<PropertyInfo*>(p)), access);
  }
  spdlog::info("[probe] === {}.{} — {} properties total ===", ns, cls, n);
}

// ─── Event Enumeration ───────────────────────────────────────────────────────

/**
 * @brief Log all events on a class.
 */
inline void dump_events(const char* assembly, const char* ns, const char* cls)
{
  auto helper = detail::resolve(assembly, ns, cls);
  auto* klass = helper.get_cls();
  if (!klass) {
    spdlog::warn("[probe] {}.{} — class not found in {}", ns, cls, assembly);
    return;
  }

  spdlog::info("[probe] === {}.{} events ===", ns, cls);
  void* iter = nullptr;
  int   n    = 0;
  while (const EventInfo* e = il2cpp_class_get_events(klass, &iter)) {
    spdlog::info("[probe]   {:>3}. {}", n++, e->name ? e->name : "(unnamed)");
  }
  spdlog::info("[probe] === {}.{} — {} events total ===", ns, cls, n);
}

// ─── Class Hierarchy ─────────────────────────────────────────────────────────

/**
 * @brief Log parent chain and implemented interfaces.
 */
inline void dump_hierarchy(const char* assembly, const char* ns, const char* cls)
{
  auto helper = detail::resolve(assembly, ns, cls);
  auto* klass = helper.get_cls();
  if (!klass) {
    spdlog::warn("[probe] {}.{} — class not found in {}", ns, cls, assembly);
    return;
  }

  // Parent chain
  spdlog::info("[probe] === {}.{} hierarchy ===", ns, cls);
  auto* parent = il2cpp_class_get_parent(klass);
  int   depth  = 1;
  while (parent) {
    spdlog::info("[probe]   {}> {}.{}", std::string(depth * 2, ' '), il2cpp_class_get_namespace(parent),
                 il2cpp_class_get_name(parent));
    parent = il2cpp_class_get_parent(parent);
    ++depth;
  }

  // Interfaces
  void* iter = nullptr;
  int   n    = 0;
  while (auto* iface = il2cpp_class_get_interfaces(klass, &iter)) {
    if (n == 0)
      spdlog::info("[probe]   implements:");
    spdlog::info("[probe]     - {}.{}", il2cpp_class_get_namespace(iface), il2cpp_class_get_name(iface));
    ++n;
  }

  spdlog::info("[probe]   instance size: {} bytes", il2cpp_class_instance_size(klass));
}

// ─── Full Class Dump ─────────────────────────────────────────────────────────

/**
 * @brief Dump everything: hierarchy, fields, properties, methods, events.
 */
inline void dump_class(const char* assembly, const char* ns, const char* cls)
{
  spdlog::info("[probe] ╔══════════════════════════════════════════════════╗");
  spdlog::info("[probe] ║  {}.{}", ns, cls);
  spdlog::info("[probe] ╚══════════════════════════════════════════════════╝");
  dump_hierarchy(assembly, ns, cls);
  dump_fields(assembly, ns, cls);
  dump_properties(assembly, ns, cls);
  dump_methods(assembly, ns, cls);
  dump_events(assembly, ns, cls);
}

// ─── Namespace Scan ──────────────────────────────────────────────────────────

/**
 * @brief List all classes in a given namespace within an assembly image.
 */
inline void dump_namespace(const char* assembly, const char* ns)
{
  auto* domain = il2cpp_domain_get();
  if (!domain) {
    spdlog::warn("[probe] il2cpp_domain_get() returned null");
    return;
  }

  size_t              count = 0;
  const auto**        assemblies = il2cpp_domain_get_assemblies(domain, &count);
  const Il2CppImage*  target_image = nullptr;
  std::string_view    target_name(assembly);

  for (size_t i = 0; i < count; ++i) {
    auto* image = il2cpp_assembly_get_image(assemblies[i]);
    if (image && target_name == il2cpp_image_get_name(image)) {
      target_image = image;
      break;
    }
  }

  if (!target_image) {
    spdlog::warn("[probe] Assembly '{}' not found", assembly);
    return;
  }

  spdlog::info("[probe] === Classes in {}.{} ===", assembly, ns);
  auto class_count = il2cpp_image_get_class_count(target_image);
  int  found       = 0;
  std::string_view target_ns(ns);

  for (size_t i = 0; i < class_count; ++i) {
    auto* klass     = il2cpp_image_get_class(target_image, i);
    auto* klass_ns  = il2cpp_class_get_namespace(const_cast<Il2CppClass*>(klass));
    if (klass_ns && target_ns == klass_ns) {
      auto* klass_name = il2cpp_class_get_name(const_cast<Il2CppClass*>(klass));
      auto  method_count_val = 0;
      void* iter = nullptr;
      while (il2cpp_class_get_methods(const_cast<Il2CppClass*>(klass), &iter))
        ++method_count_val;
      spdlog::info("[probe]   {} ({} methods)", klass_name, method_count_val);
      ++found;
    }
  }
  spdlog::info("[probe] === {} classes in {}.{} ===", found, assembly, ns);
}

// ─── Cross-Class Method Search ───────────────────────────────────────────────

/**
 * @brief Search ALL classes in an assembly for methods matching a name substring.
 * @param assembly Assembly to search.
 * @param name_substr Substring to match against method names.
 * @param max_results Stop after this many hits (0 = unlimited).
 */
inline void search_methods(const char* assembly, const char* name_substr, int max_results = 50)
{
  auto* domain = il2cpp_domain_get();
  if (!domain)
    return;

  size_t       count = 0;
  const auto** assemblies   = il2cpp_domain_get_assemblies(domain, &count);
  const Il2CppImage* target = nullptr;
  std::string_view   target_name(assembly);

  for (size_t i = 0; i < count; ++i) {
    auto* image = il2cpp_assembly_get_image(assemblies[i]);
    if (image && target_name == il2cpp_image_get_name(image)) {
      target = image;
      break;
    }
  }

  if (!target) {
    spdlog::warn("[probe] Assembly '{}' not found", assembly);
    return;
  }

  spdlog::info("[probe] Searching '{}' for methods matching '{}'...", assembly, name_substr);
  auto        class_count = il2cpp_image_get_class_count(target);
  int         hits        = 0;
  std::string needle(name_substr);

  for (size_t i = 0; i < class_count && (max_results == 0 || hits < max_results); ++i) {
    auto* klass = const_cast<Il2CppClass*>(il2cpp_image_get_class(target, i));
    void* iter  = nullptr;
    while (const MethodInfo* m = il2cpp_class_get_methods(klass, &iter)) {
      auto mname = il2cpp_method_get_name(m);
      if (mname && std::string_view(mname).find(needle) != std::string_view::npos) {
        spdlog::info("[probe]   {}.{} :: {}", il2cpp_class_get_namespace(klass), il2cpp_class_get_name(klass),
                     detail::method_signature(m));
        ++hits;
        if (max_results > 0 && hits >= max_results)
          break;
      }
    }
  }
  spdlog::info("[probe] === {} matches for '{}' ===", hits, name_substr);
}

} // namespace probe
