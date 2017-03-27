/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "chre/platform/shared/host_protocol_chre.h"

#include <inttypes.h>
#include <string.h>

#include "chre/platform/log.h"
#include "chre/platform/shared/host_messages_generated.h"

using flatbuffers::FlatBufferBuilder;
using flatbuffers::Offset;
using flatbuffers::Vector;

namespace chre {

bool HostProtocolChre::decodeMessageFromHost(const void *message,
                                             size_t messageLen) {
  bool success = verifyMessage(message, messageLen);
  if (!success) {
    LOGE("Dropping invalid/corrupted message from host (length %zu)",
         messageLen);
  } else {
    const fbs::MessageContainer *container = fbs::GetMessageContainer(message);

    switch (container->message_type()) {
      case fbs::ChreMessage::NanoappMessage: {
        const auto *nanoappMsg = static_cast<const fbs::NanoappMessage *>(
            container->message());
        // Required field; verifier ensures that this is not null (though it
        // may be empty)
        const flatbuffers::Vector<uint8_t> *msgData = nanoappMsg->message();
        HostMessageHandlers::handleNanoappMessage(
            nanoappMsg->app_id(), nanoappMsg->message_type(),
            nanoappMsg->host_endpoint(), msgData->data(), msgData->size());
        break;
      }

      case fbs::ChreMessage::HubInfoRequest:
        HostMessageHandlers::handleHubInfoRequest();
        break;

      case fbs::ChreMessage::NanoappListRequest:
        HostMessageHandlers::handleNanoappListRequest();
        break;

      default:
        LOGW("Got invalid/unexpected message type %" PRIu8,
             static_cast<uint8_t>(container->message_type()));
        success = false;
    }
  }

  return success;
}

void HostProtocolChre::encodeHubInfoResponse(
    FlatBufferBuilder& builder, const char *name, const char *vendor,
    const char *toolchain, uint32_t legacyPlatformVersion,
    uint32_t legacyToolchainVersion, float peakMips, float stoppedPower,
    float sleepPower, float peakPower, uint32_t maxMessageLen,
    uint64_t platformId, uint32_t version) {
  auto nameOffset = addStringAsByteVector(builder, name);
  auto vendorOffset = addStringAsByteVector(builder, vendor);
  auto toolchainOffset = addStringAsByteVector(builder, toolchain);

  auto response = fbs::CreateHubInfoResponse(
      builder, nameOffset, vendorOffset, toolchainOffset, legacyPlatformVersion,
      legacyToolchainVersion, peakMips, stoppedPower, sleepPower, peakPower,
      maxMessageLen, platformId, version);
  auto container = fbs::CreateMessageContainer(
      builder, fbs::ChreMessage::HubInfoResponse, response.Union());
  builder.Finish(container);
}

void HostProtocolChre::addNanoappListEntry(
    FlatBufferBuilder& builder,
    DynamicVector<Offset<fbs::NanoappListEntry>>& offsetVector,
    uint64_t appId, uint32_t appVersion, bool enabled, bool isSystemNanoapp) {
  auto offset = fbs::CreateNanoappListEntry(
      builder, appId, appVersion, enabled, isSystemNanoapp);
  if (!offsetVector.push_back(offset)) {
    LOGE("Couldn't push nanoapp list entry offset!");
  }
}

void HostProtocolChre::finishNanoappListResponse(
    FlatBufferBuilder& builder,
    DynamicVector<Offset<fbs::NanoappListEntry>>& offsetVector) {
  auto vectorOffset = builder.CreateVector<Offset<fbs::NanoappListEntry>>(
      offsetVector);
  auto response = fbs::CreateNanoappListResponse(builder, vectorOffset);
  auto container = fbs::CreateMessageContainer(
      builder, fbs::ChreMessage::NanoappListResponse, response.Union());
  builder.Finish(container);
}

}  // namespace chre