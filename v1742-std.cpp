/* written by T. Gunji */

#include <iostream>
#include <vector>
#include <cstring>
#include <stdint.h>
#include <unistd.h>
#include "vmelib.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <err.h>
#include <time.h>
// Added by KK
#include <csignal>
#include <atomic>

using namespace std;

// Added by KK
std::atomic<bool> stop_flag(false);
void handler(int sig) {
  stop_flag = true;
}

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


#define V1742_EVENT_FIFO       0x0000
#define V1742_BOARD_CONFIG     0x8000
#define V1742_EVENT_SIZE       0x814C
#define V1742_EVENT_STORED     0x812C
#define V1742_SW_TRIGGER       0x8108
#define V1742_ACQ_CONTROL      0x8100
#define V1742_SW_RESET         0xEF24
#define V1742_SW_CLEAR         0xEF28
#define V1742_TRIG_SRC_EN_MASK 0x810C

struct Board {
  vme_addr base;
  int image;
  int id; 
  uint32_t last_event_size;
};


void write_data(VMEBridge &vme, Board &b, unsigned int addr, unsigned int data);
unsigned int read_data(VMEBridge &vme, Board &b, unsigned int addr);
bool wait_for_data(VMEBridge &vme, Board &b) ;
void board_config(VMEBridge &vme, Board &b);
string trig_mode = "software";


