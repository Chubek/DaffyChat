cmake_minimum_required(VERSION 3.20)
project(DaffyChat VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DAFFY_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(DAFFY_ENABLE_WERROR "Treat warnings as errors" OFF)
option(DAFFY_ENABLE_TESTS "Build test targets" ON)
option(DAFFY_ENABLE_FRONTEND_ASSETS "Install bundled frontend assets" ON)
option(BUILD_DEB "Add a `deb` packaging target" OFF)
option(BUILD_RPM "Add an `rpm` packaging target" OFF)
option(BUILD_PACMAN "Add a `pacman` packaging target" OFF)
option(DAFFY_PACKAGE_DEB "Enable DEB packaging support" ON)
option(DAFFY_PACKAGE_RPM "Enable RPM packaging support" ON)
option(DAFFY_PACKAGE_TGZ "Enable TGZ packaging support" ON)
set(DAFFY_SANITIZER "" CACHE STRING "Optional sanitizer: address, undefined, thread, leak")
set_property(CACHE DAFFY_SANITIZER PROPERTY STRINGS "" address undefined thread leak)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(DaffyVendoredDependencies)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_library(daffy_project_options INTERFACE)
target_compile_features(daffy_project_options INTERFACE cxx_std_20)

target_compile_definitions(daffy_project_options INTERFACE
  DAFFY_VERSION="${PROJECT_VERSION}"
  DAFFY_GIT_REVISION="workspace"
  DAFFY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

if(DAFFY_ENABLE_WARNINGS)
  target_compile_options(daffy_project_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)
endif()

if(DAFFY_ENABLE_WERROR)
  target_compile_options(daffy_project_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/WX>)
endif()

if(DAFFY_SANITIZER)
  target_compile_options(daffy_project_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fsanitize=${DAFFY_SANITIZER}> )
  target_link_options(daffy_project_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fsanitize=${DAFFY_SANITIZER}> )
endif()

set(DAFFY_CORE_SOURCES
  src/core/build_info.cpp
  src/core/id.cpp
  src/core/logger.cpp
  src/core/time.cpp
  src/config/app_config.cpp
  src/util/json.cpp
  src/runtime/event_bus.cpp
  src/signaling/messages.cpp
  src/signaling/signaling_client_facade.cpp
  src/signaling/native_signaling_client.cpp
  src/signaling/admin_http_server.cpp
  src/signaling/server.cpp
  src/signaling/uwebsockets_server.cpp
  src/services/service_metadata.cpp
  src/services/service_registry.cpp
  src/ipc/nng_transport.cpp
  src/rooms/models.cpp
  src/rooms/events.cpp
  src/rooms/room_registry.cpp
  src/voice/audio_pipeline.cpp
  src/voice/audio_processing.cpp
  src/voice/live_voice_session.cpp
  src/voice/libdatachannel_peer.cpp
  src/voice/media_worker.cpp
  src/voice/native_voice_client.cpp
  src/voice/opus_codec.cpp
  src/voice/portaudio_runtime.cpp
  src/voice/portaudio_streams.cpp
  src/voice/rtp_srtp.cpp
  src/voice/voice_session_facade.cpp
  src/voice/voice_loopback_harness.cpp
  src/voice/voice_peer_session.cpp
  src/web/webhook.cpp
  src/web/voice_diagnostics_http_server.cpp
  third_party/uWebSockets/uSockets/src/bsd.c
  third_party/uWebSockets/uSockets/src/context.c
  third_party/uWebSockets/uSockets/src/crypto/openssl.c
  third_party/uWebSockets/uSockets/src/eventing/epoll_kqueue.c
  third_party/uWebSockets/uSockets/src/loop.c
  third_party/uWebSockets/uSockets/src/quic.c
  third_party/uWebSockets/uSockets/src/socket.c
  third_party/uWebSockets/uSockets/src/udp.c)

add_library(daffy_core STATIC ${DAFFY_CORE_SOURCES})

target_include_directories(daffy_core
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
  PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/spdlog/include"
    "${PROJECT_SOURCE_DIR}/third_party/libsamplerate/include"
    "${PROJECT_SOURCE_DIR}/third_party/libdatachannel/include/rtc"
    "${PROJECT_SOURCE_DIR}/third_party/opus/include"
    "${PROJECT_SOURCE_DIR}/third_party/portaudio/include"
    "${PROJECT_SOURCE_DIR}/third_party/rnnoise/include"
    "${PROJECT_SOURCE_DIR}/third_party/uWebSockets/src"
    "${PROJECT_SOURCE_DIR}/third_party/uWebSockets/uSockets/src")

target_compile_definitions(daffy_core PRIVATE LIBUS_NO_SSL UWS_NO_ZLIB)

find_library(DAFFY_DATACHANNEL_LIBRARY NAMES datachannel libdatachannel REQUIRED)
find_library(DAFFY_SRTP_LIBRARY NAMES srtp2 libsrtp2 REQUIRED)

target_link_libraries(daffy_core PUBLIC daffy_project_options ${CMAKE_DL_LIBS} ${DAFFY_DATACHANNEL_LIBRARY}
                                       ${DAFFY_SRTP_LIBRARY})

add_executable(daffy-backend src/cli/backend_main.cpp)
target_link_libraries(daffy-backend PRIVATE daffy_core)

add_executable(daffy-signaling src/cli/signaling_main.cpp)
target_link_libraries(daffy-signaling PRIVATE daffy_core)

if(DAFFY_ENABLE_TESTS)
  enable_testing()
  find_program(DAFFY_NODE_EXECUTABLE NAMES node nodejs)

  add_executable(daffy-bootstrap-tests tests/unit/bootstrap_smoke.cpp)
  target_link_libraries(daffy-bootstrap-tests PRIVATE daffy_core)

  add_executable(daffy-tier1-tests tests/unit/tier1_core.cpp)
  target_link_libraries(daffy-tier1-tests PRIVATE daffy_core)

  add_executable(daffy-tier3-voice-tests tests/unit/tier3_voice.cpp)
  target_link_libraries(daffy-tier3-voice-tests PRIVATE daffy_core)

  add_executable(daffy-tier3-portaudio-tests tests/unit/tier3_portaudio.cpp)
  target_link_libraries(daffy-tier3-portaudio-tests PRIVATE daffy_core)

  add_executable(daffy-tier3-portaudio-streams-tests tests/unit/tier3_portaudio_streams.cpp)
  target_link_libraries(daffy-tier3-portaudio-streams-tests PRIVATE daffy_core)

  add_executable(daffy-tier3-media-tests tests/unit/tier3_media.cpp)
  target_link_libraries(daffy-tier3-media-tests PRIVATE daffy_core)
  add_executable(daffy-tier3-native-voice-client-fake-tests tests/unit/tier3_native_voice_client_fake.cpp)
  target_link_libraries(daffy-tier3-native-voice-client-fake-tests PRIVATE daffy_core)
  add_executable(daffy-tier3-voice-loopback-fake-tests tests/unit/tier3_voice_loopback_fake.cpp)
  target_link_libraries(daffy-tier3-voice-loopback-fake-tests PRIVATE daffy_core)

  add_executable(daffy-tier3-transport-tests tests/unit/tier3_transport.cpp)
  target_link_libraries(daffy-tier3-transport-tests PRIVATE daffy_core)
  add_executable(daffy-tier3-peer-session-tests tests/integration/tier3_peer_session.cpp)
  target_link_libraries(daffy-tier3-peer-session-tests PRIVATE daffy_core)

  add_executable(daffy-tier2-signaling-tests tests/integration/signaling_flow.cpp)
  target_link_libraries(daffy-tier2-signaling-tests PRIVATE daffy_core)
  add_executable(daffy-tier2-admin-http-tests tests/integration/signaling_admin_http.cpp)
  target_link_libraries(daffy-tier2-admin-http-tests PRIVATE daffy_core)
  add_executable(daffy-tier2-voice-diagnostics-http-tests tests/integration/voice_diagnostics_http.cpp)
  target_link_libraries(daffy-tier2-voice-diagnostics-http-tests PRIVATE daffy_core)
  add_executable(daffy-tier2-uwebsockets-tests tests/integration/signaling_uwebsockets.cpp)
  target_link_libraries(daffy-tier2-uwebsockets-tests PRIVATE daffy_core)
  add_executable(daffy-tier2-native-signaling-client-tests tests/integration/signaling_native_client.cpp)
  target_link_libraries(daffy-tier2-native-signaling-client-tests PRIVATE daffy_core)

  add_test(NAME daffy-bootstrap-smoke COMMAND daffy-bootstrap-tests)
  add_test(NAME daffy-tier1-core COMMAND daffy-tier1-tests)
  add_test(NAME daffy-tier3-voice COMMAND daffy-tier3-voice-tests)
  add_test(NAME daffy-tier3-portaudio COMMAND daffy-tier3-portaudio-tests)
  add_test(NAME daffy-tier3-portaudio-streams COMMAND daffy-tier3-portaudio-streams-tests)
  add_test(NAME daffy-tier3-media COMMAND daffy-tier3-media-tests)
  add_test(NAME daffy-tier3-native-voice-client-fake COMMAND daffy-tier3-native-voice-client-fake-tests)
  add_test(NAME daffy-tier3-voice-loopback-fake COMMAND daffy-tier3-voice-loopback-fake-tests)
  add_test(NAME daffy-tier3-transport COMMAND daffy-tier3-transport-tests)
  add_test(NAME daffy-tier3-peer-session COMMAND daffy-tier3-peer-session-tests)
  add_test(NAME daffy-tier2-signaling COMMAND daffy-tier2-signaling-tests)
  add_test(NAME daffy-tier2-admin-http COMMAND daffy-tier2-admin-http-tests)
  add_test(NAME daffy-tier2-voice-diagnostics-http COMMAND daffy-tier2-voice-diagnostics-http-tests)
  add_test(NAME daffy-tier2-uwebsockets COMMAND daffy-tier2-uwebsockets-tests)
  add_test(NAME daffy-tier2-native-signaling-client COMMAND daffy-tier2-native-signaling-client-tests)
  if(DAFFY_NODE_EXECUTABLE)
    add_test(
      NAME daffy-frontend-bridge-bootstrap-smoke
      COMMAND ${DAFFY_NODE_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/frontend/bridge_bootstrap_smoke.js)
  endif()
endif()

# ---------------------------------------------------------------------------
# DSSL Toolchain
# ---------------------------------------------------------------------------

add_executable(dssl-bindgen dssl/tools/bindgen/main.cpp)
target_include_directories(dssl-bindgen PRIVATE
  "${PROJECT_SOURCE_DIR}/third_party/PEGTL/include"
  "${PROJECT_SOURCE_DIR}/dssl")
target_link_libraries(dssl-bindgen PRIVATE daffy_project_options)

add_executable(dssl-docstrip dssl/tools/docstrip/main.cpp)
target_include_directories(dssl-docstrip PRIVATE
  "${PROJECT_SOURCE_DIR}/third_party/PEGTL/include"
  "${PROJECT_SOURCE_DIR}/dssl")
target_link_libraries(dssl-docstrip PRIVATE daffy_project_options)

add_executable(dssl-docgen dssl/tools/docgen/main.cpp)
target_include_directories(dssl-docgen PRIVATE
  "${PROJECT_SOURCE_DIR}/third_party/PEGTL/include"
  "${PROJECT_SOURCE_DIR}/dssl")
target_link_libraries(dssl-docgen PRIVATE daffy_project_options)

# ---------------------------------------------------------------------------
# Daffyscript Compiler
# ---------------------------------------------------------------------------

add_executable(daffyscript daffyscript/main.cpp)
target_include_directories(daffyscript PRIVATE
  "${PROJECT_SOURCE_DIR}/third_party/PEGTL/include"
  "${PROJECT_SOURCE_DIR}/daffyscript")
target_link_libraries(daffyscript PRIVATE daffy_project_options)

# ---------------------------------------------------------------------------
# DSSL / Daffyscript Tests
# ---------------------------------------------------------------------------

if(DAFFY_ENABLE_TESTS)
  add_executable(daffy-dssl-toolchain-tests tests/unit/dssl_toolchain.cpp)
  target_include_directories(daffy-dssl-toolchain-tests PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/PEGTL/include"
    "${PROJECT_SOURCE_DIR}/dssl")
  target_link_libraries(daffy-dssl-toolchain-tests PRIVATE daffy_project_options)
  add_test(NAME daffy-dssl-toolchain COMMAND daffy-dssl-toolchain-tests)

  add_executable(daffy-daffyscript-toolchain-tests tests/unit/daffyscript_toolchain.cpp)
  target_include_directories(daffy-daffyscript-toolchain-tests PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/PEGTL/include"
    "${PROJECT_SOURCE_DIR}/daffyscript")
  target_link_libraries(daffy-daffyscript-toolchain-tests PRIVATE daffy_project_options)
  add_test(NAME daffy-daffyscript-toolchain COMMAND daffy-daffyscript-toolchain-tests)
endif()

install(TARGETS daffy_core daffy-backend daffy-signaling
  dssl-bindgen dssl-docstrip dssl-docgen daffyscript
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

install(FILES config/daffychat.example.json DESTINATION share/daffychat/config)

if(DAFFY_ENABLE_FRONTEND_ASSETS)
  install(FILES
    frontend/index.html
    frontend/room.html
    frontend/guide.html
    frontend/403.html
    frontend/404.html
    frontend/bridge.js
    DESTINATION share/daffychat/frontend)
  install(FILES
    frontend/app/styles/daffy.css
    DESTINATION share/daffychat/frontend/app/styles)
  install(FILES
    frontend/app/state/store.js
    DESTINATION share/daffychat/frontend/app/state)
  install(FILES
    frontend/app/api/signaling.js
    DESTINATION share/daffychat/frontend/app/api)
  install(FILES
    frontend/app/hooks/bridge-hooks.js
    DESTINATION share/daffychat/frontend/app/hooks)
  install(FILES
    frontend/app/components/message-renderer.js
    frontend/app/components/theme-toggle.js
    DESTINATION share/daffychat/frontend/app/components)
endif()

install(DIRECTORY stdext DESTINATION share/daffychat)

set(CPACK_PACKAGE_NAME "daffychat")
set(CPACK_PACKAGE_VENDOR "DaffyChat")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "maintainers@daffychat.local")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tier 0 bootstrap package for DaffyChat")

set(_daffy_cpack_generators "")
if(DAFFY_PACKAGE_DEB)
  list(APPEND _daffy_cpack_generators DEB)
endif()
if(DAFFY_PACKAGE_RPM)
  list(APPEND _daffy_cpack_generators RPM)
endif()
if(DAFFY_PACKAGE_TGZ)
  list(APPEND _daffy_cpack_generators TGZ)
endif()
if(_daffy_cpack_generators)
  list(JOIN _daffy_cpack_generators ";" CPACK_GENERATOR)
endif()

include(CPack)

set(_daffy_package_targets
  daffy_core
  daffy-backend
  daffy-signaling
  dssl-bindgen
  dssl-docstrip
  dssl-docgen
  daffyscript)

if(BUILD_DEB)
  add_custom_target(deb
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/toolchain/package_artifact.py"
      --format deb
      --source-dir "${PROJECT_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}"
      --output-dir "${CMAKE_BINARY_DIR}/packages"
      --version "${PROJECT_VERSION}"
    DEPENDS ${_daffy_package_targets}
    COMMENT "Building DaffyChat DEB package")
endif()

if(BUILD_RPM)
  add_custom_target(rpm
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/toolchain/package_artifact.py"
      --format rpm
      --source-dir "${PROJECT_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}"
      --output-dir "${CMAKE_BINARY_DIR}/packages"
      --version "${PROJECT_VERSION}"
    DEPENDS ${_daffy_package_targets}
    COMMENT "Building DaffyChat RPM package")
endif()

if(BUILD_PACMAN)
  add_custom_target(pacman
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/toolchain/package_artifact.py"
      --format pacman
      --source-dir "${PROJECT_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}"
      --output-dir "${CMAKE_BINARY_DIR}/packages"
      --version "${PROJECT_VERSION}"
    DEPENDS ${_daffy_package_targets}
    COMMENT "Building DaffyChat Pacman package")
endif()
