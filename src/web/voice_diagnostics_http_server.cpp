#include "daffy/web/voice_diagnostics_http_server.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace daffy::web {

namespace {

struct HttpResponse {
  int status_code{200};
  std::string reason{"OK"};
  std::string body{"{}"};
};

core::Result<int> CreateListenSocket(const config::AppConfig& config, int* bound_port) {
  struct addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  addrinfo* results = nullptr;
  const std::string port = std::to_string(config.server.port);
  const char* host = config.server.bind_address.empty() ? nullptr : config.server.bind_address.c_str();
  const int lookup = getaddrinfo(host, port.c_str(), &hints, &results);
  if (lookup != 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Failed to resolve backend bind address"};
  }

  int listen_fd = -1;
  for (addrinfo* current = results; current != nullptr; current = current->ai_next) {
    listen_fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
    if (listen_fd < 0) {
      continue;
    }

    const int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(listen_fd, current->ai_addr, current->ai_addrlen) == 0 && listen(listen_fd, 16) == 0) {
      sockaddr_storage bound_address {};
      socklen_t bound_length = sizeof(bound_address);
      if (getsockname(listen_fd, reinterpret_cast<sockaddr*>(&bound_address), &bound_length) == 0) {
        if (bound_address.ss_family == AF_INET) {
          *bound_port = ntohs(reinterpret_cast<sockaddr_in*>(&bound_address)->sin_port);
        } else if (bound_address.ss_family == AF_INET6) {
          *bound_port = ntohs(reinterpret_cast<sockaddr_in6*>(&bound_address)->sin6_port);
        } else {
          *bound_port = config.server.port;
        }
      }
      freeaddrinfo(results);
      return listen_fd;
    }

    close(listen_fd);
    listen_fd = -1;
  }

  freeaddrinfo(results);
  return core::Error{core::ErrorCode::kUnavailable, "Failed to bind backend diagnostics listener"};
}

bool SendAll(const int fd, std::string_view payload) {
  std::size_t sent = 0;
  while (sent < payload.size()) {
    const auto bytes = send(fd, payload.data() + sent, payload.size() - sent, 0);
    if (bytes <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(bytes);
  }
  return true;
}

std::string ReadRequest(const int fd) {
  timeval timeout {};
  timeout.tv_sec = 1;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  std::string request;
  request.reserve(1024);
  char buffer[1024];
  while (request.find("\r\n\r\n") == std::string::npos && request.size() < 8192) {
    const auto bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
      break;
    }
    request.append(buffer, buffer + bytes);
  }
  return request;
}

HttpResponse JsonResponse(const int status_code, std::string reason, const util::json::Value& body) {
  return HttpResponse{status_code, std::move(reason), util::json::Serialize(body) + '\n'};
}

void WriteResponse(const int fd, const HttpResponse& response) {
  std::string headers = "HTTP/1.1 " + std::to_string(response.status_code) + ' ' + response.reason + "\r\n";
  headers += "Content-Type: application/json\r\n";
  headers += "Access-Control-Allow-Origin: *\r\n";
  headers += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
  headers += "Connection: close\r\n\r\n";
  SendAll(fd, headers);
  SendAll(fd, response.body);
}

}  // namespace

struct VoiceDiagnosticsHttpServer::Impl {
  Impl(config::AppConfig config_in, core::Logger logger_in, SnapshotProvider provider_in)
      : config(std::move(config_in)), logger(std::move(logger_in)), provider(std::move(provider_in)) {}

  config::AppConfig config;
  core::Logger logger;
  SnapshotProvider provider;
  std::atomic<bool> running{false};
  int listen_fd{-1};
  int bound_port{0};
  std::thread accept_thread;

