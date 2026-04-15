#include "daffy/signaling/uwebsockets_server.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "App.h"
#include "daffy/core/id.hpp"
#include "daffy/signaling/messages.hpp"

namespace daffy::signaling {
namespace {

struct SocketUserData {
  std::string connection_id;
  std::string remote_address;
  std::string user_agent;
  bool browser_client{false};
};

bool LooksLikeBrowser(std::string_view user_agent) {
  return user_agent.find("Mozilla/") != std::string_view::npos ||
         user_agent.find("Chrome/") != std::string_view::npos ||
         user_agent.find("Safari/") != std::string_view::npos ||
         user_agent.find("Firefox/") != std::string_view::npos ||
         user_agent.find("Edg/") != std::string_view::npos;
}

void WriteJsonResponse(auto* response, const int status_code, std::string_view status_text, const util::json::Value& body) {
  const std::string payload = util::json::Serialize(body);
  response->writeStatus(std::to_string(status_code) + " " + std::string(status_text))
      ->writeHeader("Content-Type", "application/json")
      ->end(payload);
}

}  // namespace

UWebSocketsSignalingServer::UWebSocketsSignalingServer(config::AppConfig config,
                                                       SignalingServer& signaling_server,
                                                       core::Logger logger)
    : config_(std::move(config)), signaling_server_(signaling_server), logger_(std::move(logger)) {}

core::Status UWebSocketsSignalingServer::Run() {
  if (!signaling_server_.HasUWebSocketsRuntimeDependencies()) {
    return core::Error{core::ErrorCode::kUnavailable, "Vendored uWebSockets runtime dependencies are missing"};
  }

  std::unordered_map<std::string, uWS::WebSocket<false, true, SocketUserData>*> sockets;
  bool listen_success = false;
  int bound_port = config_.signaling.port;

  auto dispatch = [&](const std::vector<OutboundEnvelope>& outgoing) {
    for (const auto& envelope : outgoing) {
      const auto socket_it = sockets.find(envelope.connection_id);
      if (socket_it == sockets.end()) {
        continue;
      }
      const std::string payload = SerializeMessage(envelope.message);
      const auto status = socket_it->second->send(payload, uWS::OpCode::TEXT);
      if (status == uWS::WebSocket<false, true, SocketUserData>::SendStatus::DROPPED) {
        logger_.Warn("Dropped signaling message for connection " + envelope.connection_id);
      }
    }
  };

  uWS::App app;
  app.ws<SocketUserData>("/*",
                         {.compression = uWS::CompressOptions::DISABLED,
                          .maxPayloadLength = 1024 * 1024,
                          .idleTimeout = static_cast<unsigned short>(
                              std::max(8, config_.signaling.ping_timeout_ms / 1000)),
                          .maxBackpressure = 1024 * 1024,
                          .closeOnBackpressureLimit = true,
                          .resetIdleTimeoutOnSend = true,
                          .sendPingsAutomatically = true,
                          .upgrade =
                              [this](auto* response, auto* request, auto* context) {
                                SocketUserData data;
                                data.connection_id = core::GenerateId("conn");
                                data.remote_address = std::string(response->getRemoteAddressAsText());
                                data.user_agent = std::string(request->getHeader("user-agent"));
                                data.browser_client =
                                    request->getQuery("browser") == "1" || LooksLikeBrowser(data.user_agent);
                                response->template upgrade<SocketUserData>(
                                    std::move(data), request->getHeader("sec-websocket-key"),
                                    request->getHeader("sec-websocket-protocol"),
                                    request->getHeader("sec-websocket-extensions"), context);
                              },
                          .open =
                              [this, &sockets](auto* socket) {
                                const auto* data = socket->getUserData();
                                sockets[data->connection_id] = socket;
                                signaling_server_.OpenConnection(
                                    ConnectionContext{data->connection_id, data->remote_address, data->user_agent,
                                                      data->browser_client});
                              },
                          .message =
                              [this, &dispatch](auto* socket, std::string_view message, uWS::OpCode) {
                                const auto result =
                                    signaling_server_.HandleMessage(socket->getUserData()->connection_id, message);
                                dispatch(result.outgoing);
                              },
                          .close =
                              [this, &sockets, &dispatch](auto* socket, int, std::string_view) {
                                const std::string connection_id = socket->getUserData()->connection_id;
                                sockets.erase(connection_id);
                                dispatch(signaling_server_.CloseConnection(connection_id));
                              }});
  app.get("/", [this](auto* response, auto*) {
        WriteJsonResponse(response, 200, "OK",
                          util::json::Value::Object{{"service", "daffy-signaling"},
                                                    {"transport", "uwebsockets"},
                                                    {"health", config_.signaling.health_endpoint},
                                                    {"rooms", config_.signaling.debug_rooms_endpoint},
                                                    {"turn", config_.signaling.turn_credentials_endpoint}});
      })
      .get(config_.signaling.health_endpoint,
           [this](auto* response, auto*) { WriteJsonResponse(response, 200, "OK", signaling_server_.HealthToJson()); })
      .get(config_.signaling.debug_rooms_endpoint, [this](auto* response, auto*) {
        WriteJsonResponse(response, 200, "OK", signaling_server_.DebugStateToJson());
      })
      .get(config_.signaling.turn_credentials_endpoint, [this](auto* response, auto* request) {
        const std::string room = std::string(request->getQuery("room"));
        const std::string peer_id = std::string(request->getQuery("peer_id"));
        if (room.empty() || peer_id.empty()) {
          WriteJsonResponse(response, 400, "Bad Request",
                            util::json::Value::Object{{"error", "missing-query"},
                                                      {"message",
                                                       "TURN credentials require room and peer_id query parameters"}});
          return;
        }

        const auto credentials = signaling_server_.IssueTurnCredentials(room, peer_id);
        if (!credentials.ok()) {
          WriteJsonResponse(response, 503, "Service Unavailable",
                            util::json::Value::Object{{"error", "turn-unavailable"},
                                                      {"message", credentials.error().ToString()}});
          return;
        }
        WriteJsonResponse(response, 200, "OK", TurnCredentialsToJson(credentials.value()));
      })
      .any("/*", [](auto* response, auto* request) {
        WriteJsonResponse(response, 404, "Not Found",
                          util::json::Value::Object{{"error", "not-found"},
                                                    {"path", std::string(request->getUrl())}});
      })
      .listen(config_.signaling.bind_address, config_.signaling.port, [&listen_success, &bound_port, this](auto* token) {
        if (!token) {
          logger_.Error("Failed to bind uWebSockets signaling server");
          return;
        }
        listen_success = true;
        bound_port = us_socket_local_port(0, reinterpret_cast<us_socket_t*>(token));
        logger_.Info("Listening for signaling on " + config_.signaling.bind_address + ':' + std::to_string(bound_port));
      });

  if (!listen_success) {
    return core::Error{core::ErrorCode::kUnavailable, "Failed to bind uWebSockets signaling server"};
  }
  app.run();
  return core::OkStatus();
}

}  // namespace daffy::signaling
