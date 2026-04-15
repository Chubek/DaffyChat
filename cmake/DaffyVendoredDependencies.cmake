set(DAFFY_VENDORED_DEPENDENCIES
    binaryen
    coturn
    libdatachannel
    libsamplerate
    libsrtp
    lxc
    nng
    opus
    portaudio
    rnnoise
    spdlog
    uWebSockets
    wasm-micro-runtime)

foreach(dep IN LISTS DAFFY_VENDORED_DEPENDENCIES)
  set(dep_path "${PROJECT_SOURCE_DIR}/third_party/${dep}")
  if(EXISTS "${dep_path}")
    set("DAFFY_HAVE_${dep}" ON CACHE INTERNAL "Vendored dependency ${dep} is present")
    message(STATUS "Found vendored dependency ${dep}: ${dep_path}")
  else()
    set("DAFFY_HAVE_${dep}" OFF CACHE INTERNAL "Vendored dependency ${dep} is missing")
    message(STATUS "Vendored dependency ${dep} not found under third_party")
  endif()
endforeach()
