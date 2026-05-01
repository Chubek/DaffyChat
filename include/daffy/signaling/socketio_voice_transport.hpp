#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

#include "daffy/config/app_config.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/signaling/server.hpp"

namespace daffy::signaling {

struct SocketIOVoiceTransportConfig {
  std::string bind_address{"0.0.0.0"};
  int port{7002};
  bool enabled{false};
};

class SocketIOVoiceTransport {
 public:
  SocketIOVoiceTransport(const SocketIOVoiceTransportConfig& config,
                         SignalingServer& signaling_server,
                         std::shared_ptr<core::Logger> logger);
  ~SocketIOVoiceTransport();

  SocketIOVoiceTransport(const SocketIOVoiceTransport&) = delete;
  SocketIOVoiceTransport& operator=(const SocketIOVoiceTransport&) = delete;

  core::Result<void> Start();
  void Stop();
  bool IsRunning() const;

 private:
  struct ClientSession {
    std::string connection_id;
    std::string peer_id;
    std::string socket_id;
    std::int64_t created_at_ms{0};
    std::int64_t last_seen_ms{0};
  };

  void HandleConnect(const std::string& socket_id, const std::string& peer_id);
  void HandleDisconnect(const std::string& socket_id);
  void HandleSignalMessage(const std::string& socket_id, const std::string& message);
  void PollAndSendEvents(const std::string& socket_id);

  std::string CreateConnectionId(const std::string& socket_id);
  ClientSession* FindSessionBySocketId(const std::string& socket_id);
  ClientSession* FindSessionByConnectionId(const std::string& connection_id);

  void RunEventLoop();
  void BroadcastToSocket(const std::string& socket_id, const std::string& event_name, const std::string& payload);

  SocketIOVoiceTransportConfig config_;
  SignalingServer& signaling_server_;
  std::shared_ptr<core::Logger> logger_;

  std::mutex sessions_mutex_;
  std::unordered_map<std::string, ClientSession> sessions_;

  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  void* server_impl_{nullptr};
};

}  // namespace daffy::signaling
