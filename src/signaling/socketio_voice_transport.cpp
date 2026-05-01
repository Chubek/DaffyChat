#include "daffy/signaling/socketio_voice_transport.hpp"

#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>

#include "daffy/core/time.hpp"
#include "daffy/util/json.hpp"

namespace daffy::signaling {
namespace {

std::int64_t NowUnixMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

SocketIOVoiceTransport::SocketIOVoiceTransport(const SocketIOVoiceTransportConfig& config,
                                               SignalingServer& signaling_server,
                                               std::shared_ptr<core::Logger> logger)
    : config_(config), signaling_server_(signaling_server), logger_(std::move(logger)) {}

SocketIOVoiceTransport::~SocketIOVoiceTransport() { Stop(); }

core::Result<core::Status> SocketIOVoiceTransport::Start() {
  if (running_.load()) {
    return core::Error{core::ErrorCode::kStateError, "Socket.IO voice transport is already running"};
  }

  logger_->Info("Starting Socket.IO voice transport on " + config_.bind_address + ":" + std::to_string(config_.port));

  running_.store(true);
  stop_requested_.store(false);

  logger_->Info("Socket.IO voice transport started successfully");
  return core::OkStatus();
}

void SocketIOVoiceTransport::Stop() {
  if (!running_.load()) {
    return;
  }

  logger_->Info("Stopping Socket.IO voice transport");
  stop_requested_.store(true);

  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.clear();
  }

  running_.store(false);
  logger_->Info("Socket.IO voice transport stopped");
}

bool SocketIOVoiceTransport::IsRunning() const { return running_.load(); }

void SocketIOVoiceTransport::HandleConnect(const std::string& socket_id, const std::string& peer_id) {
  std::lock_guard<std::mutex> lock(sessions_mutex_);

  const std::string connection_id = CreateConnectionId(socket_id);
  const std::int64_t now_ms = NowUnixMillis();

  ClientSession session;
  session.connection_id = connection_id;
  session.peer_id = peer_id;
  session.socket_id = socket_id;
  session.created_at_ms = now_ms;
  session.last_seen_ms = now_ms;

  sessions_[socket_id] = std::move(session);

  logger_->Info("Socket.IO client connected: socket_id=" + socket_id + " peer_id=" + peer_id + 
                " connection_id=" + connection_id);

  ConnectionContext context;
  context.connection_id = connection_id;
  context.remote_address = "socketio";
  context.user_agent = "socketio-client";
  context.browser_client = true;

  signaling_server_.OpenConnection(context);

  util::json::Value response = util::json::Value::Object{{"connection_id", connection_id}, {"peer_id", peer_id}};

  BroadcastToSocket(socket_id, "connected", util::json::Serialize(response));
}

void SocketIOVoiceTransport::HandleDisconnect(const std::string& socket_id) {
  std::lock_guard<std::mutex> lock(sessions_mutex_);

  const auto it = sessions_.find(socket_id);
  if (it == sessions_.end()) {
    logger_->Warn("Socket.IO disconnect for unknown socket_id=" + socket_id);
    return;
  }

  const std::string connection_id = it->second.connection_id;
  logger_->Info("Socket.IO client disconnected: socket_id=" + socket_id + " connection_id=" + connection_id);

  signaling_server_.CloseConnection(connection_id);
  sessions_.erase(it);
}

void SocketIOVoiceTransport::HandleSignalMessage(const std::string& socket_id, const std::string& message) {
  ClientSession* session = nullptr;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    session = FindSessionBySocketId(socket_id);
    if (!session) {
      logger_->Warn("Received signal message from unknown socket_id=" + socket_id);
      return;
    }
    session->last_seen_ms = NowUnixMillis();
  }

  logger_->Debug("Socket.IO signal message from socket_id=" + socket_id + ": " + message);

  const auto dispatch_result = signaling_server_.HandleMessage(session->connection_id, message);

  if (!dispatch_result.accepted) {
    logger_->Warn("Signaling server rejected message from socket_id=" + socket_id);
    return;
  }

  for (const auto& envelope : dispatch_result.outgoing) {
    if (envelope.connection_id == session->connection_id) {
      const std::string serialized = util::json::Serialize(MessageToJson(envelope.message));
      BroadcastToSocket(socket_id, "signal", serialized);
    }
  }
}

void SocketIOVoiceTransport::PollAndSendEvents(const std::string& socket_id) {
  ClientSession* session = nullptr;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    session = FindSessionBySocketId(socket_id);
    if (!session) {
      return;
    }
  }

  // Note: SignalingServer doesn't have a PollEvents method
  // Events are returned directly from HandleMessage
  // This is a placeholder for future event polling implementation
}

std::string SocketIOVoiceTransport::CreateConnectionId(const std::string& socket_id) {
  std::ostringstream oss;
  oss << "socketio-" << socket_id;
  return oss.str();
}

SocketIOVoiceTransport::ClientSession* SocketIOVoiceTransport::FindSessionBySocketId(const std::string& socket_id) {
  const auto it = sessions_.find(socket_id);
  if (it == sessions_.end()) {
    return nullptr;
  }
  return &it->second;
}

SocketIOVoiceTransport::ClientSession* SocketIOVoiceTransport::FindSessionByConnectionId(
    const std::string& connection_id) {
  for (auto& [socket_id, session] : sessions_) {
    if (session.connection_id == connection_id) {
      return &session;
    }
  }
  return nullptr;
}

void SocketIOVoiceTransport::RunEventLoop() {
  while (!stop_requested_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<std::string> socket_ids;
    {
      std::lock_guard<std::mutex> lock(sessions_mutex_);
      for (const auto& [socket_id, session] : sessions_) {
        socket_ids.push_back(socket_id);
      }
    }

    for (const auto& socket_id : socket_ids) {
      PollAndSendEvents(socket_id);
    }
  }
}

void SocketIOVoiceTransport::BroadcastToSocket(const std::string& socket_id,
                                               const std::string& event_name,
                                               const std::string& payload) {
  logger_->Debug("Broadcasting to socket_id=" + socket_id + " event=" + event_name + " payload=" + payload);
  // Actual Socket.IO broadcast implementation would go here
  // This is a placeholder that logs the broadcast intent
}

}  // namespace daffy::signaling
