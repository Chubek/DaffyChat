#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

#include "daffy/core/logger.hpp"
#include "daffy/rooms/room_registry.hpp"
#include "daffy/runtime/event_bus.hpp"

int main() {
  std::cout << "Testing room lifecycle...\n";

  // Setup
  auto logger = daffy::core::CreateConsoleLogger("test", daffy::core::LogLevel::kInfo);
  daffy::runtime::InMemoryEventBus event_bus;
  daffy::rooms::RoomRegistry registry(logger, event_bus);

  // Test 1: Create room
  std::cout << "  Test 1: Create room\n";
  auto room_result = registry.CreateRoom("Test Room");
  assert(room_result.ok());
  auto room = room_result.value();
  assert(room.display_name == "Test Room");
  assert(room.state == daffy::rooms::RoomState::kActive);
  assert(!room.id.empty());
  std::cout << "    Created room: " << room.id << "\n";

  // Test 2: Add participant
  std::cout << "  Test 2: Add participant\n";
  auto participant_result = registry.AddParticipant(
      room.id, "Alice", daffy::rooms::ParticipantRole::kMember);
  assert(participant_result.ok());
  auto participant = participant_result.value();
  assert(participant.display_name == "Alice");
  assert(participant.role == daffy::rooms::ParticipantRole::kMember);
  assert(!participant.id.empty());
  std::cout << "    Added participant: " << participant.id << "\n";

  // Test 3: Attach session
  std::cout << "  Test 3: Attach session\n";
  auto session_result = registry.AttachSession(room.id, participant.id, "peer-123");
  assert(session_result.ok());
  auto session = session_result.value();
  assert(session.participant_id == participant.id);
  assert(session.peer_id == "peer-123");
  assert(session.state == daffy::rooms::SessionState::kPending);
  std::cout << "    Attached session: " << session.id << "\n";

  // Test 4: Find room
  std::cout << "  Test 4: Find room\n";
  auto found_result = registry.Find(room.id);
  assert(found_result.ok());
  auto found_room = found_result.value();
  assert(found_room.id == room.id);
  assert(found_room.participants.size() == 1);
  assert(found_room.sessions.size() == 1);
  std::cout << "    Found room with " << found_room.participants.size() 
            << " participants and " << found_room.sessions.size() << " sessions\n";

  // Test 5: List rooms
  std::cout << "  Test 5: List rooms\n";
  auto rooms = registry.List();
  assert(rooms.size() >= 1);
  std::cout << "    Listed " << rooms.size() << " rooms\n";

  // Test 6: Transition room state
  std::cout << "  Test 6: Transition room state\n";
  auto transition_result = registry.TransitionRoomState(room.id, daffy::rooms::RoomState::kClosing);
  assert(transition_result.ok());
  auto transitioned_room = transition_result.value();
  assert(transitioned_room.state == daffy::rooms::RoomState::kClosing);
  std::cout << "    Transitioned room to closing state\n";

  // Test 7: Add multiple participants
  std::cout << "  Test 7: Add multiple participants\n";
  auto bob_result = registry.AddParticipant(room.id, "Bob", daffy::rooms::ParticipantRole::kMember);
  assert(bob_result.ok());
  auto charlie_result = registry.AddParticipant(room.id, "Charlie", daffy::rooms::ParticipantRole::kAdmin);
  assert(charlie_result.ok());
  
  auto multi_room = registry.Find(room.id);
  assert(multi_room.ok());
  assert(multi_room.value().participants.size() == 3);
  std::cout << "    Added 2 more participants, total: " << multi_room.value().participants.size() << "\n";

  // Test 8: Error handling - room not found
  std::cout << "  Test 8: Error handling\n";
  auto not_found = registry.Find("nonexistent-room-id");
  assert(!not_found.ok());
  assert(not_found.error().code == daffy::core::ErrorCode::kNotFound);
  std::cout << "    Correctly handled room not found\n";

  // Test 9: Error handling - participant not found
  auto bad_session = registry.AttachSession(room.id, "nonexistent-participant", "peer-456");
  assert(!bad_session.ok());
  assert(bad_session.error().code == daffy::core::ErrorCode::kNotFound);
  std::cout << "    Correctly handled participant not found\n";

  std::cout << "All room lifecycle tests passed!\n";
  return 0;
}