int main(int argc, char** argv) {
  
  if (argc < 6) {
    //cout << "Usage: " << argv[0] << " [# of event] [file|socket] [software|hardware] [production|debug]" << endl;
    cout << "Usage: " << argv[0] << " [# of event] [file|socket] [software|hardware] [production|debug] [runnumber]" << endl;
    return -1;
  }
  
  int nevent = atoi(argv[1]);
  // Added by KK
  if (nevent < 0) 
    nevent = 10000000-1; // very large number
  signal(SIGINT, handler);
  int total_number_recorded = 0;

  int div = nevent/10;
  string mode = argv[2];
  trig_mode = argv[3];
  string debug = argv[4];
  // Added by KK
  int runnumber = atoi(argv[5]);
  FILE* fp = nullptr;


  int data_sock = -1;
  // --- 1. scoket server mode ---
  if(mode=="socket"){
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //setsockopt(listen_sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr = {AF_INET, htons(2222), INADDR_ANY};
    bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(listen_sock, 1);
    
    cout << "--- DAQ Server waiting for Client connection on port 2222 ---" << endl;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    data_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
    
    if (data_sock < 0) {
      perror("Accept failed");
      return -1;
    }
    cout << "Client connected. Starting DAQ initialization..." << endl;
  }else{
    // Modified by KK
    //fp = fopen("combined_data.dat", "wb");
    string filename = "run_" + to_string(runnumber) + ".dat";
    fp = fopen(filename.c_str(), "wb");
  }

  VMEBridge vme;
  vector<Board> boards = {{0x10000000, -1, 0}, {0x30000000, -1, 1}};
  //vector<Board> boards = {{0x30000000, -1, 0}};
  uint32_t val=0;



  //buffer 
  vector<uint8_t> packet_buffer;
  packet_buffer.reserve(1024 * 1024 * 256);  //256kB

  // 1. VME image 
  for (auto &b : boards) {
    b.image = vme.getImage(b.base, 0x100000, A32, D32, MASTER);
    if (b.image < 0) return -1;

    // Configuration 
    //board_config(vme, boards);
  }

  vme.setOption(DMA, BLT_ON);
  void* dma_buffer = vme.requestDMA();
  if(!dma_buffer){
    printf("Can't allocate DMA !\n");
    return 0;
  }

  for (auto &b : boards) {
    if (trig_mode == "hardware") {
      val=0x40000000;
      write_data(vme, b, V1742_TRIG_SRC_EN_MASK, val);
      cout << "Board at " << hex << b.base << ": Hardware Trigger Enabled." << dec<<endl;
    } else {
      val=0x80000000;
      write_data(vme, b, V1742_TRIG_SRC_EN_MASK, val);
      cout << "Board at " << hex << b.base << ": Software Trigger Enabled. (1 Hz)" << dec<<endl;
    }

    // Start DAQ 
    val = 0x4;
    write_data(vme, b, 0x8100, val);
  }

  uint64_t ev_id = 0;
  cout << "Start DAQ Loop " << endl;

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  for (int i = 0; i < nevent; i++) { 
    // added by KK
    if (stop_flag) {
      // Stop DAQ when Ctrl+C detected
      std::cout << "Ctrl+C detected. Exit loop now" << std::endl;
      break;
    }

    // issue the software trigger
    if (trig_mode == "software") {
      uint32_t dummy = 1;
      for (auto &b : boards) {
	write_data(vme, b, V1742_SW_TRIGGER, dummy);
      }
    }

    uint32_t total_payload =0;
    size_t total_packet_size = 0;

    total_packet_size += sizeof(EventHeader);
    for (auto &b : boards) {
      // polling .....
      uint32_t n_events = 0;
      while (n_events ==0) {
	// added by KK
	if (stop_flag) {
	  std::cout << "Ctrl+C detected. Exit loop now" << std::endl;
	  break;
	}
	n_events = read_data(vme, b, V1742_EVENT_STORED);
      }
      // added by KK
      if (stop_flag)
	break;

      //if (!wait_for_data(vme, b)) {
      //cout << "Timeout on board " << hex << b.base << endl;
      //continue;
      //}
    
      //// Get the event Size 
      b.last_event_size = read_data(vme, b, V1742_EVENT_SIZE);
      b.last_event_size = 4*b.last_event_size;
      if(debug == "debug") cout << "Board ID : "<<b.id<<"  DataSize="<<b.last_event_size << endl;      
      total_payload += b.last_event_size;
      total_packet_size += sizeof(BoardHeader)+b.last_event_size; 
    }
    // added by KK
    if (stop_flag)
      break;
    
    total_packet_size += sizeof(EventTrailer);

    // buffer size reassigned 
    packet_buffer.resize(total_packet_size);
    uint8_t* p = packet_buffer.data();
    
    EventHeader h = {0xAAAA1111, sizeof(EventHeader), total_payload, ev_id++};
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    h.sec = ts.tv_sec; h.nsec = ts.tv_nsec; h.flags = 0; h.spare=0xffff;
    h.trigger_counter = i;
    memcpy(p, &h, sizeof(h));
    p += sizeof(h);

    // added by KK
    total_number_recorded = h.event_id;

    for (auto &b : boards) {
      BoardHeader b0 = {0xBBBB1111, sizeof(BoardHeader), 
			b.last_event_size,
			b.id};
      memcpy(p, &b0, sizeof(b0));
      p += sizeof(b0);
    }    
    
    // 4. DMA readout and copy to the buffer 
    for (auto &b : boards) {

      // Bus extention mode 
      // EF00 has to be 0x120
      uint32_t dma_size_bytes = b.last_event_size ;
      int offset = vme.DMAread(b.base + V1742_EVENT_FIFO, dma_size_bytes, A32, D32);
      if (offset >= 0) {
	memcpy(p, (char*)dma_buffer + offset, dma_size_bytes);
	p += dma_size_bytes;
      }
      if(debug =="debug") cout <<"  Board ID="<<b.id<<" readout size = " << dma_size_bytes << " bytes)" << endl;


      /// No Bus extended mode. Bit 8 of FE00 is 0x0
      /// read data for every 4096 bytes. this is limiuted by FIFO.
      /*
      uint32_t total_to_read = b.last_event_size; 
      uint32_t bytes_read = 0;

      while (bytes_read < total_to_read) {
        uint32_t remaining = total_to_read - bytes_read;
        uint32_t current_chunk = (remaining > 4096) ? 4096 : remaining;
        int offset = vme.DMAread(b.base + V1742_EVENT_FIFO, current_chunk, A32, D32);

        if (offset >= 0) {
	  memcpy(p, (char*)dma_buffer + offset, current_chunk);
	  p += current_chunk;
	  bytes_read += current_chunk;
        } else {
	  cerr << "DMA Error at Board " << b.id << "! Offset: " << offset << endl;
	  break; 
        }

	if (debug == "debug") {
	cout << "  Board ID=" << b.id << " read chunk: " << current_chunk << " bytes" << endl;
        }
      }
      */

    }
    
    // 5. Tailer
    EventTrailer t = {0, 0xEEEE2222};
    memcpy(p, &t, sizeof(t));
    
    // 6. send to socket 
    if (mode == "socket") {
      // all data is sent at only once 
      //ssize_t sent = send(data_sock, packet_buffer.data(), total_packet_size, 0);
      ssize_t sent = write(data_sock, 
			   packet_buffer.data(), 
			   total_packet_size);
      if (sent < 0) {
	cout << "Connection lost. Exiting DAQ." << endl;
	break;
      }

    } else if(mode == "file"){
      // all data is writte into file 
      fwrite(packet_buffer.data(), 1, total_packet_size, fp);
      fflush(fp);
    }
  
    if(trig_mode == "software") sleep(1);     
    if(debug == "debug"){
      cout << "Event " << h.event_id << " / "<<nevent<<" total transmitted (" << total_packet_size << " bytes)" << endl;
    }else if(debug == "production"){
      if(i%div==0){
	clock_gettime(CLOCK_MONOTONIC, &end);
	double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
	double total_time = elapsed;
	double avg_rate = i / total_time;
	cout <<" Event " << h.event_id << " / "<<nevent
	     <<" Transmitted (" << total_packet_size 
	     << " bytes) - Rate (Hz) = " <<avg_rate
	     << " Throughput (MB/s) = " <<total_packet_size*i*1.0e-06/total_time<< endl;
      }
    }
  }
  cout << "Cleaning up and stopping acquisition..." << endl;
  
  // added by KK
  cout << "Total number recorded is: " << total_number_recorded+1 << endl;

  // terminate DAQ ACD close 
  for (auto &b : boards) {
    vme.wl(b.image, b.base + V1742_ACQ_CONTROL, val); // Stop
  }

  // file close
  if (fp != nullptr) {
    fclose(fp);
    fp = nullptr;
    cout << "File closed." << endl;
  }

  // socket close 
  if (mode == "socket" && data_sock >= 0) {
    close(data_sock);
    cout << "Socket closed." << endl;
  }

  vme.releaseDMA();
  for (auto &b : boards) {
    vme.releaseImage(b.image);
  }

  cout << "DAQ process finished safely." << endl;
  return 0;
}

