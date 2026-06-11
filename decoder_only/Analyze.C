// Written by Kentaro Kawade, assisted by AI chat
// Usage: root -l 'Analyze.C("data.root")'

#include <set>
#include <vector>
#include <numeric>

// -----------------------------------------------------------------------
// PulseShape
// -----------------------------------------------------------------------
struct PulseShape {
  int   board_id;
  int   ch_id;
  float pedestal;
  bool  hit;
  int   peak_index;
  float t_lead;
  float t_trail;
  float tot;
  float charge;
  float min_adc;
};

// -----------------------------------------------------------------------
// AnalyzeWaveform
// -----------------------------------------------------------------------
// Compute Median, more robust but slower
float ComputePedestalMedian(const Float_t* amp, int n_ped) {
  std::vector<Float_t> buf(amp, amp + n_ped);
  std::nth_element(buf.begin(), buf.begin() + n_ped/2, buf.end());
  return buf[n_ped / 2];
}

std::vector<PulseShape> AnalyzeWaveform(const Float_t* amp,
                                        int board_id, int ch_id,
                                        int sample_n, float threshold,
                                        float sampling_interval, float min_tot) {
  std::vector<PulseShape> results;
  bool  in_pulse   = false;
  int   peak_idx   = 0;
  float t_lead     = 0.0f;
  float charge_acc = 0.0f;
  float min_adc    = 0.0f;

  // Pedestal: sigma-clipping median of first 50 samples
  float pedestal = ComputePedestalMedian(amp, 50);

  // Subtract pedestal from all samples before threshold processing.
  // min_adc and charge are therefore pedestal-subtracted values.
  for (int s = 0; s < sample_n; s++) {
    float val  = amp[s] - pedestal; // pedestal-subtracted
    bool  over = (val < threshold); // True if val exceed threshold level

    float val_prev = (s > 0) ? amp[s-1] - pedestal : 0.0f;  // ped-subtracted previous sample

    if (over && !in_pulse) {
      float t_cross = s * sampling_interval;
      if (s > 0) {
        float frac = (threshold - val_prev) / (val - val_prev);
        t_cross = (s - 1 + frac) * sampling_interval;
      }
      t_lead = t_cross; charge_acc = 0.0f;
      min_adc = val;
      in_pulse = true;
    }
    if (in_pulse) {
      charge_acc += (-val - threshold) * sampling_interval;
      if (val < min_adc)
	min_adc = val;
    }
    if (in_pulse && (!over || s == sample_n - 1)) {
      float t_trail = s * sampling_interval;
      if (!over && s > 0) {
        float frac = (threshold - val_prev) / (val - val_prev);
        t_trail = (s - 1 + frac) * sampling_interval;
      }
      PulseShape r;
      r.board_id   = board_id;
      r.ch_id      = ch_id;
      r.pedestal   = pedestal;
      r.peak_index = peak_idx;
      r.t_lead     = t_lead;
      r.t_trail    = t_trail;
      r.tot        = t_trail - t_lead;
      r.charge     = charge_acc;
      r.min_adc    = min_adc;
      // Hit decision by ToT
      if (t_trail - t_lead > min_tot)
	r.hit        = true;
      // push result
      results.push_back(r);
      peak_idx++;
      in_pulse = false;
    }
  }
  return results;
}

// -----------------------------------------------------------------------
// Storage structs for two-pass alignment correction
// -----------------------------------------------------------------------
struct StripFourLayerData {
  double x1_mm, y1_mm;   // Front X, Front Y centroid [mm]
  double x2_mm, y2_mm;   // Back  X, Back  Y centroid [mm]
};

struct SixLayerData {
  double x1_mm, y1_mm, x2_mm, y2_mm;  // Strip centroids [mm]
  double pix_row[2], pix_col[2];        // Pixel row/col centroid per layer
};

