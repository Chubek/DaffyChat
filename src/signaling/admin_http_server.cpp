#include "daffy/signaling/admin_http_server.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace daffy::signaling {
namespace {

struct HttpResponse {
  int status_code{200};
  std::string reason{"OK"};
  std::string body{"{}"};
};

std::string SerializeJson(const util::json::Value& value) { return util::json::Serialize(value) + '\n'; }

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

std::string UrlDecode(std::string_view value) {
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] == '+' ) {
      decoded.push_back(' ');
      continue;
    }
    if (value[index] == '%' && index + 2 < value.size()) {
      const auto hi = value[index + 1];
      const auto lo = value[index + 2];
      const auto hex_to_int = [](const char character) -> int {
        if (character >= '0' && character <= '9') {
          return character - '0';
        }
        if (character >= 'a' && character <= 'f') {
          return 10 + character - 'a';
        }
        if (character >= 'A' && character <= 'F') {
          return 10 + character - 'A';
        }
        return -1;
      };
      const auto high = hex_to_int(hi);
      const auto low = hex_to_int(lo);
      if (high >= 0 && low >= 0) {
        decoded.push_back(static_cast<char>((high << 4) | low));
        index += 2;
        continue;
      }
    }
    decoded.push_back(value[index]);
  }
  return decoded;
}

std::pair<std::string, std::unordered_map<std::string, std::string>> SplitTarget(std::string_view target) {
  const auto separator = target.find('?');
  if (separator == std::string_view::npos) {
    return {std::string(target), {}};
  }

  std::unordered_map<std::string, std::string> query;
  std::string_view query_string = target.substr(separator + 1);
  while (!query_string.empty()) {
    const auto ampersand = query_string.find('&');
    const auto part = query_string.substr(0, ampersand);
    const auto equals = part.find('=');
    const auto key = UrlDecode(part.substr(0, equals));
    const auto value = equals == std::string_view::npos ? std::string() : UrlDecode(part.substr(equals + 1));
    if (!key.empty()) {
      query[key] = value;
    }

    if (ampersand == std::string_view::npos) {
      break;
    }
    query_string.remove_prefix(ampersand + 1);
  }

  return {std::string(target.substr(0, separator)), std::move(query)};
}

HttpResponse JsonResponse(const int status_code, std::string reason, const util::json::Value& body) {
  return HttpResponse{status_code, std::move(reason), SerializeJson(body)};
}

HttpResponse ParseErrorResponse(std::string_view code, std::string_view message, int status_code = 400) {
  return JsonResponse(status_code, status_code == 400 ? "Bad Request" : "Error",
                      util::json::Value::Object{{"error", std::string(code)}, {"message", std::string(message)}});
}

HttpResponse RouteRequest(SignalingServer& signaling_server,
                          const config::AppConfig& config,
                          std::string_view method,
                          std::string_view target) {
  if (method != "GET") {
    return JsonResponse(405, "Method Not Allowed",
                        util::json::Value::Object{{"error", "method-not-allowed"}, {"allowed", "GET"}});
  }

  const auto [path, query] = SplitTarget(target);
  if (path == "/" || path.empty()) {
    return JsonResponse(200, "OK",
                        util::json::Value::Object{{"service", "daffy-signaling-admin"},
                                                  {"health", config.signaling.health_endpoint},
                                                  {"rooms", config.signaling.debug_rooms_endpoint},
                                                  {"turn", config.signaling.turn_credentials_endpoint}});
  }
  if (path == config.signaling.health_endpoint) {
    return JsonResponse(200, "OK", signaling_server.HealthToJson());
  }
  if (path == config.signaling.debug_rooms_endpoint) {
    return JsonResponse(200, "OK", signaling_server.DebugStateToJson());
  }
  if (path == config.signaling.turn_credentials_endpoint) {
    const auto room_it = query.find("room");
    const auto peer_it = query.find("peer_id");
    if (room_it == query.end() || room_it->second.empty() || peer_it == query.end() || peer_it->second.empty()) {
      return ParseErrorResponse("missing-query", "TURN credentials require room and peer_id query parameters");
    }

    const auto credentials = signaling_server.IssueTurnCredentials(room_it->second, peer_it->second);
    if (!credentials.ok()) {
      return JsonResponse(503, "Service Unavailable",
                          util::json::Value::Object{{"error", "turn-unavailable"},
                                                    {"message", credentials.error().ToString()}});
    }
    return JsonResponse(200, "OK", TurnCredentialsToJson(credentials.value()));
  }

  return JsonResponse(404, "Not Found",
                      util::json::Value::Object{{"error", "not-found"}, {"path", path}});
}

