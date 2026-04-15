#pragma once

#include <memory>
#include <string>

#include "daffy/voice/portaudio_runtime.hpp"
#include "portaudio.h"

namespace daffy::voice {

struct PortAudioRuntime::Impl {
  using GetVersionInfoFn = const PaVersionInfo* (*)();
  using InitializeFn = PaError (*)();
  using TerminateFn = PaError (*)();
  using GetDeviceCountFn = PaDeviceIndex (*)();
  using GetDefaultDeviceFn = PaDeviceIndex (*)();
  using GetDeviceInfoFn = const PaDeviceInfo* (*)(PaDeviceIndex);
  using GetHostApiInfoFn = const PaHostApiInfo* (*)(PaHostApiIndex);
  using IsFormatSupportedFn = PaError (*)(const PaStreamParameters*, const PaStreamParameters*, double);
  using OpenStreamFn = PaError (*)(PaStream**,
                                   const PaStreamParameters*,
                                   const PaStreamParameters*,
                                   double,
                                   unsigned long,
                                   PaStreamFlags,
                                   PaStreamCallback*,
                                   void*);
  using StartStreamFn = PaError (*)(PaStream*);
  using StopStreamFn = PaError (*)(PaStream*);
  using AbortStreamFn = PaError (*)(PaStream*);
  using CloseStreamFn = PaError (*)(PaStream*);
  using IsStreamActiveFn = PaError (*)(PaStream*);
  using GetErrorTextFn = const char* (*)(PaError);

  void* handle{nullptr};
  std::string library_path;
  GetVersionInfoFn get_version_info{nullptr};
  InitializeFn initialize{nullptr};
  TerminateFn terminate{nullptr};
  GetDeviceCountFn get_device_count{nullptr};
  GetDefaultDeviceFn get_default_input_device{nullptr};
  GetDefaultDeviceFn get_default_output_device{nullptr};
  GetDeviceInfoFn get_device_info{nullptr};
  GetHostApiInfoFn get_host_api_info{nullptr};
  IsFormatSupportedFn is_format_supported{nullptr};
  OpenStreamFn open_stream{nullptr};
  StartStreamFn start_stream{nullptr};
  StopStreamFn stop_stream{nullptr};
  AbortStreamFn abort_stream{nullptr};
  CloseStreamFn close_stream{nullptr};
  IsStreamActiveFn is_stream_active{nullptr};
  GetErrorTextFn get_error_text{nullptr};

  ~Impl();
};

core::Result<PortAudioStreamSession> OpenPortAudioStreamSession(const std::shared_ptr<PortAudioRuntime::Impl>& impl,
                                                                const VoiceSessionPlan& plan);

}  // namespace daffy::voice
