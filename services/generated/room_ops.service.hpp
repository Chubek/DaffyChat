#pragma once
#include <string>

#include "daffy/core/error.hpp"
#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/service_metadata.hpp"
#include "room_ops.generated.hpp"

namespace room_ops {

class RoomopsGeneratedService {
public:
    static daffy::services::ServiceMetadata Metadata();
    daffy::core::Result<daffy::ipc::MessageEnvelope> Handle(const daffy::ipc::MessageEnvelope& request) const;
    daffy::core::Status Bind(daffy::ipc::NngRequestReplyTransport& transport, std::string url) const;
};

} // namespace room_ops