/////////////////////////////////////////////////////
bool wait_for_data(VMEBridge &vme, Board &b) {
  uint32_t status = 0;
  int timeout = 10000; 

  while (timeout--) {
    status = read_data(vme, b,0x8104); // Acquisition Status
    
    // Bit 3 (Event Ready) かつ Bit 8 (Event Stored) を確認
    if ((status & 0x8) && (status & 0x100)) {
      return true; 
    }
    usleep(10); // 
  }
  return false; //
}

/////////////////////////////////////////////////////
unsigned int read_data(VMEBridge &vme, Board &b, unsigned int addr){
  unsigned int val32;  
  if (!vme.rl(b.image, b.base+addr, &val32)){
    val32 = ntohl(val32);
  }
  usleep(1);
  return val32;
}

/////////////////////////////////////////////////////
 void write_data(VMEBridge &vme, Board &b, unsigned int addr, unsigned int data){

  unsigned int val32 = ntohl(data);
  vme.wl(b.image, b.base+addr, val32);
  usleep(1);

}

/////////////////////////////////////////////////////
void board_config(VMEBridge &vme, Board &b){

  //uint32_t val=1;
  // --- 1. SW Reset & Wait ---
  //val = 1; 
  //write_data(vme, b, 0xEF24, val);
  
  // --- 2. Sampling Frequency (5GHz) of DRS
  //val = 0;
  //write_data(vme, b, 0x80D8, val);
  
  // --- 3. Record Length (RECORD_LENGTH 520) ---
  //val = 0x2; //256 samples
  //write_data(vme, b, 0x8020, val); 
  
  // --- 4. Post Trigger (POST_TRIGGER 60) ---
  //val = 0x10; 
  //for (int g = 0; g < 4; g++) {
  //write_data(vme, b, 0x1014 + (g << 8), val);
  //}
  
  // --- 5. Busy Signal from GPO (0x811C の設定) ---
  // CONFIG内の 000D0000 は Bit 19, 18, 16 が 1
  // つまり MON Selection = 11b (Logic), Bit 16 = 1 (Enable)
  // 以前の議論通り Bit 20 は 0 (BUSY) のまま
  //val = 0x000D0000;
  //write_data(vme, b, 0x811C, val);
  
  // --- 6. Group / Channel Settings (ENABLE_INPUT YES / DC_OFFSET) ---
  // Group Enable Mask (全グループ有効)
  //val = 0xF;
  //write_data(vme, b, 0x8120, val);
  
  /*
  for (int g = 0; g < 4; g++) {
    // DC Offset (DC_OFFSET -10)
    // -10% は 16bit DAC で計算 (例: 0x8000(0V) から 10% 分オフセット)
    // ここでは便宜上、一般的なベースライン調整値を書き込み
    val = 0x6666; // -10% 相当の DAC 値
    write_data(vme, b, 0x1098 + (g << 8), val);
    //vme.wl(b.image, b.base + 0x1098 + (g << 8), ntohl(offset_val));
    
    // Correction Level (CORRECTION_LEVEL AUTO)
    // Bit 1:0 = 11b (全補正有効)
    val = 0x3;
    write_data(vme, b, 0x1020 + (g << 8), val);
    //vme.wl(b.image, b.base + 0x1020 + (g << 8), ntohl(val));
  }
  */
  // 2. front-pane IO setting 
  // Bit 19:18 を 11b (3) に、Bit 20 を 0 に設定
  // (3 << 18) = 0xC0000
  //uint32_t fpio_ctrl = (3 << 18); 
  //uint32_t current_val;
  //vme.rl(b.image, b.base + 0x811C, &current_val);
  //current_val = ntohl(current_val);
  //uint32_t next_val = (current_val & ~(1 << 20)) | fpio_ctrl;
  //vme.wl(b.image, b.base + 0x811C, ntohl(next_val));
  //usleep(200000);
  

  // --- 10. board config 
  //val=0x118; //testmode
  //val=0x110; //normal mode
  //write_data(vme, b, 0x8000, val);

  // --- 8. software clear 
  //val = 1;
  //write_data(vme, b, V1742_SW_CLEAR, val);


}

