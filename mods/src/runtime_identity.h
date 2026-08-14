#pragma once

#include <string>
#include <string_view>

namespace runtime_identity
{

struct Record {
  std::string_view distribution_id;
  std::string_view display_name;
  std::string_view downstream_version;
  std::string_view unofficial_label;
  std::string_view build_class;
  std::string_view build_class_label;
  std::string_view source_state_id;
  std::string_view base_commit;
  std::string_view upstream_base;
  std::string_view test_target;
  std::string_view test_expiry;
  std::string_view support_boundary;
  bool             reproducible;
};

const Record& Current();
std::string   SupportIdentity(const Record& identity);
std::string   SupportIdentity();
std::string   ShortRuntimeLabel(const Record& identity);
std::string   ShortRuntimeLabel();

} // namespace runtime_identity
