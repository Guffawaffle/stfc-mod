/**
 * @file sync_payload_builders.h
 * @brief Sync payload parsing, delta tracking, and entity-group dispatch.
 */
#pragma once

#include <memory>
#include <string>

class EntityGroup;
class RealtimeDataPayload;
class ServiceResponse;

void HandleEntityGroup(EntityGroup* entity_group);
void HandleServiceResponseEntityGroups(ServiceResponse* service_response);
void HandleRealtimeDataPayload(RealtimeDataPayload* data);
