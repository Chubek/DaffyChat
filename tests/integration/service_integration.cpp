#include <cassert>
#include <iostream>
#include <thread>
#include <filesystem>
#include <chrono>
#include <unistd.h>

#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/health_service.hpp"
#include "daffy/services/echo_service.hpp"
#include "daffy/runtime/daemon_manager.hpp"

int main() {
  std::cout << "Testing service integration...\n";

  const std::string run_dir = "/tmp/daffy-service-integration-" + std::to_string(getpid());
  
  // Create run directory
  std::filesystem::create_directories(run_dir);
  
  // Test 1: Health service
  std::cout << "  Test 1: Health service\n";
  daffy::ipc::NngRequestReplyTransport health_transport;
  daffy::services::HealthService health_service;
  const std::string health_url = "ipc://" + run_dir + "/health.sock";
  
  auto health_bind = health_service.Bind(health_transport, health_url);
  assert(health_bind.ok());
  
  // Give server time to bind
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  // Test Status RPC
  auto status_reply = health_transport.Request(
      health_url,
      daffy::ipc::MessageEnvelope{
          "service.health",
          "request",
          daffy::util::json::Value::Object{{"rpc", "Status"}}
      });
  assert(status_reply.ok());
  assert(status_reply.value().topic == "service.health");
  assert(status_reply.value().type == "reply");
  
  const auto* status_field = status_reply.value().payload.Find("status");
  assert(status_field != nullptr);
  assert(status_field->AsString() == "ok");
  std::cout << "    Health Status RPC passed\n";
  
  // Test Ping RPC
  auto ping_reply = health_transport.Request(
      health_url,
      daffy::ipc::MessageEnvelope{
          "service.health",
          "request",
          daffy::util::json::Value::Object{{"rpc", "Ping"}}
      });
  assert(ping_reply.ok());
  
  const auto* reply_field = ping_reply.value().payload.Find("reply");
  assert(reply_field != nullptr);
  assert(reply_field->AsString() == "pong");
  std::cout << "    Health Ping RPC passed\n";

  // Test 2: Echo service
  std::cout << "  Test 2: Echo service\n";
  daffy::ipc::NngRequestReplyTransport echo_transport;
  daffy::services::EchoService echo_service;
  const std::string echo_url = "ipc://" + run_dir + "/echo.sock";
  
  auto echo_bind = echo_service.Bind(echo_transport, echo_url);
  assert(echo_bind.ok());
  
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  auto echo_reply = echo_transport.Request(
      echo_url,
      daffy::ipc::MessageEnvelope{
          "service.echo",
          "request",
          daffy::services::EchoRequestToJson({"Hello World", "TestUser"})
      });
  assert(echo_reply.ok());
  
  const auto* echoed = echo_reply.value().payload.Find("echoed");
  assert(echoed != nullptr);
  assert(echoed->AsBool() == true);
  
  const auto* sender = echo_reply.value().payload.Find("sender");
  assert(sender != nullptr);
  assert(sender->AsString() == "TestUser");
  std::cout << "    Echo service passed\n";

  // Test 3: Daemon manager
  std::cout << "  Test 3: Daemon manager\n";
  daffy::runtime::DaemonManager daemon_manager(run_dir);
  
  // Register a service
  daffy::runtime::ManagedService managed_service;
  managed_service.metadata = daffy::services::HealthService::Metadata();
  managed_service.service_url = health_url;
  managed_service.pid = 0;
  managed_service.state = "registered";
  managed_service.auto_restart = false;
  
  auto register_status = daemon_manager.RegisterService(managed_service);
  assert(register_status.ok());
  std::cout << "    Registered service with daemon manager\n";
  
  // List services
  auto services = daemon_manager.ListServices();
  assert(services.size() == 1);
  assert(services[0].metadata.name == "health");
  std::cout << "    Listed " << services.size() << " services\n";
  
  // Find service
  auto found = daemon_manager.FindService("health");
  assert(found.ok());
  assert(found.value().metadata.name == "health");
  std::cout << "    Found service by name\n";
  
  // Test 4: Service brokering through daemon manager
  std::cout << "  Test 4: Service brokering\n";
  
  // Update service with running PID (fake for test)
  auto update_status = daemon_manager.UpdateServiceState("health", "running");
  assert(update_status.ok());
  
  // Note: BrokerRequest would need the service to be actually running
  // For this test, we just verify the method exists and handles errors correctly
  auto broker_result = daemon_manager.BrokerRequest(
      "nonexistent-service",
      daffy::ipc::MessageEnvelope{"test", "request", daffy::util::json::Value::Object{}}
  );
  assert(!broker_result.ok());
  std::cout << "    Broker correctly handles missing service\n";

  std::cout << "All service integration tests passed!\n";
  return 0;
}
