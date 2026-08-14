#include "runtime_identity.h"

#include "version.h"

namespace runtime_identity
{
namespace
{

  void AppendField(std::string& output, const std::string_view name, const std::string_view value)
  {
    output.append(" | ");
    output.append(name);
    output.push_back('=');
    output.append(value.empty() ? "not-recorded" : value);
  }

} // namespace

const Record& Current()
{
  static constexpr Record identity{
      .distribution_id    = STFC_DISTRIBUTION_ID,
      .display_name       = STFC_MOD_DISPLAY_NAME,
      .downstream_version = VER_RUNTIME_VERSION_STR,
      .unofficial_label   = STFC_UNOFFICIAL_LABEL,
      .build_class        = STFC_BUILD_CLASS,
      .build_class_label  = STFC_BUILD_CLASS_LABEL,
      .source_state_id    = STFC_SOURCE_STATE_ID,
      .base_commit        = STFC_BASE_COMMIT,
      .upstream_base      = STFC_UPSTREAM_BASE,
      .test_target        = STFC_TEST_TARGET,
      .test_expiry        = STFC_TEST_EXPIRY,
      .support_boundary   = STFC_SUPPORT_BOUNDARY,
      .reproducible       = STFC_SOURCE_REPRODUCIBLE != 0,
  };
  return identity;
}

std::string SupportIdentity(const Record& identity)
{
  std::string output;
  output.reserve(512);
  output.append(identity.display_name);
  output.push_back(' ');
  output.append(identity.downstream_version);
  output.append(" | ");
  output.append(identity.unofficial_label);
  AppendField(output, "class", identity.build_class);
  AppendField(output, "distribution", identity.distribution_id);
  AppendField(output, "source", identity.source_state_id);
  AppendField(output, "base", identity.base_commit);
  AppendField(output, "upstream", identity.upstream_base);
  AppendField(output, "reproducible", identity.reproducible ? "true" : "false");

  if (identity.build_class == "test") {
    AppendField(output, "target", identity.test_target);
    AppendField(output, "expires", identity.test_expiry);
    AppendField(output, "support", identity.support_boundary);
  }

  return output;
}

std::string SupportIdentity()
{ return SupportIdentity(Current()); }

std::string ShortRuntimeLabel(const Record& identity)
{
  std::string output;
  output.reserve(160);
  output.append(identity.display_name);
  output.push_back(' ');
  output.append(identity.downstream_version);
  output.append(" | ");
  output.append(identity.unofficial_label);
  output.append(" | ");
  output.append(identity.build_class_label);
  return output;
}

std::string ShortRuntimeLabel()
{ return ShortRuntimeLabel(Current()); }

} // namespace runtime_identity
