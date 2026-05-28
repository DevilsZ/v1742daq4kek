#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <stdint.h>
#include <filesystem>

#include "TFile.h"
#include "TTree.h"

#include "v1742Event.hpp"
#include "headers.h"

using namespace std;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
const int NUM_BOARDS   = 2;
const int NUM_GROUPS   = 4;
const int CH_PER_GROUP = 8;
const int TOTAL_CH     = NUM_GROUPS * CH_PER_GROUP; // 32
const int SAMPLE_N     = 1024;

// ---------------------------------------------------------------------------
// Read exactly one event from the file into a contiguous buffer.
//
// Binary layout on disk:
//   EventHeader
//   BoardHeader[0]
//   BoardHeader[1]
//   BoardData[0]   (bh[0].payload_size bytes)
//   BoardData[1]   (bh[1].payload_size bytes)
//   EventTrailer
//
// EventHeader.payload_size counts only the raw board data, NOT the
// BoardHeaders, so we read the BoardHeaders explicitly first and then
// compute the true total size.
//
// Returns false when EOF is reached or a read fails.
// ---------------------------------------------------------------------------
static bool ReadOneEvent(ifstream& ifs, vector<uint8_t>& buf, int num_boards)
{
    // 1. Read EventHeader
    EventHeader eh;
    if (!ifs.read(reinterpret_cast<char*>(&eh), sizeof(EventHeader)))
        return false;

    // 2. Read all BoardHeaders upfront
    vector<BoardHeader> bhs(num_boards);
    for (int b = 0; b < num_boards; b++) {
        if (!ifs.read(reinterpret_cast<char*>(&bhs[b]), sizeof(BoardHeader)))
            return false;
    }

    // 3. Calculate total buffer size
    uint32_t board_data_total = 0;
    for (int b = 0; b < num_boards; b++)
        board_data_total += bhs[b].payload_size;

    const uint32_t total = sizeof(EventHeader)
                         + num_boards * sizeof(BoardHeader)
                         + board_data_total
                         + sizeof(EventTrailer);
    buf.resize(total);

    // 4. Assemble: copy already-read headers into buffer
    uint32_t offset = 0;
    memcpy(buf.data() + offset, &eh, sizeof(EventHeader));
    offset += sizeof(EventHeader);

    for (int b = 0; b < num_boards; b++) {
        memcpy(buf.data() + offset, &bhs[b], sizeof(BoardHeader));
        offset += sizeof(BoardHeader);
    }

    // 5. Read board data payloads
    for (int b = 0; b < num_boards; b++) {
        if (!ifs.read(reinterpret_cast<char*>(buf.data() + offset),
                      bhs[b].payload_size))
            return false;
        offset += bhs[b].payload_size;
    }

    // 6. Read EventTrailer
    if (!ifs.read(reinterpret_cast<char*>(buf.data() + offset),
                  sizeof(EventTrailer)))
        return false;

    return true;
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    if (argc < 2) {
        cout << "Usage: ./dat2root <raw_data_file> [output_dir]" << endl;
        return 1;
    }

    // Determine output file path
    filesystem::path rootfilename = argv[1];
    rootfilename.replace_extension(".root");
    if (argc > 2) {
        filesystem::path prefix = argv[2];
        rootfilename = prefix / rootfilename.filename();
        cout << "Output: " << rootfilename << endl;
    }
    int max_event = 1000000;
    if (argc > 3) {
      max_event = atoi(argv[3]);
    }

    // Open input file
    ifstream ifs(argv[1], ios::binary);
    if (!ifs) {
        cerr << "Error: Cannot open input file: " << argv[1] << endl;
        return 1;
    }

    // Load pedestal tables
    auto pedestal_tables =
        V1742Event::LoadPedestalFiles(NUM_BOARDS, "pedestal_def");

    // Initialize parser
    V1742Event parser;
    parser.SetSamples(SAMPLE_N);
    parser.SetNumberOfBoards(NUM_BOARDS);

    // ROOT output
    TFile* fout = new TFile(rootfilename.string().c_str(), "RECREATE");
    TTree* tree = new TTree("tree", "Pedestal-corrected");

    Long64_t ev_id;
    tree->Branch("ev_id", &ev_id);

    // Branch arrays: float (pedestal-corrected), one per board x channel
    vector<vector<array<float, SAMPLE_N>>> amp(
        NUM_BOARDS,
        vector<array<float, SAMPLE_N>>(TOTAL_CH)
    );
    for (int b = 0; b < NUM_BOARDS; b++) {
        for (int ch = 0; ch < TOTAL_CH; ch++) {
            string bname = Form("amp_b%d_ch%02d", b, ch);
            tree->Branch(bname.c_str(),
                         amp[b][ch].data(),
                         Form("%s[%d]/F", bname.c_str(), SAMPLE_N));
        }
    }

    // Event loop
    int event_count = 0;
    vector<uint8_t> event_buf;

    while (ReadOneEvent(ifs, event_buf, NUM_BOARDS)) {

        if (event_count > max_event)
	  break;
      
        if (event_count % 10000 == 0)
            cout << "Progress: " << event_count << endl;

        int parsed = parser.Parse(event_buf.data(),
                                  static_cast<uint32_t>(event_buf.size()));
        if (parsed <= 0) {
            cerr << "Warning: Parse failed at event " << event_count
                 << " (ret=" << parsed << "). Skipping." << endl;
            event_count++;
            continue;
        }

        parser.ApplyPedestalCorrection(pedestal_tables);

        ev_id = static_cast<Long64_t>(parser.GetEventHeader().event_id);

        for (int b = 0; b < NUM_BOARDS; b++)
            for (int ch = 0; ch < TOTAL_CH; ch++)
                for (int s = 0; s < SAMPLE_N; s++)
                    amp[b][ch][s] = parser.GetCorrectedWaveform(b, ch, s);

        tree->Fill();
        event_count++;
    }

    fout->Write();
    fout->Close();

    cout << "Done. " << event_count << " events written to "
         << rootfilename << endl;

    return 0;
}