// -----------------------------------------------------------------------
// Analyze
// -----------------------------------------------------------------------
void Analyze(const char* filename) {

  // -----------------------------------------------------------------------
  // Configuration
  // -----------------------------------------------------------------------
  const int   NBOARDS           = 2;
  const int   N_CH              = 32;
  const int   SAMPLE_N          = 1024;
  const float THRESHOLD         = -10.0f;
  const float SAMPLING_INTERVAL = 1.0f;
  const int   MAX_LAYERS        = 4;
  const int   reso_factor       = 4;

  // Strip: 0.5 mm pitch
  const float STRIP_PITCH = 0.5f;   // mm per strip

  // Pixel: col -> X, row -> Y
  // Set PIXEL_PITCH to convert pixel units to mm if needed
  const float PIXEL_PITCH = 1.0f;   // mm per pixel (adjust to actual geometry)

  const char* BOARD_NAMES[NBOARDS]  = {"Strip", "Pixel"};
  const int   N_LAYERS[NBOARDS]     = {4, 2};
  const char* LAYER_NAMES[NBOARDS][MAX_LAYERS] = {
    {"Front X", "Front Y", "Back X", "Back Y"},
    {"Front",   "Back",    nullptr,  nullptr  }
  };
  const int LAYER_START[NBOARDS][MAX_LAYERS] = {
    {0,  8, 16, 24},
    {0, 16,  0,  0}
  };
  const int CH_PER_LAYER[NBOARDS] = {8, 16};

  // Z positions [mm] -- adjust to actual geometry
  const float Z_STRIP_FRONT = 0.0f;
  const float Z_STRIP_BACK  = 375.0f;
  const float Z_PIXEL_FRONT = 500.0f;
  const float Z_PIXEL_BACK  = 520.0f;

  // Pixel channel mapping: local_ch (0-15) -> row / col in a 4x4 grid
  // col -> X direction, row -> Y direction
  const int PIXEL_NROW = 4;
  const int PIXEL_NCOL = 4;
  const int mapping_row[16] = {3, 3, 2, 2, 1, 1, 0, 0,
                                0, 0, 1, 1, 2, 2, 3, 3};
  const int mapping_col[16] = {1, 0, 1, 0, 1, 0, 1, 0,
                                3, 2, 3, 2, 3, 2, 3, 2};

  const float min_tot[NBOARDS][4] = {
    {2.0, 2.0, 2.0, 2.0},
    {1.5, 1.5, 1.5, 1.5}
  };
  
  // Dead channel list per board (global ch index within the board)
  const std::set<int> DEAD_CH[NBOARDS] = {
    {},        // Board 0 (Strip): none
    {},    // Board 1 (Pixel): list dead channels here
  };

  // -----------------------------------------------------------------------
  // Open file / tree
  // -----------------------------------------------------------------------

  TFile* f = TFile::Open(filename);
  if (!f || f->IsZombie()) {
    cerr << "Cannot open: " << filename << endl;
    return;
  }
  // Get run_number for filename
  char run_number[32];
  sscanf(filename, "run_%[^.].root", run_number);

  cout << "Run number:" << run_number << endl;
  
  TTree* tree = (TTree*)f->Get("tree");
  if (!tree) {
    cerr << "TTree 'tree' not found." << endl;
    return;
  }

  Long64_t ev_id;
  tree->SetBranchAddress("ev_id", &ev_id);

  Float_t amp[NBOARDS][N_CH][SAMPLE_N];
  for (int b = 0; b < NBOARDS; b++) {
    for (int ch = 0; ch < N_CH; ch++) {
      tree->SetBranchAddress(Form("amp_b%d_ch%02d", b, ch), amp[b][ch]);
    }
  }
  
  // -----------------------------------------------------------------------
  // Histograms -- per channel
  // -----------------------------------------------------------------------
  TH1F* hMinAmp_ch  [NBOARDS][N_CH];
  TH1F* hPedestal_ch[NBOARDS][N_CH];
  TH1F* hTlead_ch   [NBOARDS][N_CH];
  TH1F* hToT_ch     [NBOARDS][N_CH];
  TH1F* hCharge_ch  [NBOARDS][N_CH];
  for (int b = 0; b < NBOARDS; b++) {
    for (int ch = 0; ch < N_CH; ch++) {
      hMinAmp_ch[b][ch]   = new TH1F(Form("hMinAmp_b%d_ch%02d",b,ch),
				     Form("%s Ch%02d min amplitude;Min ADC [counts];Entries",BOARD_NAMES[b],ch),
				     200,-4000.,0.);
      hPedestal_ch[b][ch] = new TH1F(Form("hPedestal_b%d_ch%02d",b,ch),
				     Form("%s Ch%02d pedestal;ADC [counts];Entries",BOARD_NAMES[b],ch),
				     400,-200.,200.);
      hTlead_ch[b][ch]    = new TH1F(Form("hTlead_b%d_ch%02d",b,ch),
				     Form("%s Ch%02d leading edge;t_lead [s];Entries",BOARD_NAMES[b],ch),
				     200,0.,SAMPLE_N*SAMPLING_INTERVAL);
      hToT_ch[b][ch]      = new TH1F(Form("hToT_b%d_ch%02d",b,ch),
				     Form("%s Ch%02d ToT;ToT [s];Entries",BOARD_NAMES[b],ch),
				     100,0.,25.*SAMPLING_INTERVAL);
      hCharge_ch[b][ch]   = new TH1F(Form("hCharge_b%d_ch%02d",b,ch),
				     Form("%s Ch%02d charge;Charge [ADC*s];Entries",BOARD_NAMES[b],ch),
				     200,0.,500.*SAMPLING_INTERVAL);
    }
  }

  // -----------------------------------------------------------------------
  // Histograms -- N layers
  // -----------------------------------------------------------------------
  TH1F* hNLayers[NBOARDS];
  TH1F* hNLayers_total;
  for (int b = 0; b < NBOARDS; b++) {
    hNLayers[b] = new TH1F(Form("hNLayers_b%d",b),
			   Form("%s hit layers per event;N layers hit;Entries",BOARD_NAMES[b]),
			   N_LAYERS[b]+1,-0.5,N_LAYERS[b]+0.5);
  }
  hNLayers_total = new TH1F("hNLayers_total",
			    "Total hit layers per event;N layers hit;Entries",
			    N_LAYERS[0]+N_LAYERS[1]+1,-0.5,N_LAYERS[0]+N_LAYERS[1]+0.5);

  // -----------------------------------------------------------------------
  // Histograms -- Strip (in mm)
  // -----------------------------------------------------------------------
  const int   ST_BINS = CH_PER_LAYER[0];                        // 32
  const float ST_LO   = -0.5f * STRIP_PITCH;                    // -0.25 mm
  const float ST_HI   = (CH_PER_LAYER[0] - 0.5f) * STRIP_PITCH; //  3.75 mm

  TH1F* hNHitCh_strip[MAX_LAYERS];
  TH1F* hCentroid_strip[MAX_LAYERS];
  TH1F* hLead_ch       [MAX_LAYERS];
  TH1F* hSubLead_ch    [MAX_LAYERS];
  for (int l = 0; l < N_LAYERS[0]; l++) {
    hNHitCh_strip[l]   = new TH1F(Form("hNHitCh_strip_l%d",l),
				  Form("Strip %s: hit ch/event;N ch hit;Entries",LAYER_NAMES[0][l]),
				  CH_PER_LAYER[0]+1,-0.5,CH_PER_LAYER[0]+0.5);
    hCentroid_strip[l] = new TH1F(Form("hCentroid_strip_l%d",l),
				  Form("Strip %s: centroid;Position [mm];Entries",LAYER_NAMES[0][l]),
				  ST_BINS,ST_LO,ST_HI);
    hLead_ch[l]        = new TH1F(Form("hLead_strip_l%d",l),
				  Form("Strip %s: leading strip;Local ch;Entries",LAYER_NAMES[0][l]),
				  CH_PER_LAYER[0],-0.5,CH_PER_LAYER[0]-0.5);
    hSubLead_ch[l]     = new TH1F(Form("hSubLead_strip_l%d",l),
				  Form("Strip %s: sub-leading strip;Local ch;Entries",LAYER_NAMES[0][l]),
				  CH_PER_LAYER[0],-0.5,CH_PER_LAYER[0]-0.5);
  }

  // Strip Front vs Back correlation (raw and alignment-corrected)
  TH2F* hCorX_strip         = new TH2F("hCorX_strip",
				       "Strip X: Front vs Back (raw);X1 [mm];X2 [mm]",         ST_BINS,ST_LO,ST_HI,ST_BINS,ST_LO,ST_HI);
  TH2F* hCorY_strip         = new TH2F("hCorY_strip",
				       "Strip Y: Front vs Back (raw);Y1 [mm];Y2 [mm]",         ST_BINS,ST_LO,ST_HI,ST_BINS,ST_LO,ST_HI);
  TH2F* hCorX_strip_corr    = new TH2F("hCorX_strip_corr",
				       "Strip X: Front vs Back (corrected);X1 [mm];X2-dX [mm]",ST_BINS,ST_LO,ST_HI,ST_BINS,ST_LO,ST_HI);
  TH2F* hCorY_strip_corr    = new TH2F("hCorY_strip_corr",
				       "Strip Y: Front vs Back (corrected);Y1 [mm];Y2-dY [mm]",ST_BINS,ST_LO,ST_HI,ST_BINS,ST_LO,ST_HI);
  TH1F* hResid_strip_X      = new TH1F("hResid_strip_X",
				       "Strip X: Back-Front (raw);X2-X1 [mm];Entries",       100,-2.0,2.0);
  TH1F* hResid_strip_Y      = new TH1F("hResid_strip_Y",
				       "Strip Y: Back-Front (raw);Y2-Y1 [mm];Entries",       100,-2.0,2.0);
  TH1F* hResid_strip_X_corr = new TH1F("hResid_strip_X_corr",
				       "Strip X: Back-Front (corrected);X2-dX-X1 [mm];Entries",100,-2.0,2.0);
  TH1F* hResid_strip_Y_corr = new TH1F("hResid_strip_Y_corr",
				       "Strip Y: Back-Front (corrected);Y2-dY-Y1 [mm];Entries",100,-2.0,2.0);

  // -----------------------------------------------------------------------
  // Histograms -- Pixel (row/col units)
  // -----------------------------------------------------------------------
  const int   PX_BINS   = PIXEL_NCOL * 4;
  const float PX_COL_LO = -0.5f, PX_COL_HI = PIXEL_NCOL - 0.5f;
  const float PX_ROW_LO = -0.5f, PX_ROW_HI = PIXEL_NROW - 0.5f;

  TH1F* hNHitCh_pixel  [2];
  TH2F* hPixelOcc      [2];
  TH1F* hPixelCentRow  [2];
  TH1F* hPixelCentCol  [2];
  TH1F* hPixelSumToTs  [2];
  TH2F* hPixelCorFrontBack;
  for (int l = 0; l < N_LAYERS[1]; l++) {
    hNHitCh_pixel[l]   = new TH1F(Form("hNHitCh_pixel_l%d",l),
				  Form("Pixel %s: hit ch/event;N ch hit;Entries",LAYER_NAMES[1][l]),
				  CH_PER_LAYER[1]+1,-0.5,CH_PER_LAYER[1]+0.5);
    hPixelOcc[l]       = new TH2F(Form("hPixelOcc_l%d",l),
				  Form("Pixel %s: occupancy map;Col (X);Row (Y)",LAYER_NAMES[1][l]),
				  PIXEL_NCOL,PX_COL_LO,PX_COL_HI, PIXEL_NROW,PX_ROW_LO,PX_ROW_HI);
    hPixelCentRow[l]   = new TH1F(Form("hPixelCentRow_l%d",l),
				  Form("Pixel %s: row centroid;Row;Entries",LAYER_NAMES[1][l]),
				  PX_BINS,PX_ROW_LO,PX_ROW_HI);
    hPixelCentCol[l]   = new TH1F(Form("hPixelCentCol_l%d",l),
				  Form("Pixel %s: col centroid;Col;Entries",LAYER_NAMES[1][l]),
				  PX_BINS,PX_COL_LO,PX_COL_HI);
    hPixelSumToTs[l]   = new TH1F(Form("hPixelSumToTs_l%d",l),
				  Form("Pixel %s: Sum ToTs;ToT[a.u.];Entries",LAYER_NAMES[1][l]),
				  1600,0,8000);
  }
  hPixelCorFrontBack = new TH2F("hPixelCorFrontBack",
				"Pixel: Front col vs Back col centroid;Front col;Back col",
				PX_BINS,PX_COL_LO,PX_COL_HI, PX_BINS,PX_COL_LO,PX_COL_HI);

  // -----------------------------------------------------------------------
  // Histograms -- extrapolation vs Pixel (6-layer events)
  // Filled after alignment correction is computed (two-pass)
  // X_extrap [mm] vs pixel col centroid [pixel units]
  // Y_extrap [mm] vs pixel row centroid [pixel units]
  // -----------------------------------------------------------------------
  TH2F* hExtrapX_vs_Col[2];
  TH2F* hExtrapY_vs_Row[2];
  TH1F* hResidX_extrap [2];
  TH1F* hResidY_extrap [2];
  for (int l = 0; l < N_LAYERS[1]; l++) {
    hExtrapX_vs_Col[l] = new TH2F(Form("hExtrapX_vs_Col_l%d",l),
				  Form("Pixel %s: X_extrap vs col centroid;X_extrap [mm];Col centroid [pixel]",LAYER_NAMES[1][l]),
				  ST_BINS,ST_LO,ST_HI, PX_BINS,PX_COL_LO,PX_COL_HI);
    hExtrapY_vs_Row[l] = new TH2F(Form("hExtrapY_vs_Row_l%d",l),
				  Form("Pixel %s: Y_extrap vs row centroid;Y_extrap [mm];Row centroid [pixel]",LAYER_NAMES[1][l]),
				  ST_BINS,ST_LO,ST_HI, PX_BINS,PX_ROW_LO,PX_ROW_HI);
    hResidX_extrap[l]  = new TH1F(Form("hResidX_extrap_l%d",l),
				  Form("Pixel %s: col - X_extrap/pitch;#Delta col [pixel];Entries",LAYER_NAMES[1][l]),
				  100,-5.0,5.0);
    hResidY_extrap[l]  = new TH1F(Form("hResidY_extrap_l%d",l),
				  Form("Pixel %s: row - Y_extrap/pitch;#Delta row [pixel];Entries",LAYER_NAMES[1][l]),
				  100,-5.0,5.0);
  }

  // -----------------------------------------------------------------------
  // Event loop (pass 1): fill per-ch histos, store data for correction
  // -----------------------------------------------------------------------
  const Long64_t n_entries = tree->GetEntries();
  cout << "Processing " << n_entries << " events..." << endl;

  std::vector<StripFourLayerData> strip4_data;
  std::vector<SixLayerData>       six_data;
  strip4_data.reserve(n_entries);
  six_data.reserve(n_entries);

  // Extrapolation to front of pixel sensor
  auto extrap_fn = [](double c_front, double c_back,
                      float z_front, float z_back, float z_target) -> double {
    if (z_back == z_front) return c_front;
    return c_front + (c_back - c_front) * (z_target - z_front) / (z_back - z_front);
  };

  for (Long64_t iev = 0; iev < n_entries; iev++) {
    if (iev % 10000 == 0)
      cout << "  Event " << iev << " / " << n_entries << endl;
    tree->GetEntry(iev);

    // ---- Step 1: compute pulses for all channels ----
    std::vector<PulseShape> pulses_all[NBOARDS][N_CH];
    bool  ch_hit[NBOARDS][N_CH]; // Hit flag
    float ch_min[NBOARDS][N_CH]; // Store maximum ToT
    float ch_charge[NBOARDS][N_CH]; // Store maximum charge
    for (int b = 0; b < NBOARDS; b++) {
      for (int ch = 0; ch < N_CH; ch++) {
	// Init flags
	ch_hit[b][ch] = false;
        ch_min[b][ch] = 0.0f;
        ch_charge[b][ch] = 0.0f;
        if (DEAD_CH[b].count(ch))
	  continue;
	// Get waveform from samling
        pulses_all[b][ch] = AnalyzeWaveform(amp[b][ch], b, ch, SAMPLE_N, THRESHOLD, SAMPLING_INTERVAL, min_tot[b][(int)ch%4]);
        if (!pulses_all[b][ch].empty()) {
          ch_hit[b][ch] = true;
        }
        for (const auto& p : pulses_all[b][ch]) {
	  if (ch_min[b][ch] < p.tot)
	    ch_min[b][ch] = p.tot;
	  if (ch_charge[b][ch] < p.charge)
	    ch_charge[b][ch] = p.charge;


	  
          hMinAmp_ch  [b][ch]->Fill(p.min_adc);
          hPedestal_ch[b][ch]->Fill(p.pedestal);
          hTlead_ch   [b][ch]->Fill(p.t_lead);
          hToT_ch     [b][ch]->Fill(p.tot);
          hCharge_ch  [b][ch]->Fill(p.charge);
        }
      }
    }

    // ---- Step 2: Strip layer analysis ----
    int    n_layers_hit_strip = 0;
    double strip_centroid_mm[MAX_LAYERS];
    for (int l = 0; l < MAX_LAYERS; l++)
      strip_centroid_mm[l] = -1.0; // Initialize with dummy values

    for (int l = 0; l < N_LAYERS[0]; l++) {
      int    n_hit = 0;
      double wsum  = 0.0;
      double psum  = 0.0;
      float  max1  = 0.0f;
      float  max2  = 0.0f;
      int    ch1   = -1;
      int    ch2   = -1;
      int    lead  = -1;
      int    slead = -1;
      // Find leading and sub-leading strips
      for (int i = 0; i < CH_PER_LAYER[0]; i++) {
        int ch = LAYER_START[0][l] + i;
        if (!ch_hit[0][ch])
	  continue;
        n_hit++;
        double w = ch_min[0][ch];
        if (w > max1) { // Update leading strip 
	  max2 = max1;
	  ch2 = ch1;
	  max1 = (float)w;
	  ch1 = i;
	} else if (w > max2) {
	  max2 = (float)w;
	  ch2 = i;
	}	
      }
      
      hNHitCh_strip[l]->Fill(n_hit);
      if (n_hit > 0) {
        n_layers_hit_strip++;
        if (ch1 >= 0) hLead_ch[l]->Fill(ch1);
        if (ch2 >= 0) hSubLead_ch[l]->Fill(ch2);
      }

      
      if (n_hit == 1) {
	// Only leading strip has hit
	strip_centroid_mm[l] = ch1 * STRIP_PITCH;
        hCentroid_strip[l]->Fill(strip_centroid_mm[l]);
      } else if (n_hit>2) {
	// Get centroid from leading and subleading
	strip_centroid_mm[l] = (ch1*max1+ch2*max2)/(max1+max2) * STRIP_PITCH;
        hCentroid_strip[l]->Fill(strip_centroid_mm[l]);
      } 
    }

    hNLayers[0]->Fill(n_layers_hit_strip);

    // Store strip 4-layer data for alignment correction
    if (n_layers_hit_strip == N_LAYERS[0]) {
      StripFourLayerData sd;
      sd.x1_mm = strip_centroid_mm[0]; sd.y1_mm = strip_centroid_mm[1];
      sd.x2_mm = strip_centroid_mm[2]; sd.y2_mm = strip_centroid_mm[3];
      strip4_data.push_back(sd);
      hCorX_strip->Fill(sd.x1_mm, sd.x2_mm);
      hCorY_strip->Fill(sd.y1_mm, sd.y2_mm);
      hResid_strip_X->Fill(sd.x2_mm - sd.x1_mm);
      hResid_strip_Y->Fill(sd.y2_mm - sd.y1_mm);
    }

    // ---- Step 3: Pixel layer analysis ----
    int    n_layers_hit_pixel = 0;
    double pix_centroid_row[2] = {-1.0, -1.0};
    double pix_centroid_col[2] = {-1.0, -1.0};
  
    for (int l = 0; l < N_LAYERS[1]; l++) {
      int    n_hit = 0;
      double wsum  = 0.0, rsum = 0.0, csum = 0.0;

      for (int i = 0; i < CH_PER_LAYER[1]; i++) {
        int ch = LAYER_START[1][l] + i;
        if (!ch_hit[1][ch]) continue;
        n_hit++;
        double w = -ch_min[1][ch];
        wsum += w;
        rsum += mapping_row[i] * w;
        csum += mapping_col[i] * w;
        hPixelOcc[l]->Fill(mapping_col[i], mapping_row[i]);
      }

      hNHitCh_pixel[l]->Fill(n_hit);
      if (n_hit > 0) {
        n_layers_hit_pixel++;
        pix_centroid_row[l] = rsum / wsum;
        pix_centroid_col[l] = csum / wsum;
        hPixelCentRow[l]->Fill(pix_centroid_row[l]);
        hPixelCentCol[l]->Fill(pix_centroid_col[l]);
      }

      // --- sumToT: threshold-passing channels only ---
      // Sum over all hit channels in this layer whose ToT exceeds min_tot[1][l].
      double sumToT = 0.0;
      for (int i = 0; i < CH_PER_LAYER[1]; i++) {
        int ch = LAYER_START[1][l] + i;
        if (!ch_hit[1][ch]) continue;
        // ch_min stores max ToT for each channel (set in Step 1)
        if (ch_min[1][ch] > min_tot[1][l]) {
          //sumToT += ch_min[1][ch];
	  sumToT += ch_charge[1][ch];
	}
      }
      if (sumToT > 0.0)
        hPixelSumToTs[l]->Fill(sumToT);
    }

    hNLayers[1]->Fill(n_layers_hit_pixel);
    hNLayers_total->Fill(n_layers_hit_strip + n_layers_hit_pixel);

    if (n_layers_hit_pixel == N_LAYERS[1])
      hPixelCorFrontBack->Fill(pix_centroid_col[0], pix_centroid_col[1]);

    // ---- Step 4: store 6-layer events for second pass ----
    if (n_layers_hit_strip != N_LAYERS[0]) continue;
    if (n_layers_hit_pixel != N_LAYERS[1]) continue;

    SixLayerData sx;
    sx.x1_mm = strip_centroid_mm[0]; sx.y1_mm = strip_centroid_mm[1];
    sx.x2_mm = strip_centroid_mm[2]; sx.y2_mm = strip_centroid_mm[3];
    for (int l = 0; l < 2; l++) {
      sx.pix_row[l] = pix_centroid_row[l];
      sx.pix_col[l] = pix_centroid_col[l];
    }
    six_data.push_back(sx);

  } // end event loop (pass 1)

  // -----------------------------------------------------------------------
  // Compute strip alignment offset from 4-layer strip events
  // offset = mean(Back - Front) for X and Y
  // -----------------------------------------------------------------------
  double offset_X = 0.0, offset_Y = 0.0;
  if (!strip4_data.empty()) {
    double sum_dX = 0.0, sum_dY = 0.0;
    for (const auto& sd : strip4_data) {
      sum_dX += sd.x2_mm - sd.x1_mm;
      sum_dY += sd.y2_mm - sd.y1_mm;
    }
    offset_X = sum_dX / strip4_data.size();
    offset_Y = sum_dY / strip4_data.size();
  }
  cout << Form("Strip alignment offset: dX = %.3f mm, dY = %.3f mm", offset_X, offset_Y) << endl;

  // Fill corrected strip correlation histograms
  for (const auto& sd : strip4_data) {
    double x2c = sd.x2_mm - offset_X;
    double y2c = sd.y2_mm - offset_Y;
    hCorX_strip_corr->Fill(sd.x1_mm, x2c);
    hCorY_strip_corr->Fill(sd.y1_mm, y2c);
    hResid_strip_X_corr->Fill(x2c - sd.x1_mm);
    hResid_strip_Y_corr->Fill(y2c - sd.y1_mm);
  }

  // -----------------------------------------------------------------------
  // Pass 2: fill extrap vs pixel histograms using corrected strip positions
  // -----------------------------------------------------------------------
  cout << "6-layer events: " << six_data.size() << " / " << n_entries << endl;

  for (const auto& sx : six_data) {
    double x2c = sx.x2_mm - offset_X;
    double y2c = sx.y2_mm - offset_Y;

    for (int l = 0; l < N_LAYERS[1]; l++) {
      float  zt    = (l == 0) ? Z_PIXEL_FRONT : Z_PIXEL_BACK;
      double Xex   = extrap_fn(sx.x1_mm, x2c,    Z_STRIP_FRONT, Z_STRIP_BACK, zt);
      double Yex   = extrap_fn(sx.y1_mm, y2c,    Z_STRIP_FRONT, Z_STRIP_BACK, zt);
      hExtrapX_vs_Col[l]->Fill(Xex, sx.pix_col[l]);
      hExtrapY_vs_Row[l]->Fill(Yex, sx.pix_row[l]);
      // Residual: pixel centroid [pixel] - extrap [mm] / pitch [mm/pixel]
      hResidX_extrap[l]->Fill(sx.pix_col[l] - Xex / PIXEL_PITCH);
      hResidY_extrap[l]->Fill(sx.pix_row[l] - Yex / PIXEL_PITCH);
    }
  }

  // -----------------------------------------------------------------------
  // Draw helpers
  // -----------------------------------------------------------------------
  auto DrawSingle = [&](TH1* h, const char* cname, const char* outpng, bool logy=false) {
    TCanvas* c = new TCanvas(cname, h->GetTitle(), 800*reso_factor, 600*reso_factor);
    if (logy) gPad->SetLogy();
    h->SetLineColor(kRed+1);
    if (auto h1 = dynamic_cast<TH1F*>(h))
      h1->SetFillColorAlpha(kRed,0.3);
    h->Draw(); c->SaveAs(outpng); 
    delete c;
  };

  auto DrawPerCh = [&](TH1F** harr, const char* cname, const char* outpng, bool logy=false) {
    TCanvas* c = new TCanvas(cname, cname, 1600*reso_factor, 1200*reso_factor);
    c->Divide(8, 4);
    for (int ch = 0; ch < N_CH; ch++) {
      c->cd(ch+1);
      if (logy) gPad->SetLogy();
      harr[ch]->SetLineColor(kBlue+1);
      harr[ch]->Draw();
    }
    c->SaveAs(outpng); 
    delete c;
  };

  auto DrawPerLayer = [&](TH1F** harr, int nlayers, const char* cname,
                          const char* outpng, bool logy=false) {
    TCanvas* c = new TCanvas(cname, cname, 1200*reso_factor, 400*reso_factor);
    c->Divide(nlayers, 1);
    for (int l = 0; l < nlayers; l++) {
      c->cd(l+1);
      if (logy) gPad->SetLogy();
      harr[l]->SetLineColor(kGreen+2);
      harr[l]->SetFillColorAlpha(kGreen+1, 0.3);
      harr[l]->Draw();
    }
    c->SaveAs(outpng); 
    delete c;
  };

  auto DrawCorr2D = [&](TH2F* h, const char* cname, const char* outpng) {
    TCanvas* c = new TCanvas(cname, h->GetTitle(), 800*reso_factor, 700*reso_factor);
    gPad->SetRightMargin(0.15);
    h->SetStats(0);
    h->Draw("COLZ");
    c->SaveAs(outpng); 
    delete c;
  };

  auto DrawSideBySide2D = [&](TH2F* h1, TH2F* h2, const char* cname, const char* outpng) {
    TCanvas* c = new TCanvas(cname, cname, 1600*reso_factor, 700*reso_factor);
    c->Divide(2,1);
    c->cd(1); gPad->SetRightMargin(0.15); h1->SetStats(0); h1->Draw("COLZ");
    c->cd(2); gPad->SetRightMargin(0.15); h2->SetStats(0); h2->Draw("COLZ");
    c->SaveAs(outpng); 
    delete c;
  };

  // -----------------------------------------------------------------------
  // Save plots
  // -----------------------------------------------------------------------
  for (int b = 0; b < NBOARDS; b++) {
    const char* bn = BOARD_NAMES[b];
    DrawPerCh(hMinAmp_ch  [b], Form("c_minamp_b%d",  b), Form("%s_minamp_per_ch_run%s.png",  bn, run_number), true);
    DrawPerCh(hPedestal_ch[b], Form("c_pedestal_b%d",b), Form("%s_pedestal_per_ch_run%s.png",bn, run_number));
    DrawPerCh(hTlead_ch   [b], Form("c_tlead_b%d",   b), Form("%s_tlead_per_ch_run%s.png",   bn, run_number));
    DrawPerCh(hToT_ch     [b], Form("c_tot_b%d",     b), Form("%s_tot_per_ch_run%s.png",     bn, run_number));
    DrawPerCh(hCharge_ch  [b], Form("c_charge_b%d",  b), Form("%s_charge_per_ch_run%s.png",  bn, run_number));
    DrawSingle(hNLayers[b],    Form("c_nlayers_b%d",b),   Form("%s_n_layers_hit_run%s.png",  bn, run_number));
  }
  DrawSingle(hNLayers_total, "c_nlayers_total", Form("n_layers_hit_total_run%s.png", run_number));

  // Strip
  DrawPerLayer(hNHitCh_strip,    N_LAYERS[0], "c_nhitCh_strip",   Form("Strip_nhit_ch_per_layer_run%s.png", run_number));
  DrawPerLayer(hCentroid_strip,  N_LAYERS[0], "c_centroid_strip",  Form("Strip_centroid_per_layer_run%s.png", run_number));
  DrawPerLayer(hLead_ch,         N_LAYERS[0], "c_lead_strip",      Form("Strip_leading_strip_run%s.png", run_number));
  DrawPerLayer(hSubLead_ch,      N_LAYERS[0], "c_sublead_strip",   Form("Strip_subleading_strip_run%s.png", run_number));

  DrawSideBySide2D(hCorX_strip, hCorY_strip,           "c_cor_strip_raw",  Form("Strip_correlation_raw_run%s.png", run_number));
  DrawSideBySide2D(hCorX_strip_corr, hCorY_strip_corr, "c_cor_strip_corr", Form("Strip_correlation_corrected_run%s.png", run_number));

  {
    TCanvas* cRs = new TCanvas("c_resid_strip","Strip residuals",1600*reso_factor,600*reso_factor);
    cRs->Divide(4,1);
    auto dr = [&](int p, TH1F* h) {
      cRs->cd(p);
      h->SetLineColor(kBlue+1);
      h->Draw();
    };
    dr(1,hResid_strip_X);
    dr(2,hResid_strip_Y);
    dr(3,hResid_strip_X_corr);
    dr(4,hResid_strip_Y_corr);
    cRs->SaveAs(Form("Strip_residuals_front_back_run%s.png", run_number)); 
    delete cRs;
  }

  // Pixel occupancy
  for (int l = 0; l < N_LAYERS[1]; l++) {
    DrawCorr2D(hPixelOcc[l],     Form("c_pixocc_l%d",l), Form("Pixel_%s_occupancy_run%s.png",LAYER_NAMES[1][l], run_number));
    DrawSingle(hPixelCentRow[l], Form("c_pixrow_l%d",l), Form("Pixel_%s_centroid_row_run%s.png",LAYER_NAMES[1][l], run_number));
    DrawSingle(hPixelCentCol[l], Form("c_pixcol_l%d",l), Form("Pixel_%s_centroid_col_run%s.png",LAYER_NAMES[1][l], run_number));
  }
  DrawCorr2D(hPixelCorFrontBack, "c_pix_fb", Form("Pixel_Front_vs_Back_col_run%s.png", run_number));

  // Extrap vs Pixel (6-layer events, corrected strip positions)
  DrawSideBySide2D(hExtrapX_vs_Col[0], hExtrapX_vs_Col[1], "c_extrap_x_vs_col", Form("extrap_X_vs_pixel_col_run%s.png", run_number));
  DrawSideBySide2D(hExtrapY_vs_Row[0], hExtrapY_vs_Row[1], "c_extrap_y_vs_row", Form("extrap_Y_vs_pixel_row_run%s.png", run_number));

  {
    TCanvas* cRp = new TCanvas("c_resid_extrap","Extrap residuals",1600*reso_factor,800*reso_factor);
    cRp->Divide(2,2);
    auto dr = [&](int p, TH1F* h){ cRp->cd(p); h->SetLineColor(kMagenta+1); h->Draw(); };
    dr(1,hResidX_extrap[0]); dr(2,hResidY_extrap[0]);
    dr(3,hResidX_extrap[1]); dr(4,hResidY_extrap[1]);
    cRp->SaveAs(Form("residuals_extrap_vs_pixel_run%s.png", run_number));
    delete cRp;
  }

  // -----------------------------------------------------------------------
  // Draw sumToT histograms (PNG)
  // -----------------------------------------------------------------------
  DrawPerLayer(hPixelSumToTs, N_LAYERS[1], "c_sumtot_pixel", "Pixel_sumToT_per_layer_run%s.png");

  // -----------------------------------------------------------------------
  // Save hPixelSumToTs to ROOT file:  out_run<run_number>.root
  // -----------------------------------------------------------------------
  /*  TString outname = Form("out_run%s.root", run_number);
  TFile* fout = TFile::Open(outname, "RECREATE");
  if (fout && !fout->IsZombie()) {
    for (int l = 0; l < N_LAYERS[1]; l++) {
      hPixelSumToTs[l]->SetName(Form("hPixelSumToTs_run%s_l%d", run_number, l));
      hPixelSumToTs[l]->Write();
    }
    fout->Close();
    cout << "SumToT histograms saved to " << outname << endl;
  } else {
    cerr << "Cannot create output file: " << outname << endl;
  }
  */
  TString outname = Form("out_run%s.root", run_number);
  TFile* fout = TFile::Open(outname, "RECREATE");
  TIter next(gDirectory->GetList());
  TObject* obj;

  while ((obj = next())) {
    obj->Write();
  }

  fout->Close();

  cout << "Done. Plots saved." << endl;
  f->Close();
}
