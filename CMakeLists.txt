cmake_minimum_required(VERSION 3.20)
project(DaffyChat VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DAFFY_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(DAFFY_ENABLE_WERROR "Treat warnings as errors" OFF)
option(DAFFY_ENABLE_TESTS "Build test targets" ON)
option(DAFFY_ENABLE_FRONTEND_ASSETS "Install bundled frontend assets" ON)
option(DAFFY_INSTALL_DEFAULT_SERVICES "Install default service specifications" ON)
option(DAFFY_BUILD_DOCS "Build documentation with Doxygen/Sphinx" OFF)
option(DAFFY_INSTALL_DOCS "Build and install documentation" OFF)
option(DAFFY_CLIENT_SIDE_ONLY "Build and install client-side tools only" OFF)
set(DAFFY_CONFIG_FORMAT "JSON" CACHE STRING "Config format to install: JSON or CONF")
set_property(CACHE DAFFY_CONFIG_FORMAT PROPERTY STRINGS JSON CONF)

# Linking mode: DYNAMIC (default), STATIC, or PACK
# DYNAMIC  – link against system-installed shared libraries (default)
# STATIC   – build vendored libs from source and statically link them in
# PACK     – build vendored libs as shared objects and bundle them in packages
set(LINKING "DYNAMIC" CACHE STRING "Library linking mode: DYNAMIC | STATIC | PACK")
set_property(CACHE LINKING PROPERTY STRINGS DYNAMIC STATIC PACK)

option(PACK_FRONTEND "Bundle and install frontend assets in packages" ON)

option(BUILD_DEB "Add a `deb` packaging target" OFF)
option(BUILD_RPM "Add an `rpm` packaging target" OFF)
option(BUILD_PACMAN "Add a `pacman` packaging target" OFF)
option(BUILD_TARBALL "Add a `tarball` packaging target" OFF)
option(BUILD_GZIP "Add a `gzip` archive target" OFF)
option(BUILD_BZIP2 "Add a `bzip2` archive target" OFF)
option(BUILD_ZLIB "Add a `zlib` archive target" OFF)
option(NO_INSTALL_MAN "Disable manpage installation" OFF)
option(DAFFY_INSTALL_EXAMPLE_PLUGINS "Install example extensions" ON)
option(DAFFY_INSTALL_TOOLCHAIN "Install toolchain helper scripts" ON)
option(DAFFY_INSTALL_STDEXT "Install standard extensions from stdext/" OFF)
option(DAFFY_INSTALL_COTURN "Install vendored coturn from third_party/" OFF)
option(DAFFY_PACKAGE_DEB "Enable DEB packaging support" OFF)
option(DAFFY_PACKAGE_RPM "Enable RPM packaging support" OFF)
option(DAFFY_PACKAGE_TGZ "Enable TGZ packaging support" OFF)
set(DAFFY_SANITIZER "" CACHE STRING "Optional sanitizer: address, undefined, thread, leak")
set_property(CACHE DAFFY_SANITIZER PROPERTY STRINGS "" address undefined thread leak)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(DaffyVendoredDependencies)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(Confuse QUIET)

if(DAFFY_HAVE_nng)
  set(NNG_ENABLE_TLS OFF CACHE BOOL "" FORCE)
  set(NNG_ENABLE_HTTP OFF CACHE BOOL "" FORCE)
  set(NNG_TESTS OFF CACHE BOOL "" FORCE)
  set(NNG_TOOLS OFF CACHE BOOL "" FORCE)
  set(NNG_ENABLE_NNGCAT OFF CACHE BOOL "" FORCE)
  if(LINKING STREQUAL "STATIC")
    # Build nng as a static archive and link it wholly into daffy_core
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/nng EXCLUDE_FROM_ALL)
    set_target_properties(nng PROPERTIES
      POSITION_INDEPENDENT_CODE ON)
  elseif(LINKING STREQUAL "PACK")
    # Build nng as a shared library that will be bundled in the package
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    add_subdirectory(third_party/nng EXCLUDE_FROM_ALL)
    # Collect the shared library for later packaging
    set(DAFFY_PACK_SHARED_LIBS ${DAFFY_PACK_SHARED_LIBS} nng CACHE INTERNAL "")
  else()
    # DYNAMIC: prefer system-installed libnng; fall back to vendored build
    find_library(_DAFFY_SYS_NNG NAMES nng libnng)
    if(_DAFFY_SYS_NNG)
      message(STATUS "Using system nng: ${_DAFFY_SYS_NNG}")
      add_library(nng SHARED IMPORTED)
      set_target_properties(nng PROPERTIES IMPORTED_LOCATION "${_DAFFY_SYS_NNG}")
      # Expose nng headers from vendored source regardless
      target_include_directories(nng INTERFACE
        "${PROJECT_SOURCE_DIR}/third_party/nng/include")
    else()
      message(STATUS "System nng not found – building vendored nng as shared library")
      set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
      add_subdirectory(third_party/nng EXCLUDE_FROM_ALL)
    endif()
  endif()
