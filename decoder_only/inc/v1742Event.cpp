#include <iostream>
#include "v1742Event.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <sstream>




using namespace std;

int debug=0;

V1742Event::V1742Event() : num_boards_(0), num_samples_(1024)
{

}

std::vector<std::vector<std::vector<float>>> V1742Event::LoadPedestalFiles(int num_boards, const std::string& prefix){

  std::vector<std::vector<std::vector<float>>> pedestal_tables(
							       num_boards,
							       std::vector<std::vector<float>>
							       (32, std::vector<float>(1024, 2048.0f))
							       );


  for (int b = 0; b < num_boards; b++) {
    std::string filename = prefix + "_b" + std::to_string(b) + ".dat";
    if (num_boards == 1) {
       filename = prefix + ".dat";
    }
    
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
      std::cerr << "[WARNING] Cannot open pedestal file: " << filename 
                << ". Using default pedestal values (2048.0)." << std::endl;
      continue;
    }

    std::string line;
    int line_count = 0;

    while (std::getline(ifs, line)) {
      // コメント行や空行をスキップ
      if (line.empty() || line[0] == '#') continue;

      std::stringstream ss(line);
      int ch = -1;
      int cell_id = -1;
      float ped_avg = 0.0f;

      if (ss >> ch >> cell_id >> ped_avg) {
        if (ch >= 0 && ch < 32 && cell_id >= 0 && cell_id < 1024) {
          pedestal_tables[b][ch][cell_id] = ped_avg;
          line_count++;
        }
      }
    }

    std::cout << "[INFO] Loaded " << line_count << " pedestal entries from " << filename << std::endl;
    ifs.close();
  }

  return pedestal_tables;
}

  
int V1742Event::Parse(const uint8_t* data, uint32_t size) {

  if (num_boards_ <= 0) return -1;
    
  uint32_t offset = 0;


  // 1. EventHeader
  if (size < sizeof(EventHeader)) return 0;
  std::memcpy(&eh_, data + offset, sizeof(EventHeader));
  if (eh_.marker_begin != 0xAAAA1111) return -1;
  offset += sizeof(EventHeader);

  if(debug){
    cout << "========================================" << endl;
    cout << " Event ID: " << eh_.event_id
	 << " Trigger Counter ID: "<<eh_.trigger_counter
	 << " pPayload Size: "<<eh_.payload_size<<endl;
    cout << "========================================" << endl;
  }
  
  // 2. Boards Loop
  for (int b = 0; b < num_boards_; b++) {
    // BoardHeader
    if (size < offset + sizeof(BoardHeader)) return 0;
    std::memcpy(&boards_[b].bh, data + offset, sizeof(BoardHeader));
    if (boards_[b].bh.marker_begin != 0xBBBB1111) return -2;
    offset += sizeof(BoardHeader);

    if(debug){
      cout<<" Board ID : "<<boards_[b].bh.board_id
	  <<" Payload Size : "<<boards_[b].bh.payload_size<<endl;
    }
  }

  // 3. V1742 Main Data
  for (int b = 0; b < num_boards_; b++) {
    const uint32_t* raw32 = reinterpret_cast<const uint32_t*>(data + offset);
    for(int i=0; i<4; i++) boards_[b].v1742_main_header[i] = ntohl(raw32[i]);

    if(debug){
      cout<<" 1010 + Total Event size "<<hex<<boards_[b].v1742_main_header[0]<<endl;
      cout<<" Board ID + Pattern + Mask "<<hex<<boards_[b].v1742_main_header[1]<<endl;
      cout<<" Event Counter "<<hex<<boards_[b].v1742_main_header[2]<<endl;
      cout<<" Event Time Tag "<<hex<<boards_[b].v1742_main_header[3]<<endl;
    }
    uint32_t group_mask = boards_[b].v1742_main_header[1] & 0xF;
    uint32_t board_internal_offset = 16; // 4 words of v1742 main header

    for (int g = 0; g < 4; g++) {
      if (!(group_mask & (1 << g))) continue;

      const uint32_t* v1742_ptr = reinterpret_cast<const uint32_t*>(data + offset + board_internal_offset);

      // 1. Group n Event Description Word
      uint32_t grp_desc = ntohl(v1742_ptr[0]); 
      uint32_t start_index = (grp_desc >> 20) & 0x3FF; 
      if(debug){
	cout<<" Group "<<g<<" Start index "<<dec<<start_index<<" cell / 0 0 FREQ 000 / Size CH0..7 "<<hex<<grp_desc<<endl;
      }

      boards_[b].start_index[g] = start_index;
      
      uint32_t idx = 1; //offset by grp_desc
      
      // 2. Data Payload (Ch0 - Ch7)
      for (int s_block = 0; s_block < num_samples_ ; s_block++) {
	uint16_t adcs[8];
	decode_8_samples(&v1742_ptr[idx], adcs);
	for (int i = 0; i < 8; i++) { //channel
	  boards_[b].waveforms[g*8+i][s_block] = adcs[i];
	}
	idx += 3;
      }

      // 3. Group n Time Tag 
      uint32_t grp_time_tag = ntohl(v1742_ptr[idx]); 
      if(debug){
	cout<<idx<<" grp_time_tag "<<hex<<grp_time_tag<<endl;
      }
      idx++;
      if(debug){
	cout<<" index "<<dec<<idx<<endl;     
      }
      board_internal_offset += idx*4;

      //////////////////////////////////
      // dump the waveform in txt 
      if(debug){
	for (int ch = 0; ch < 8; ch++) {
	  cout <<dec<< "Board" << b << " Group" << g << " ch" << (g * 8 + ch) << endl;
	  for (int s = 0; s < num_samples_; s++) {
	    cout << setw(5) << setfill(' ') << boards_[b].waveforms[g*8+ch][s] << " ";
	    if ((s + 1) % 16  == 0) cout << endl;
	  }
	  cout << endl;
	}
      }
      ///////////////////////////////
      
    }
    offset += boards_[b].bh.payload_size;
  }

  std::memcpy(&et_, data + offset, sizeof(EventTrailer));
  offset += sizeof(EventTrailer);
      
  return offset;
}


