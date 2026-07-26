/**
 * @file notification_policy.h
 * @brief Load-time notification delivery policy matrix.
 */
#pragma once

#include "patches/notification_catalog.h"

#include <cstddef>
#include <optional>
#include <string_view>

#include <toml++/toml.h>

class NotificationConfig;

struct NotificationPolicy {
  bool              system = false;
  bool              audio  = false;
  NotificationSound sound  = NotificationSound::Default;
};

void notification_policy_load(const toml::table& config, toml::table& runtime_config,
                              const NotificationConfig& legacy_notifications);
void notification_policy_prepare_generated_config(toml::table& user_config);
void notification_policy_write_runtime_snapshot(toml::table& runtime_config);

[[nodiscard]] const NotificationPolicy& notification_policy_for(NotificationKind kind);
[[nodiscard]] bool                      notification_policy_has_delivery(NotificationKind kind);
[[nodiscard]] bool                      notification_policy_system_enabled(NotificationKind kind);
[[nodiscard]] bool                      notification_policy_audio_enabled(NotificationKind kind);
[[nodiscard]] bool                      notification_policy_any_system_enabled();
[[nodiscard]] bool                      notification_policy_any_audio_enabled();
[[nodiscard]] bool notification_policy_delivery_equivalent(NotificationKind left, NotificationKind right);
[[nodiscard]] std::optional<NotificationKind>  notification_kind_from_toast_state(int state);
[[nodiscard]] const char*                      notification_kind_name(NotificationKind kind);
[[nodiscard]] const char*                      notification_canonical_key(NotificationKind kind);
[[nodiscard]] const char*                      notification_sound_name(NotificationSound sound);
[[nodiscard]] std::optional<NotificationSound> notification_sound_from_name(std::string_view name);
