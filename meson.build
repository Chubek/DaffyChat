project(
  'daffychat',
  ['c', 'cpp'],
  version: '0.1.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3'
  ]
)

fs = import('fs')
python = import('python')
python3 = python.find_installation('python3')

third_party_root = meson.project_source_root() / 'third_party'
vendored_dependencies = [
  'binaryen',
  'coturn',
  'libdatachannel',
  'libsamplerate',
  'libsrtp',
  'lxc',
  'nng',
  'opus',
  'portaudio',
  'rnnoise',
  'spdlog',
  'uWebSockets',
  'wasm-micro-runtime',
]

foreach dep_name : vendored_dependencies
  dep_path = third_party_root / dep_name
  if fs.is_dir(dep_path)
    message('Found vendored dependency ' + dep_name + ': ' + dep_path)
  else
    message('Vendored dependency missing from third_party: ' + dep_name)
  endif
endforeach

common_c_args = [
  '-DLIBUS_NO_SSL',
]

common_cpp_args = [
  '-DDAFFY_VERSION="@0@"'.format(meson.project_version()),
  '-DDAFFY_GIT_REVISION="workspace"',
  '-DDAFFY_SOURCE_DIR="@0@"'.format(meson.project_source_root()),
  '-DLIBUS_NO_SSL',
  '-DUWS_NO_ZLIB',
]
node_exe = find_program('node', 'nodejs', required: false)

project_includes = include_directories(
  'include',
  'third_party/spdlog/include',
  'third_party/libdatachannel/include',
  'third_party/libdatachannel/include/rtc',
  'third_party/libsamplerate/include',
  'third_party/opus/include',
  'third_party/portaudio/include',
  'third_party/rnnoise/include',
  'third_party/uWebSockets/src',
  'third_party/uWebSockets/uSockets/src'
)

dl_dep = meson.get_compiler('cpp').find_library('dl', required: false)
datachannel_dep = meson.get_compiler('cpp').find_library('datachannel', required: true)
srtp_dep = meson.get_compiler('cpp').find_library('srtp2', required: true)

bootstrap_lib = static_library(
  'daffy_core',
  [
    'src/core/build_info.cpp',
    'src/core/id.cpp',
    'src/core/logger.cpp',
    'src/core/time.cpp',
    'src/config/app_config.cpp',
    'src/util/json.cpp',
    'src/runtime/event_bus.cpp',
    'src/signaling/messages.cpp',
    'src/signaling/signaling_client_facade.cpp',
    'src/signaling/native_signaling_client.cpp',
    'src/signaling/admin_http_server.cpp',
    'src/signaling/server.cpp',
    'src/signaling/uwebsockets_server.cpp',
    'src/services/service_metadata.cpp',
    'src/services/service_registry.cpp',
    'src/ipc/nng_transport.cpp',
    'src/rooms/models.cpp',
    'src/rooms/events.cpp',
    'src/rooms/room_registry.cpp',
    'src/voice/audio_pipeline.cpp',
    'src/voice/audio_processing.cpp',
    'src/voice/live_voice_session.cpp',
    'src/voice/libdatachannel_peer.cpp',
    'src/voice/media_worker.cpp',
    'src/voice/native_voice_client.cpp',
    'src/voice/opus_codec.cpp',
    'src/voice/portaudio_runtime.cpp',
    'src/voice/portaudio_streams.cpp',
    'src/voice/rtp_srtp.cpp',
    'src/voice/voice_session_facade.cpp',
    'src/voice/voice_loopback_harness.cpp',
    'src/voice/voice_peer_session.cpp',
    'src/web/webhook.cpp',
    'src/web/voice_diagnostics_http_server.cpp',
    'third_party/uWebSockets/uSockets/src/bsd.c',
    'third_party/uWebSockets/uSockets/src/context.c',
    'third_party/uWebSockets/uSockets/src/crypto/openssl.c',
    'third_party/uWebSockets/uSockets/src/eventing/epoll_kqueue.c',
    'third_party/uWebSockets/uSockets/src/loop.c',
    'third_party/uWebSockets/uSockets/src/quic.c',
    'third_party/uWebSockets/uSockets/src/socket.c',
    'third_party/uWebSockets/uSockets/src/udp.c',
  ],
  include_directories: project_includes,
  c_args: common_c_args,
  cpp_args: common_cpp_args,
  dependencies: [dl_dep, datachannel_dep, srtp_dep],
  install: true,
)

backend = executable(
  'daffy-backend',
  'src/cli/backend_main.cpp',
  include_directories: project_includes,
  link_with: bootstrap_lib,
  cpp_args: common_cpp_args,
  install: true,
)