core::Result<int> CreateListenSocket(const config::AppConfig& config, int* bound_port) {
  struct addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  addrinfo* results = nullptr;
  const std::string port = std::to_string(config.signaling.port);
  const char* host = config.signaling.bind_address.empty() ? nullptr : config.signaling.bind_address.c_str();
  const int lookup = getaddrinfo(host, port.c_str(), &hints, &results);
  if (lookup != 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Failed to resolve signaling bind address"};
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
          *bound_port = config.signaling.port;
        }
      }
      freeaddrinfo(results);
      return listen_fd;
    }

    close(listen_fd);
    listen_fd = -1;
  }

  freeaddrinfo(results);
  return core::Error{core::ErrorCode::kUnavailable, "Failed to bind signaling admin HTTP listener"};
}

std::string ReadRequest(const int fd) {
  timeval timeout {};
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
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

HttpResponse HandleRequest(SignalingServer& signaling_server, const config::AppConfig& config, const std::string& request) {
  const auto line_end = request.find("\r\n");
  if (line_end == std::string::npos) {
    return ParseErrorResponse("malformed-request", "Expected an HTTP request line");
  }

  const std::string_view request_line(request.data(), line_end);
  const auto first_space = request_line.find(' ');
  const auto second_space = request_line.find(' ', first_space == std::string_view::npos ? 0 : first_space + 1);
  if (first_space == std::string_view::npos || second_space == std::string_view::npos) {
    return ParseErrorResponse("malformed-request", "Expected METHOD TARGET HTTP/VERSION");
  }

  const auto method = request_line.substr(0, first_space);
  const auto target = request_line.substr(first_space + 1, second_space - first_space - 1);
  return RouteRequest(signaling_server, config, method, target);
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

SignalingAdminHttpServer::SignalingAdminHttpServer(config::AppConfig config,
                                                   SignalingServer& signaling_server,
                                                   core::Logger logger)
    : config_(std::move(config)), signaling_server_(signaling_server), logger_(std::move(logger)) {}

SignalingAdminHttpServer::~SignalingAdminHttpServer() { Stop(); }

core::Result<int> SignalingAdminHttpServer::Start() {
  if (running_.load()) {
    return bound_port_;
  }

  int bound_port = 0;
  auto listen_fd = CreateListenSocket(config_, &bound_port);
  if (!listen_fd.ok()) {
    return listen_fd.error();
  }

  listen_fd_ = listen_fd.value();
  bound_port_ = bound_port;
  running_.store(true);
  worker_ = std::thread([this]() { AcceptLoop(); });

  logger_.Info("Started signaling admin HTTP server on " + config_.signaling.bind_address + ':' +
               std::to_string(bound_port_));
  return bound_port_;
}

void SignalingAdminHttpServer::Stop() {
  if (!running_.exchange(false)) {
    return;
  }

  const int fd = listen_fd_;
  listen_fd_ = -1;
  if (fd >= 0) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
  }
  if (worker_.joinable()) {
    worker_.join();
  }

  logger_.Info("Stopped signaling admin HTTP server");
}

bool SignalingAdminHttpServer::is_running() const { return running_.load(); }

int SignalingAdminHttpServer::bound_port() const { return bound_port_; }

void SignalingAdminHttpServer::AcceptLoop() {
  while (running_.load()) {
    pollfd descriptor {};
    descriptor.fd = listen_fd_;
    descriptor.events = POLLIN;

    const int poll_result = poll(&descriptor, 1, 200);
    if (!running_.load()) {
      break;
    }
    if (poll_result <= 0 || (descriptor.revents & POLLIN) == 0) {
      continue;
    }

    const int client_fd = accept(listen_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (running_.load()) {
        logger_.Warn("Failed to accept signaling admin HTTP connection");
      }
      continue;
    }

    const auto request = ReadRequest(client_fd);
    const auto response = HandleRequest(signaling_server_, config_, request);
    WriteResponse(client_fd, response);
    close(client_fd);
  }
}

}  // namespace daffy::signaling
