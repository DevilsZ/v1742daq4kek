#include <iostream>
#include <vector>
#include <cstring>
#include <ctime>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include "vmelib.h"
#include <chrono>

using namespace std;

////////////////////////////
struct EventHeader {
  uint32_t marker_begin;
  uint32_t header_size;
  uint32_t payload_size;
  uint64_t event_id;
  uint64_t sec;
  uint64_t nsec;
  uint32_t board_id;
  uint32_t trigger_counter;
  uint32_t flags;
};
struct EventTrailer {
  uint32_t checksum;
  uint32_t marker_end;
};

#define MY_MARKER_BEGIN 0xAAAA1111
#define MY_MARKER_END   0xEEEE2222
#define V1742_EVENT_FIFO       0x0000
#define V1742_EVENT_STORED     0x812C
#define V1742_EVENT_SIZE       0x814C

typedef vector<uint8_t> DataPacket;
queue<DataPacket> data_queue;
mutex mtx;
condition_variable cv;
atomic<bool> running(true);
const size_t MAX_QUEUE_SIZE = 1000; 


class DataWriter {
public:
  enum Mode { FILE_OUT, SOCKET_OUT } mode;
  FILE* fp = nullptr;
  int sock = -1;

  void write(const DataPacket& packet) {
    if (mode == FILE_OUT && fp) {
      fwrite(packet.data(), 1, packet.size(), fp);
    } else if (mode == SOCKET_OUT && sock >= 0) {
      send(sock, packet.data(), packet.size(), 0);
    }
  }
};


void sender_thread_func(DataWriter* writer) {
  while (running || !data_queue.empty()) {
    DataPacket packet;
    {
      unique_lock<mutex> lock(mtx);
      cv.wait(lock, [] { return !data_queue.empty() || !running; });
      if (data_queue.empty()) break;

      packet = std::move(data_queue.front());
      data_queue.pop();
    }
    writer->write(packet);
    cout << "write..." << endl;
  }
  cout << "Sender thread finished." << endl;
}


int main(int argc, char** argv) {
  if (argc < 2) {
    cout << "Usage: " << argv[0] << " [file|socket]" << endl;
    return -1;
  }

  DataWriter writer;
  if (string(argv[1]) == "socket") {
    writer.mode = DataWriter::SOCKET_OUT;
    writer.sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {AF_INET, htons(9000)};
    inet_pton(AF_INET, "192.168.0.1", &addr.sin_addr);
    if (connect(writer.sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
  } else {
    writer.mode = DataWriter::FILE_OUT;
    writer.fp = fopen("data.dat", "wb");
  }

  VMEBridge vme;
  struct BoardConfig { vme_addr base; int image; uint32_t id; };
  vector<BoardConfig> boards = {{0x10000000, -1, 0}, {0x30000000, -1, 1}};

  for (auto &b : boards) {
    b.image = vme.getImage(b.base, 0x100000, A32, D32, MASTER);
    if (b.image < 0) return -1;
  }

  vme.setOption(DMA, BLT_ON);
  void* dma_ptr = vme.requestDMA();
  if(!dma_ptr){
    printf("Can't allocate DMA !\n");
    return 0;
  }


  uint64_t global_event_id = 0;

  // 送信スレッドの起動
  //if(writer.mode == DataWriter::SOCKET_OUT){
  thread sender(sender_thread_func, &writer);
    //}


    // 1. Acquisition Start (念のため)
  unsigned int start_data = 0x4;
  vme.wl(boards[0].image, boards[0].base + 0x8100, ntohl(start_data));
  vme.wl(boards[1].image, boards[1].base + 0x8100, ntohl(start_data));

  cout << "DAQ Started. Press Ctrl+C to stop (not handled here for simplicity)." << endl;

  auto last_trigger_time = std::chrono::steady_clock::now();

  auto numbers=10;
  auto ievent=0;

  while (running && ievent < numbers) {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_trigger_time).count() >= 1) {
      cout << "Issuing Software Trigger..." <<ievent<<endl;
      uint32_t dummy = 1;
      for (auto &b : boards) {
	// Software Trigger Registerへ書き込み
	vme.wl(b.image, b.base + 0x8108, ntohl(dummy));
      }
      ievent++;
      last_trigger_time = now;
    }


    for (auto &b : boards) {
      uint32_t status = 0;
      vme.rl(b.image, b.base + V1742_EVENT_STORED, &status);
      status = ntohl(status); 

      if ( status > 0) {

	uint32_t word_size;
	vme.rl(b.image, b.base + V1742_EVENT_SIZE, &word_size);
	word_size = ntohl(word_size);


	uint32_t payload_bytes = word_size ;

	int offset = vme.DMAread(b.base + V1742_EVENT_FIFO, payload_bytes, A32, D32);
	if (offset < 0) continue;

	// --- パケット構築（DAQスレッド内で実行） ---
	DataPacket packet(sizeof(EventHeader) + payload_bytes + sizeof(EventTrailer));
	uint8_t* p = packet.data();

	EventHeader h = {MY_MARKER_BEGIN, 
			 sizeof(EventHeader), 
			 payload_bytes, 
			 //global_event_id++
			 ievent};
	struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
	h.sec = ts.tv_sec; h.nsec = ts.tv_nsec; h.board_id = b.id;

	memcpy(p, &h, sizeof(h)); p += sizeof(h);
	memcpy(p, (char*)dma_ptr + offset, payload_bytes); p += payload_bytes;
	EventTrailer t = {0, MY_MARKER_END};
	memcpy(p, &t, sizeof(t));
	cout<<"Board=0x"<<b.id<<" Event=0x"<<status<<"  Word Size=0x"<<word_size<<"  DMAread="<<offset<<endl;


	// --- キューへ投入 ---
	{
	  lock_guard<mutex> lock(mtx);
	  if (data_queue.size() < MAX_QUEUE_SIZE) {
	    data_queue.push(std::move(packet)); // moveで効率化
	    cout<<" packet inserted "<<endl;
	  } else {
	    // キューが溢れたら警告（古いデータを捨てるなどの処理も可）
	    cerr << "Queue Full! Data dropped." << endl;
	  }
	}
	cv.notify_one();
      }
    }
    usleep(1); 
  }

  // 終了処理
  running = false;
  cv.notify_one();
  //if(writer.mode == DataWriter::SOCKET_OUT){
    sender.join();
    //}
  if (writer.fp) fclose(writer.fp);
  if (writer.sock >= 0) close(writer.sock);

  vme.releaseDMA();
  return 0;
}