signaling = executable(
  'daffy-signaling',
  'src/cli/signaling_main.cpp',
  include_directories: project_includes,
  link_with: bootstrap_lib,
  cpp_args: common_cpp_args,
  install: true,
)

if get_option('enable_tests')
  bootstrap_test = executable(
    'daffy-bootstrap-tests',
    'tests/unit/bootstrap_smoke.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-bootstrap-smoke', bootstrap_test)

  tier1_test = executable(
    'daffy-tier1-tests',
    'tests/unit/tier1_core.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier1-core', tier1_test)

  tier3_voice_test = executable(
    'daffy-tier3-voice-tests',
    'tests/unit/tier3_voice.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier3-voice', tier3_voice_test)

  tier3_portaudio_test = executable(
    'daffy-tier3-portaudio-tests',
    'tests/unit/tier3_portaudio.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier3-portaudio', tier3_portaudio_test)

  tier3_portaudio_streams_test = executable(
    'daffy-tier3-portaudio-streams-tests',
    'tests/unit/tier3_portaudio_streams.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier3-portaudio-streams', tier3_portaudio_streams_test)

  tier3_media_test = executable(
    'daffy-tier3-media-tests',
    'tests/unit/tier3_media.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier3-media', tier3_media_test)

  tier3_native_voice_client_fake_test = executable(
    'daffy-tier3-native-voice-client-fake-tests',
    'tests/unit/tier3_native_voice_client_fake.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier3-native-voice-client-fake', tier3_native_voice_client_fake_test)

  tier3_voice_loopback_fake_test = executable(
    'daffy-tier3-voice-loopback-fake-tests',
    'tests/unit/tier3_voice_loopback_fake.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier3-voice-loopback-fake', tier3_voice_loopback_fake_test)

  tier3_transport_test = executable(
    'daffy-tier3-transport-tests',
    'tests/unit/tier3_transport.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier3-transport', tier3_transport_test)

  tier3_peer_session_test = executable(
    'daffy-tier3-peer-session-tests',
    'tests/integration/tier3_peer_session.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier3-peer-session', tier3_peer_session_test)

  tier2_test = executable(
    'daffy-tier2-signaling-tests',
    'tests/integration/signaling_flow.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier2-signaling', tier2_test)

  tier2_admin_http_test = executable(
    'daffy-tier2-admin-http-tests',
    'tests/integration/signaling_admin_http.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier2-admin-http', tier2_admin_http_test)

  tier2_voice_diagnostics_http_test = executable(
    'daffy-tier2-voice-diagnostics-http-tests',
    'tests/integration/voice_diagnostics_http.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier2-voice-diagnostics-http', tier2_voice_diagnostics_http_test)

  tier2_uwebsockets_test = executable(
    'daffy-tier2-uwebsockets-tests',
    'tests/integration/signaling_uwebsockets.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier2-uwebsockets', tier2_uwebsockets_test)

  tier2_native_signaling_client_test = executable(
    'daffy-tier2-native-signaling-client-tests',
    'tests/integration/signaling_native_client.cpp',
    include_directories: project_includes,
    link_with: bootstrap_lib,
    cpp_args: common_cpp_args,
  )
  test('daffy-tier2-native-signaling-client', tier2_native_signaling_client_test)

  if node_exe.found()
    test(
      'daffy-frontend-bridge-bootstrap-smoke',
      node_exe,
      args: ['tests/frontend/bridge_bootstrap_smoke.js'],
    )
  endif
endif

# ---------------------------------------------------------------------------
# DSSL / Daffyscript toolchain includes
# ---------------------------------------------------------------------------

pegtl_includes = include_directories('third_party/PEGTL/include')
dssl_includes = include_directories('dssl')
daffyc_includes = include_directories('daffyc')

dssl_bindgen = executable(
  'dssl-bindgen',
  'dssl/tools/bindgen/main.cpp',
  include_directories: [pegtl_includes, dssl_includes],
  cpp_args: common_cpp_args,
  install: true,
)

dssl_docstrip = executable(
  'dssl-docstrip',
  'dssl/tools/docstrip/main.cpp',
  include_directories: [pegtl_includes, dssl_includes],
  cpp_args: common_cpp_args,
  install: true,
)

dssl_docgen = executable(
  'dssl-docgen',
  'dssl/tools/docgen/main.cpp',
  include_directories: [pegtl_includes, dssl_includes],
  cpp_args: common_cpp_args,
  install: true,
)