  HttpResponse HandleRequest(const std::string& request) const {
    const auto line_end = request.find("\r\n");
    if (line_end == std::string::npos) {
      return JsonResponse(400, "Bad Request", util::json::Value::Object{{"error", "malformed-request"}});
    }

    const std::string_view request_line(request.data(), line_end);
    const auto first_space = request_line.find(' ');
    const auto second_space = request_line.find(' ', first_space == std::string_view::npos ? 0 : first_space + 1);
    if (first_space == std::string_view::npos || second_space == std::string_view::npos) {
      return JsonResponse(400, "Bad Request", util::json::Value::Object{{"error", "malformed-request"}});
    }

    const auto method = request_line.substr(0, first_space);
    const auto target = request_line.substr(first_space + 1, second_space - first_space - 1);
    if (method != "GET") {
      return JsonResponse(405, "Method Not Allowed",
                          util::json::Value::Object{{"error", "method-not-allowed"}});
    }

    if (target == "/" || target.empty()) {
      return JsonResponse(200,
                          "OK",
                          util::json::Value::Object{{"service", "daffy-backend-voice-diagnostics"},
                                                    {"health", config.signaling.health_endpoint},
                                                    {"voice_bridge", config.frontend_bridge.bridge_endpoint}});
    }
    if (target == config.signaling.health_endpoint) {
      return JsonResponse(200,
                          "OK",
                          util::json::Value::Object{{"service", "daffy-backend-voice-diagnostics"},
                                                    {"status", "ok"},
                                                    {"voice_transport", config.frontend_bridge.voice_transport},
                                                    {"bridge_enabled", config.frontend_bridge.enabled}});
    }
    if (target == config.frontend_bridge.bridge_endpoint) {
      return JsonResponse(200, "OK", provider());
    }
    return JsonResponse(404, "Not Found", util::json::Value::Object{{"error", "not-found"}});
  }

  void AcceptLoop() {
    while (running.load()) {
      pollfd descriptor {};
      descriptor.fd = listen_fd;
      descriptor.events = POLLIN;
      const auto ready = poll(&descriptor, 1, 200);
      if (ready <= 0) {
        continue;
      }

      sockaddr_storage remote_address {};
      socklen_t remote_length = sizeof(remote_address);
      const auto client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&remote_address), &remote_length);
      if (client_fd < 0) {
        if (running.load()) {
          logger.Warn("Failed to accept backend diagnostics HTTP connection");
        }
        continue;
      }

      const auto request = ReadRequest(client_fd);
      const auto response = HandleRequest(request);
      WriteResponse(client_fd, response);
      close(client_fd);
    }
  }
};

VoiceDiagnosticsHttpServer::VoiceDiagnosticsHttpServer(config::AppConfig config,
                                                       core::Logger logger,
                                                       SnapshotProvider provider)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(logger), std::move(provider))) {}

VoiceDiagnosticsHttpServer::~VoiceDiagnosticsHttpServer() { Stop(); }

core::Result<int> VoiceDiagnosticsHttpServer::Start() {
  if (impl_->running.load()) {
    return impl_->bound_port;
  }

  auto listen_fd = CreateListenSocket(impl_->config, &impl_->bound_port);
  if (!listen_fd.ok()) {
    return listen_fd.error();
  }

  impl_->listen_fd = listen_fd.value();
  impl_->running.store(true);
  impl_->accept_thread = std::thread([this]() { impl_->AcceptLoop(); });
  impl_->logger.Info("Started voice diagnostics HTTP server on " + impl_->config.server.bind_address + ':' +
                     std::to_string(impl_->bound_port));
  return impl_->bound_port;
}

void VoiceDiagnosticsHttpServer::Stop() {
  if (!impl_ || !impl_->running.exchange(false)) {
    return;
  }
  if (impl_->listen_fd >= 0) {
    close(impl_->listen_fd);
    impl_->listen_fd = -1;
  }
  if (impl_->accept_thread.joinable()) {
    impl_->accept_thread.join();
  }
  impl_->logger.Info("Stopped voice diagnostics HTTP server");
}

util::json::Value BuildNativeVoiceDiagnosticsPayload(const voice::NativeVoiceClientStateSnapshot& state,
                                                     const voice::NativeVoiceClientTelemetry& telemetry,
                                                     const std::string_view voice_transport) {
  return util::json::Value::Object{{"voice_client_state", voice::NativeVoiceClientStateToJson(state)},
                                   {"voice_client_telemetry", voice::NativeVoiceClientTelemetryToJson(telemetry)},
                                   {"voice_transport", std::string(voice_transport)}};
}

}  // namespace daffy::web
