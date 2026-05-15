#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <stdint.h>
#include <arpa/inet.h>

#include "TFile.h"
#include "TTree.h"

#include <filesystem>

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

  const int number_of_boards=2;
  const int sample_n = 1024; 


  ifstream ifs(argv[1], ios::binary);
  if (!ifs) return 1;

  int event_count = 0;

  std::filesystem::path rootfilename = argv[1];
  rootfilename.replace_extension(".root");
  
  // ROOT output
  TFile *fout = new TFile(rootfilename.string().c_str(), "RECREATE");
  TTree *tree = new TTree("tree", "Waveform tree");

  // TTree Variables
  int nsamples;
  int boardid;
  int channelid;
  vector<unsigned short> amplitude;
  
  // Branches
  tree->Branch("nsamples",  &nsamples);
  tree->Branch("boardid",   &boardid);
  tree->Branch("channelid", &channelid);
  tree->Branch("amplitude", &amplitude);
  
  while (true) {
    EventHeader eh;
    if (!ifs.read(reinterpret_cast<char*>(&eh), sizeof(EventHeader))) break;

    cout << "========================================" << endl;
    cout << " Event ID: " << eh.event_id << " (Count: " << ++event_count << ")" << endl;
    cout << "========================================" << endl;

    BoardHeader bh[number_of_boards];
    
    for (int b = 0; b < number_of_boards; b++) {
      ifs.read(reinterpret_cast<char*>(&bh[b]), sizeof(BoardHeader));

      cout<<" Board "<<bh[b].board_id<<" payload_size = "<<bh[b].payload_size<<endl;

    }

    for (int b = 0; b < number_of_boards; b++) {
      // V1742 raw data
      vector<uint32_t> v1742_raw(bh[b].payload_size / 4);
      ifs.read(reinterpret_cast<char*>(v1742_raw.data()), bh[b].payload_size);

      cout<<" 1010 + Total Event size "<<hex<<ntohl(v1742_raw[0])<<endl;
      cout<<" Board ID + Pattern + Mask "<<hex<<ntohl(v1742_raw[1])<<endl;
      cout<<" Event Counter "<<hex<<ntohl(v1742_raw[2])<<endl;
      cout<<" Event Time Tag "<<hex<<ntohl(v1742_raw[3])<<endl;

      // --- V1742 packet data
      // Main Header (4 words)
      uint32_t group_mask = ntohl(v1742_raw[1]) & 0xF;
      size_t idx = 4; 

      for (int g = 0; g < 4; g++) {
	if (!(group_mask & (1 << g))) continue;

	// 1. Group n Event Description Word (P43)
	uint32_t grp_desc = ntohl(v1742_raw[idx]); 
	cout<<" Group "<<g<<" Start index cell / 0 0 FREQ 000 / Size CH0..7 "<<hex<<grp_desc<<endl;
	idx++; 


	// 2. Data Payload (Ch0 - Ch7)
	vector<vector<uint16_t>> waveforms(8, vector<uint16_t>(sample_n));
	for (int s_block = 0; s_block < sample_n ; s_block++) {
	  uint16_t adcs[8];
	  decode_8_samples(&v1742_raw[idx], adcs); // at certain time and 8 channels
	  for (int i = 0; i < 8; i++) { //channel
	    waveforms[i][s_block] = adcs[i];
	  }
	  idx += 3;
	}

	// 3. TRn samples but rel
	//idx += (3 * sample_n / 8); 
	
	// 4. Group n Time Tag 
	uint32_t grp_time_tag = ntohl(v1742_raw[idx]); 
	cout<<idx<<" grp_time_tag "<<hex<<grp_time_tag<<endl;
	idx++;
	cout<<" index "<<dec<<idx<<endl;
	// dump the waveform in txt
	amplitude.clear();
	for (int ch = 0; ch < 8; ch++) {
	  cout <<dec<< "Board" << b << " Group" << g << " ch" << (g * 8 + ch) << endl;
	  for (int s = 0; s < sample_n; s++) {
	    //amplitude[b][g * 8 + ch][s] = waveforms[ch][s];
	    amplitude.push_back(waveforms[ch][s]);
	    cout << setw(5) << setfill(' ') << waveforms[ch][s] << " ";
	    if ((s + 1) % 16  == 0) cout << endl;
	  }
	  cout << endl;
	  channelid = g*8 + ch;
	  boardid = b;
	  tree->Fill();
  
	}
      }
    }
    // EventTrailer
    EventTrailer et;
    ifs.read(reinterpret_cast<char*>(&et), sizeof(EventTrailer));
  }

  fout->Write();
  fout->Close();

  return 0;
}