endif()

# For STATIC mode: tell the linker to pull in the full archive so all
# nng symbols survive even if daffy_core is itself static.
if(LINKING STREQUAL "STATIC" AND TARGET nng)
  set(DAFFY_NNG_LINK_FLAGS "-Wl,--whole-archive $<TARGET_FILE:nng> -Wl,--no-whole-archive")
endif()

add_library(daffy_project_options INTERFACE)
target_compile_features(daffy_project_options INTERFACE cxx_std_20)

target_compile_definitions(daffy_project_options INTERFACE
  DAFFY_VERSION="${PROJECT_VERSION}"
  DAFFY_GIT_REVISION="workspace"
  DAFFY_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  DAFFY_SYSCONFDIR="/etc/daffychat")

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
  src/runtime/daemon_manager.cpp
  src/signaling/messages.cpp
  src/signaling/signaling_client_facade.cpp
  src/signaling/native_signaling_client.cpp
  src/signaling/admin_http_server.cpp
  src/signaling/server.cpp
  src/signaling/uwebsockets_server.cpp
  services/generated/echo.service.cpp
  services/generated/echo.skeleton.cpp
  services/generated/room_ops.service.cpp
  services/generated/room_ops.skeleton.cpp
  src/services/echo_service.cpp
  src/services/event_bridge_service.cpp
  src/services/bot_api_service.cpp
  src/services/health_service.cpp
  src/services/room_ops_service.cpp
  src/services/room_state_service.cpp
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
add_dependencies(daffy_core daffy-generate-echo-service daffy-generate-room-ops-service)

target_include_directories(daffy_core
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}"
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
if(TARGET nng)
  if(LINKING STREQUAL "STATIC")
    # For static linking we use whole-archive so that all nng translation
    # units are included even though daffy_core is also a static archive.
    target_link_libraries(daffy_core PUBLIC
      -Wl,--whole-archive $<TARGET_FILE:nng> -Wl,--no-whole-archive)
    add_dependencies(daffy_core nng)
  else()
    target_link_libraries(daffy_core PUBLIC nng)
  endif()
endif()

add_executable(daffy-backend src/cli/backend_main.cpp)
target_link_libraries(daffy-backend PRIVATE daffy_core)

add_executable(daffy-signaling src/cli/signaling_main.cpp)
target_link_libraries(daffy-signaling PRIVATE daffy_core)

add_executable(daffydmd src/cli/daffydmd_main.cpp)
target_link_libraries(daffydmd PRIVATE daffy_core)

# STATIC mode: pull in C++ runtime statically so no libnng.so / libstdc++.so
# is required at runtime.  GCC/Clang only – harmless on MSVC (not supported).
if(LINKING STREQUAL "STATIC")
  foreach(_tgt daffy-backend daffy-signaling daffydmd)
    target_link_options(${_tgt} PRIVATE
      $<$<CXX_COMPILER_ID:GNU,Clang>:-static-libstdc++ -static-libgcc>)
  endforeach()
endif()

add_executable(daffy-echo-service src/cli/echo_service_main.cpp)
target_link_libraries(daffy-echo-service PRIVATE daffy_core)

add_executable(daffy-health-service src/cli/health_service_main.cpp)
target_link_libraries(daffy-health-service PRIVATE daffy_core)

add_executable(daffy-botapi-service src/cli/bot_api_service_main.cpp)
target_link_libraries(daffy-botapi-service PRIVATE daffy_core)

