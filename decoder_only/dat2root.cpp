#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <stdint.h>
#include <arpa/inet.h>
#include <filesystem>

#include "TFile.h"
#include "TTree.h"

using namespace std;

#pragma pack(push, 1)
struct EventHeader {
  uint32_t marker_begin;
  uint32_t header_size;
  uint32_t payload_size;
  uint64_t event_id;
  uint64_t sec;
  uint64_t nsec;
  uint32_t trigger_counter;
  uint32_t flags;
  uint32_t spare;
};

struct BoardHeader {
  uint32_t marker_begin;
  uint32_t header_size;
  uint32_t payload_size;
  int board_id;
};

struct EventTrailer {
  uint32_t checksum;
  uint32_t marker_end;
};
#pragma pack(pop)

void decode_8_samples(const uint32_t* raw, uint16_t* out_adc) {

  uint32_t w[3];

  for (int i=0;i<3;i++) {
    w[i] = ntohl(raw[i]);
  }
  out_adc[0] =  w[0] & 0xFFF;
  out_adc[1] = (w[0] >> 12) & 0xFFF;
  out_adc[2] = ((w[0] >> 24) & 0xFF) | ((w[1] & 0xF) << 8);
  out_adc[3] = (w[1] >> 4) & 0xFFF;
  out_adc[4] = (w[1] >> 16) & 0xFFF;
  out_adc[5] = ((w[1] >> 28) & 0xF) | ((w[2] & 0xFF) << 4);
  out_adc[6] = (w[2] >> 8) & 0xFFF;
  out_adc[7] = (w[2] >> 20) & 0xFFF;
}

int main (int argc, char** argv) {
  if (argc < 2) {
    cout << "Usage: ./decoder <raw_data_file>" << endl;
    return 1;
  }

  
  const int number_of_boards = 2;
  const int number_of_groups = 4;
  const int ch_per_group     = 8;
  const int total_ch         = number_of_groups * ch_per_group;
  const int sample_n = 1024; 

  ifstream ifs(argv[1], ios::binary);
  if (!ifs) return 1;

  std::filesystem::path rootfilename = argv[1];
  rootfilename.replace_extension(".root");
  
  // ROOT output
  TFile *fout = new TFile(rootfilename.string().c_str(), "RECREATE");
  TTree *tree = new TTree("tree", "Waveform tree");

  // TTree Variables
  Long64_t ev_id;
  // Branches
  tree->Branch("ev_id",     &ev_id);


  // Amplitude
  vector<vector<array<uint16_t, 1024>>> amp(
    number_of_boards,
    vector<array<uint16_t, 1024>>(total_ch)
  );

  for (int b = 0; b < number_of_boards; b++) {
    for (int ch = 0; ch < total_ch; ch++) {
      string bname = Form("amp_b%d_ch%02d", b, ch);
      tree->Branch(bname.c_str(),
                   amp[b][ch].data(),
                   Form("%s[%d]/s", bname.c_str(), sample_n));
    }
  }
  
  int event_count = 0;
  
  while (true) {
    EventHeader eh;
    if (!ifs.read(reinterpret_cast<char*>(&eh), sizeof(EventHeader)))
      break;

    event_count++;

    ev_id = (Long64_t)eh.event_id;

    for (int b = 0; b < number_of_boards; b++)
      for (int ch = 0; ch < total_ch; ch++)
        amp[b][ch].fill(0);

    BoardHeader bh[number_of_boards];
    for (int b = 0; b < number_of_boards; b++) {
      ifs.read(reinterpret_cast<char*>(&bh[b]), sizeof(BoardHeader));
    }

    for (int b = 0; b < number_of_boards; b++) {
      // V1742 raw data
      vector<uint32_t> v1742_raw(bh[b].payload_size / 4);
      ifs.read(reinterpret_cast<char*>(v1742_raw.data()), bh[b].payload_size);

      // --- V1742 packet data
      // Main Header (4 words)
      uint32_t group_mask = ntohl(v1742_raw[1]) & 0xF;
      size_t idx = 4; 

      for (int g = 0; g < number_of_groups; g++) {
	if (!(group_mask & (1 << g)))
	  continue;

	// Group Event Description Word (P43)
	uint32_t grp_desc = ntohl(v1742_raw[idx++]); 

	// Data Payload (Ch0 - Ch7)
	for (int s_block = 0; s_block < sample_n ; s_block++) {
	  uint16_t adcs[8];
	  decode_8_samples(&v1742_raw[idx], adcs); // at certain time and 8 channels
	  for (int ich = 0; ich < 8; ich++) { //channel
	    int global_ch = g * ch_per_group + ich;
	    amp[b][global_ch][s_block] = adcs[ich];
	    // cout << "A: [" << global_ch << "]" << s_block << ": " << adcs[ich] << endl;
	  }
	  idx += 3;
	}
	
	// Group n Time Tag 
	uint32_t grp_time_tag = ntohl(v1742_raw[idx++]); 
      }
    }
    // EventTrailer
    EventTrailer et;
    ifs.read(reinterpret_cast<char*>(&et), sizeof(EventTrailer));
    tree->Fill();
  }

  
  
  fout->Write();
  fout->Close();

  cout << "Done. " << event_count << " events written." << endl;

  return 0;
}