daffyc_exe = executable(
  'daffyc',
  'daffyc/main.cpp',
  include_directories: [pegtl_includes, daffyc_includes],
  cpp_args: common_cpp_args,
  install: true,
)

package_script = files('toolchain/package_artifact.py')
package_outputs_dir = meson.current_build_dir() / 'packages'
package_build_targets = [bootstrap_lib, backend, signaling, dssl_bindgen, dssl_docstrip, dssl_docgen, daffyc_exe]
if get_option('build_deb')
  deb_package = custom_target(
    'deb-package',
    output: 'deb-package.stamp',
    build_always_stale: true,
    depends: package_build_targets,
    command: [
      python3,
      package_script,
      '--format', 'deb',
      '--source-dir', meson.project_source_root(),
      '--build-dir', meson.current_build_dir(),
      '--output-dir', package_outputs_dir,
      '--version', meson.project_version(),
      '--stamp', '@OUTPUT@',
    ],
  )
  alias_target('deb', deb_package)
endif

if get_option('build_rpm')
  rpm_package = custom_target(
    'rpm-package',
    output: 'rpm-package.stamp',
    build_always_stale: true,
    depends: package_build_targets,
    command: [
      python3,
      package_script,
      '--format', 'rpm',
      '--source-dir', meson.project_source_root(),
      '--build-dir', meson.current_build_dir(),
      '--output-dir', package_outputs_dir,
      '--version', meson.project_version(),
      '--stamp', '@OUTPUT@',
    ],
  )
  alias_target('rpm', rpm_package)
endif

if get_option('build_pacman')
  pacman_package = custom_target(
    'pacman-package',
    output: 'pacman-package.stamp',
    build_always_stale: true,
    depends: package_build_targets,
    command: [
      python3,
      package_script,
      '--format', 'pacman',
      '--source-dir', meson.project_source_root(),
      '--build-dir', meson.current_build_dir(),
      '--output-dir', package_outputs_dir,
      '--version', meson.project_version(),
      '--stamp', '@OUTPUT@',
    ],
  )
  alias_target('pacman', pacman_package)
endif

if get_option('enable_tests')
  dssl_toolchain_test = executable(
    'daffy-dssl-toolchain-tests',
    'tests/unit/dssl_toolchain.cpp',
    include_directories: [pegtl_includes, dssl_includes],
    cpp_args: common_cpp_args,
  )
  test('daffy-dssl-toolchain', dssl_toolchain_test)

  daffyscript_toolchain_test = executable(
    'daffy-daffyscript-toolchain-tests',
    'tests/unit/daffyscript_toolchain.cpp',
    include_directories: [pegtl_includes, daffyc_includes],
    cpp_args: common_cpp_args,
  )
  test('daffy-daffyscript-toolchain', daffyscript_toolchain_test)
endif

install_data('config/daffychat.example.json', install_dir: get_option('datadir') / 'daffychat' / 'config')

if get_option('enable_frontend_assets')
  install_data(
    [
      'frontend/index.html',
      'frontend/room.html',
      'frontend/guide.html',
      'frontend/403.html',
      'frontend/404.html',
      'frontend/bridge.js',
    ],
    install_dir: get_option('datadir') / 'daffychat' / 'frontend'
  )
  install_data(
    ['frontend/app/styles/daffy.css'],
    install_dir: get_option('datadir') / 'daffychat' / 'frontend' / 'app' / 'styles'
  )
  install_data(
    ['frontend/app/state/store.js'],
    install_dir: get_option('datadir') / 'daffychat' / 'frontend' / 'app' / 'state'
  )
  install_data(
    ['frontend/app/api/signaling.js'],
    install_dir: get_option('datadir') / 'daffychat' / 'frontend' / 'app' / 'api'
  )
  install_data(
    ['frontend/app/hooks/bridge-hooks.js'],
    install_dir: get_option('datadir') / 'daffychat' / 'frontend' / 'app' / 'hooks'
  )
  install_data(
    [
      'frontend/app/components/message-renderer.js',
      'frontend/app/components/theme-toggle.js',
    ],
    install_dir: get_option('datadir') / 'daffychat' / 'frontend' / 'app' / 'components'
  )
endif

install_subdir('stdext', install_dir: get_option('datadir') / 'daffychat')

summary({
  'build_deb': get_option('build_deb'),
  'build_rpm': get_option('build_rpm'),
  'build_pacman': get_option('build_pacman'),
  'package_deb': get_option('package_deb'),
  'package_rpm': get_option('package_rpm'),
  'package_tgz': get_option('package_tgz'),
}, section: 'Packaging')