add_executable(daffy-eventbridge-service src/cli/event_bridge_service_main.cpp)
target_link_libraries(daffy-eventbridge-service PRIVATE daffy_core)

add_executable(daffy-roomops-service src/cli/room_ops_service_main.cpp)
target_link_libraries(daffy-roomops-service PRIVATE daffy_core)

add_executable(daffy-roomstate-service src/cli/room_state_service_main.cpp)
target_link_libraries(daffy-roomstate-service PRIVATE daffy_core)

if(DAFFY_ENABLE_TESTS)
  enable_testing()
  find_program(DAFFY_NODE_EXECUTABLE NAMES node nodejs)

  add_executable(daffy-bootstrap-tests tests/unit/bootstrap_smoke.cpp)
  target_link_libraries(daffy-bootstrap-tests PRIVATE daffy_core)

  add_executable(daffy-tier1-tests tests/unit/tier1_core.cpp)
  target_link_libraries(daffy-tier1-tests PRIVATE daffy_core)

  add_executable(daffy-service-vertical-slice-tests tests/unit/service_vertical_slice.cpp)
  target_link_libraries(daffy-service-vertical-slice-tests PRIVATE daffy_core)

  add_executable(daffy-generated-multi-rpc-tests tests/unit/generated_multi_rpc_service.cpp)
  target_link_libraries(daffy-generated-multi-rpc-tests PRIVATE daffy_core)

  add_executable(daffy-builtin-services-tests tests/unit/builtin_services.cpp)
  target_link_libraries(daffy-builtin-services-tests PRIVATE daffy_core)

  add_executable(daffy-event-bridge-service-tests tests/unit/event_bridge_service.cpp)
  target_link_libraries(daffy-event-bridge-service-tests PRIVATE daffy_core)
  add_executable(daffy-bot-api-service-tests tests/unit/bot_api_service.cpp)
  target_link_libraries(daffy-bot-api-service-tests PRIVATE daffy_core)


  add_executable(daffy-daemon-manager-tests tests/integration/daemon_manager.cpp)
  target_link_libraries(daffy-daemon-manager-tests PRIVATE daffy_core)

  add_executable(daffy-room-lifecycle-tests tests/integration/room_lifecycle.cpp)
  target_link_libraries(daffy-room-lifecycle-tests PRIVATE daffy_core)

  add_executable(daffy-service-integration-tests tests/integration/service_integration.cpp)
  target_link_libraries(daffy-service-integration-tests PRIVATE daffy_core)

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
  add_executable(daffy-tier2-signaling-http-api-tests tests/integration/signaling_http_api.cpp)
  target_link_libraries(daffy-tier2-signaling-http-api-tests PRIVATE daffy_core)
  add_executable(daffy-tier2-native-signaling-client-tests tests/integration/signaling_native_client.cpp)
  target_link_libraries(daffy-tier2-native-signaling-client-tests PRIVATE daffy_core)

  add_test(NAME daffy-bootstrap-smoke COMMAND daffy-bootstrap-tests)
  add_test(NAME daffy-service-vertical-slice COMMAND daffy-service-vertical-slice-tests)
  add_test(NAME daffy-generated-multi-rpc COMMAND daffy-generated-multi-rpc-tests)
  add_test(NAME daffy-builtin-services COMMAND daffy-builtin-services-tests)
  add_test(NAME daffy-event-bridge-service COMMAND daffy-event-bridge-service-tests)
  add_test(NAME daffy-bot-api-service COMMAND daffy-bot-api-service-tests)
  add_test(NAME daffy-daemon-manager COMMAND daffy-daemon-manager-tests)
  add_test(NAME daffy-room-lifecycle COMMAND daffy-room-lifecycle-tests)
  add_test(NAME daffy-service-integration COMMAND daffy-service-integration-tests)
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
  add_test(NAME daffy-tier2-signaling-http-api COMMAND daffy-tier2-signaling-http-api-tests)
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

