#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "daffy/core/error.hpp"

namespace daffy::voice {

class SampleRateConverter {
 public:
  struct Impl;

  SampleRateConverter();
  SampleRateConverter(SampleRateConverter&& other) noexcept;
  SampleRateConverter& operator=(SampleRateConverter&& other) noexcept;
  SampleRateConverter(const SampleRateConverter&) = delete;
  SampleRateConverter& operator=(const SampleRateConverter&) = delete;
  ~SampleRateConverter();

  static core::Result<SampleRateConverter> Create(int input_sample_rate,
                                                  int output_sample_rate,
                                                  int channels = 1);

  [[nodiscard]] bool IsLoaded() const;
  [[nodiscard]] const std::string& library_path() const;
  [[nodiscard]] int input_sample_rate() const;
  [[nodiscard]] int output_sample_rate() const;

  core::Result<std::vector<float>> Process(const float* samples,
                                           std::size_t frame_count,
                                           bool end_of_input = false);
  core::Result<std::vector<float>> Process(const std::vector<float>& samples, bool end_of_input = false);
  core::Status Reset();

 private:
  explicit SampleRateConverter(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

class NoiseSuppressor {
 public:
  struct Impl;

  NoiseSuppressor();
  NoiseSuppressor(NoiseSuppressor&& other) noexcept;
  NoiseSuppressor& operator=(NoiseSuppressor&& other) noexcept;
  NoiseSuppressor(const NoiseSuppressor&) = delete;
  NoiseSuppressor& operator=(const NoiseSuppressor&) = delete;
  ~NoiseSuppressor();

  static core::Result<NoiseSuppressor> Create();

  [[nodiscard]] bool IsLoaded() const;
  [[nodiscard]] const std::string& library_path() const;
  [[nodiscard]] std::size_t frame_samples() const;

  core::Result<float> ProcessFrame(float* samples, std::size_t sample_count);

 private:
  explicit NoiseSuppressor(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace daffy::voice
