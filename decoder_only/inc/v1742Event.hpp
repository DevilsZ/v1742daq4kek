#ifndef V1742_EVENT_HPP
#define V1742_EVENT_HPP

#include <vector>
#include <stdint.h>
#include <string>

#include "headers.h"

struct V1742BoardData {
  BoardHeader bh;
  uint32_t v1742_main_header[4];
  uint32_t start_index[4]; 
  std::vector<std::vector<uint16_t>> waveforms; // [ch][sample]
  std::vector<std::vector<float>> corrected_waveforms; // [ch][sample]
  
  
  void Resize(int ch_n, int sample_n) {
    waveforms.assign(ch_n, std::vector<uint16_t>(sample_n));
    corrected_waveforms.assign(ch_n, std::vector<float>(sample_n, 0.0f));
  }
  
};


class V1742Event {
public:
  V1742Event();

  void SetNumberOfBoards(int n) { 
    num_boards_ = n; 
    boards_.resize(n); 
    for(auto &b : boards_) b.Resize(32, num_samples_);
  }

  void SetSamples(int n) {
    num_samples_ = n;
    for(auto &b : boards_) b.Resize(32, num_samples_);
  }
  
  // parse data from buffer 
  int Parse(const uint8_t* data, uint32_t size);

  // apply pedestal correction 
  void ApplyPedestalCorrection(const std::vector<std::vector<std::vector<float>>>& pedestal_tables);

  static std::vector<std::vector<std::vector<float>>> LoadPedestalFiles(int num_boards, const std::string& prefix);
  
  
  const EventHeader& GetEventHeader() const { return eh_; }
  const V1742BoardData& GetBoardData(int b_idx) const { return boards_[b_idx]; }
  int GetNumberOfBoards() const { return num_boards_; }
  int GetSamples() const { return num_samples_; }

  float GetCorrectedWaveform(int b_idx, int ch, int sample) const {
    if (b_idx >= 0 && b_idx < num_boards_ && ch >= 0 && ch < 32 && sample >= 0 && sample < num_samples_) {
      return boards_[b_idx].corrected_waveforms[ch][sample];
    }
    return 0.0f;
  }

  
private:
  int num_boards_;
  int num_samples_;
  EventHeader eh_;
  EventTrailer et_;
  std::vector<V1742BoardData> boards_;
  
  void decode_8_samples(const uint32_t* raw, uint16_t* out_adc);
};

#endif