set(DAFFY_GENERATED_SERVICE_DIR "${CMAKE_BINARY_DIR}/generated/services")
add_custom_command(
  OUTPUT
    "${DAFFY_GENERATED_SERVICE_DIR}/echo.generated.hpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/echo.service.hpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/echo.service.cpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/echo.skeleton.cpp"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DAFFY_GENERATED_SERVICE_DIR}"
  COMMAND dssl-bindgen --target cpp --out-dir "${DAFFY_GENERATED_SERVICE_DIR}" "${PROJECT_SOURCE_DIR}/services/specs/echo.dssl"
  DEPENDS dssl-bindgen "${PROJECT_SOURCE_DIR}/services/specs/echo.dssl"
  COMMENT "Generating echo service artifacts from DSSL"
  VERBATIM)
add_custom_target(daffy-generate-echo-service
  DEPENDS
    "${DAFFY_GENERATED_SERVICE_DIR}/echo.generated.hpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/echo.service.hpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/echo.service.cpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/echo.skeleton.cpp")

add_custom_command(
  OUTPUT
    "${DAFFY_GENERATED_SERVICE_DIR}/room_ops.generated.hpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/room_ops.service.hpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/room_ops.service.cpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/room_ops.skeleton.cpp"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DAFFY_GENERATED_SERVICE_DIR}"
  COMMAND dssl-bindgen --target cpp --out-dir "${DAFFY_GENERATED_SERVICE_DIR}" "${PROJECT_SOURCE_DIR}/services/specs/room_ops.dssl"
  DEPENDS dssl-bindgen "${PROJECT_SOURCE_DIR}/services/specs/room_ops.dssl"
  COMMENT "Generating room_ops service artifacts from DSSL"
  VERBATIM)
add_custom_target(daffy-generate-room-ops-service
  DEPENDS
    "${DAFFY_GENERATED_SERVICE_DIR}/room_ops.generated.hpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/room_ops.service.hpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/room_ops.service.cpp"
    "${DAFFY_GENERATED_SERVICE_DIR}/room_ops.skeleton.cpp")

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
  add_dependencies(daffy-dssl-toolchain-tests daffy-generate-echo-service daffy-generate-room-ops-service)

  add_executable(daffy-daffyscript-toolchain-tests tests/unit/daffyscript_toolchain.cpp)
  target_include_directories(daffy-daffyscript-toolchain-tests PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/PEGTL/include"
    "${PROJECT_SOURCE_DIR}/daffyscript")
  target_link_libraries(daffy-daffyscript-toolchain-tests PRIVATE daffy_project_options)
  add_test(NAME daffy-daffyscript-toolchain COMMAND daffy-daffyscript-toolchain-tests)
endif()