void V1742Event::ApplyPedestalCorrection(const std::vector<std::vector<std::vector<float>>>& pedestal_tables) {

  for (int b = 0; b < num_boards_; b++) {
    if (b >= (int)pedestal_tables.size()) continue;

     for (int g = 0; g < 4; g++) {  
      uint32_t start_idx = boards_[b].start_index[g];
      for (int ch_in_group = 0; ch_in_group < 8; ch_in_group++) {
        int global_ch = g * 8 + ch_in_group;

        if (global_ch >= (int)pedestal_tables[b].size()) continue;

        for (int s = 0; s < num_samples_; s++) {
          
          int cell_id = (start_idx + s) % 1024;
          uint16_t raw_adc = boards_[b].waveforms[global_ch][s];

          float pedestal_val = pedestal_tables[b][global_ch][cell_id];
          float corrected_adc = (float)raw_adc - pedestal_val;

          boards_[b].corrected_waveforms[global_ch][s] = corrected_adc;
        }
      }
    }
  }
}

void V1742Event::decode_8_samples(const uint32_t* raw, uint16_t* out_adc) {
    uint32_t w[3];
    for(int i=0; i<3; i++) w[i] = ntohl(raw[i]);

    out_adc[0] =  w[0] & 0xFFF;
    out_adc[1] = (w[0] >> 12) & 0xFFF;
    out_adc[2] = ((w[0] >> 24) & 0xFF) | ((w[1] & 0xF) << 8);
    out_adc[3] = (w[1] >> 4) & 0xFFF;
    out_adc[4] = (w[1] >> 16) & 0xFFF;
    out_adc[5] = ((w[1] >> 28) & 0xF) | ((w[2] & 0xFF) << 4);
    out_adc[6] = (w[2] >> 8) & 0xFFF;
    out_adc[7] = (w[2] >> 20) & 0xFFF;
}