install(TARGETS daffy_core daffy-backend daffy-signaling daffydmd
  dssl-bindgen dssl-docstrip dssl-docgen daffyscript
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

install(FILES config/daffychat.example.json DESTINATION share/daffychat/config)

# Config installation handled by post-install script based on DAFFY_CONFIG_FORMAT
if(NOT DAFFY_CLIENT_SIDE_ONLY)
  install(FILES config/daffychat.example.conf DESTINATION share/daffychat/config OPTIONAL)
endif()

install(FILES
  README.md
  INSTALL.md
  GUIDE.md
  DEPLOY.md
  EXTENSIBILITY.md
  RECIPES.md
  DESTINATION share/daffychat/docs)

install(FILES services/specs/echo.dssl DESTINATION share/daffychat/services/specs)
install(FILES services/specs/room_ops.dssl DESTINATION share/daffychat/services/specs)
install(FILES
  services/generated/echo.generated.hpp
  services/generated/echo.service.hpp
  services/generated/echo.service.cpp
  services/generated/echo.skeleton.cpp
  services/generated/room_ops.generated.hpp
  services/generated/room_ops.service.hpp
  services/generated/room_ops.service.cpp
  services/generated/room_ops.skeleton.cpp
  DESTINATION share/daffychat/services/generated)

if(DAFFY_INSTALL_DEFAULT_SERVICES)
  install(DIRECTORY services/builtin DESTINATION share/daffychat/services
    FILES_MATCHING PATTERN "*.dssl" PATTERN "*.cpp" PATTERN "*.hpp")
endif()

install(PROGRAMS scripts/deploy.sh DESTINATION share/daffychat/scripts)
install(FILES scripts/daffydmd.service DESTINATION share/daffychat/scripts)
install(FILES scripts/daffybackend.service DESTINATION share/daffychat/scripts)
install(FILES scripts/daffysignaling.service DESTINATION share/daffychat/scripts)
install(PROGRAMS scripts/os-service.pl DESTINATION share/daffychat/scripts)
install(PROGRAMS scripts/post-install.sh DESTINATION share/daffychat/scripts)
install(PROGRAMS scripts/linking-service.sh DESTINATION share/daffychat/scripts)
install(PROGRAMS scripts/install-frontend.sh DESTINATION share/daffychat/scripts)
install(PROGRAMS scripts/stdext-helper.sh DESTINATION share/daffychat/scripts)

if(DAFFY_INSTALL_TOOLCHAIN)
  install(PROGRAMS
    toolchain/install-service.py
    toolchain/package_artifact.py
    scripts/archive-service.py
    DESTINATION share/daffychat/toolchain)
endif()

if(DAFFY_ENABLE_FRONTEND_ASSETS AND PACK_FRONTEND)
  install(FILES
    frontend/index.html
    frontend/room.html
    frontend/guide.html
    frontend/dynerror.html
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
  install(FILES
    frontend/lib/animate.min.css
    frontend/lib/quote.js
    DESTINATION share/daffychat/frontend/lib)
  install(FILES
    frontend/resources/boxicons.min.css
    frontend/resources/box-icons.list
    frontend/resources/quotes.json
    frontend/resources/error-messages.json
    DESTINATION share/daffychat/frontend/resources)
endif()

# PACK linking mode: install vendored shared libraries so the package can
# deliver them alongside the binaries.
if(LINKING STREQUAL "PACK")
  # nng shared library
  if(TARGET nng)
    install(FILES $<TARGET_FILE:nng>
      DESTINATION lib/daffychat
      OPTIONAL)
    # Also create an ld.so.conf.d snippet so the system linker finds the lib
    install(CODE "
      file(WRITE \"\${CMAKE_INSTALL_PREFIX}/etc/ld.so.conf.d/daffychat.conf\"
           \"\${CMAKE_INSTALL_PREFIX}/lib/daffychat\n\")
    ")
  endif()
endif()

if(DAFFY_INSTALL_EXAMPLE_PLUGINS)
  install(DIRECTORY stdext DESTINATION share/daffychat)
endif()

if(DAFFY_INSTALL_COTURN)
  if(EXISTS "${PROJECT_SOURCE_DIR}/third_party/coturn")
    install(DIRECTORY third_party/coturn DESTINATION share/daffychat/third_party)
  else()
    message(WARNING "DAFFY_INSTALL_COTURN is ON but third_party/coturn was not found")
  endif()
endif()

if(NOT NO_INSTALL_MAN AND EXISTS "${PROJECT_SOURCE_DIR}/man")
  install(DIRECTORY man/ DESTINATION share/man/man1 FILES_MATCHING PATTERN "*.1")
  add_custom_target(install-manpages
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --component manpages
    COMMENT "Installing DaffyChat manpages")
endif()

# Documentation build and install
if(DAFFY_BUILD_DOCS OR DAFFY_INSTALL_DOCS)
  find_package(Doxygen)
  find_program(SPHINX_BUILD sphinx-build)
  
  if(DOXYGEN_FOUND AND SPHINX_BUILD)
    set(DOXYGEN_INPUT_DIR "${PROJECT_SOURCE_DIR}/include")
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/xml")
    set(SPHINX_SOURCE_DIR "${PROJECT_SOURCE_DIR}/docs")
    set(SPHINX_BUILD_DIR "${CMAKE_BINARY_DIR}/docs/html")
    
    # Doxygen target
    add_custom_target(doxygen
      COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_SOURCE_DIR}/docs/Doxyfile
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/docs
      COMMENT "Generating C++ API documentation with Doxygen"
      VERBATIM)
    
    # Sphinx target
    add_custom_target(sphinx
      COMMAND ${SPHINX_BUILD} -b html ${SPHINX_SOURCE_DIR} ${SPHINX_BUILD_DIR}
      DEPENDS doxygen
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMENT "Generating documentation with Sphinx"
      VERBATIM)
    
    add_custom_target(docs DEPENDS sphinx)
    
    if(DAFFY_INSTALL_DOCS)
      install(DIRECTORY ${SPHINX_BUILD_DIR}/
        DESTINATION share/doc/daffychat/html
        OPTIONAL)
    endif()
  else()
    message(WARNING "Doxygen or Sphinx not found, documentation will not be built")
  endif()
endif()

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
  daffydmd
  dssl-bindgen
  dssl-docstrip
  dssl-docgen
  daffyscript)

# Shared flags passed to archive-service.py for every package format
set(_daffy_archive_flags
  --linking "${LINKING}"
  $<IF:$<BOOL:${PACK_FRONTEND}>,--pack-frontend,--no-pack-frontend>
  $<IF:$<BOOL:${DAFFY_CLIENT_SIDE_ONLY}>,--client-only,>
  $<IF:$<BOOL:${DAFFY_INSTALL_STDEXT}>,--with-stdext,--no-stdext>
  $<IF:$<BOOL:${DAFFY_INSTALL_COTURN}>,--with-coturn,--no-coturn>)

if(BUILD_DEB)
  add_custom_target(deb
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/archive-service.py"
      --format deb
      --source-dir "${PROJECT_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}"
      --output-dir "${CMAKE_BINARY_DIR}/packages"
      --version "${PROJECT_VERSION}"
      ${_daffy_archive_flags}
    DEPENDS ${_daffy_package_targets}
    COMMENT "Building DaffyChat DEB package")
endif()

if(BUILD_RPM)
  add_custom_target(rpm
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/archive-service.py"
      --format rpm
      --source-dir "${PROJECT_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}"
      --output-dir "${CMAKE_BINARY_DIR}/packages"
      --version "${PROJECT_VERSION}"
      ${_daffy_archive_flags}
    DEPENDS ${_daffy_package_targets}
    COMMENT "Building DaffyChat RPM package")
endif()

if(BUILD_PACMAN)
  add_custom_target(pacman
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/archive-service.py"
      --format pacman
      --source-dir "${PROJECT_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}"
      --output-dir "${CMAKE_BINARY_DIR}/packages"
      --version "${PROJECT_VERSION}"
      ${_daffy_archive_flags}
    DEPENDS ${_daffy_package_targets}
    COMMENT "Building DaffyChat Pacman package")
endif()

if(BUILD_TARBALL)
  add_custom_target(tarball
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/archive-service.py"
      --format tarball
      --source-dir "${PROJECT_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}"
      --output-dir "${CMAKE_BINARY_DIR}/packages"
      --version "${PROJECT_VERSION}"
      ${_daffy_archive_flags}
    DEPENDS ${_daffy_package_targets}
    COMMENT "Building DaffyChat tarball")
endif()

if(BUILD_GZIP)
  add_custom_target(gzip
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/archive-service.py"
      --format gz
      --source-dir "${PROJECT_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}"
      --output-dir "${CMAKE_BINARY_DIR}/packages"
      --version "${PROJECT_VERSION}"
      ${_daffy_archive_flags}
    DEPENDS ${_daffy_package_targets}
    COMMENT "Building DaffyChat gzip archive")
endif()

if(BUILD_BZIP2)
  add_custom_target(bzip2
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/archive-service.py"
      --format bz2
      --source-dir "${PROJECT_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}"
      --output-dir "${CMAKE_BINARY_DIR}/packages"
      --version "${PROJECT_VERSION}"
      ${_daffy_archive_flags}
    DEPENDS ${_daffy_package_targets}
    COMMENT "Building DaffyChat bzip2 archive")
endif()

if(BUILD_ZLIB)
  add_custom_target(zlib
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/archive-service.py"
      --format zlib
      --source-dir "${PROJECT_SOURCE_DIR}"
      --build-dir "${CMAKE_BINARY_DIR}"
      --output-dir "${CMAKE_BINARY_DIR}/packages"
      --version "${PROJECT_VERSION}"
      ${_daffy_archive_flags}
    DEPENDS ${_daffy_package_targets}
    COMMENT "Building DaffyChat zlib archive")
endif()
  src/signaling/socketio_voice_transport.cpp
