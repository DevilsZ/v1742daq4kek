// Usage: root -l 'Analyze.C("data.root")'

void Analyze(const char* filename) {

  // -----------------------------------------------------------------------
  // Configuration
  // -----------------------------------------------------------------------
  const int   BOARD      = 0;       // which board to analyze
  const int   N_CH       = 32;
  const int   SAMPLE_N   = 1024;
  const float THRESHOLD  = -100.0f;  // hit threshold (negative pulse)

  // Layer definitions: 4 layers x 8 channels each
  const int   N_LAYERS   = 4;
  const char* LAYER_NAMES[N_LAYERS] = {"Front X", "Front Y", "Back X", "Back Y"};
  const int   LAYER_START[N_LAYERS] = {0, 8, 16, 24};
  const int   CH_PER_LAYER          = 8;

  // -----------------------------------------------------------------------
  // Open file / tree
  // -----------------------------------------------------------------------
  TFile* f = TFile::Open(filename);
  if (!f || f->IsZombie()) {
    cerr << "Cannot open: " << filename << endl;
    return;
  }
  TTree* tree = (TTree*)f->Get("tree");
  if (!tree) {
    cerr << "TTree 'tree' not found." << endl;
    return;
  }

  Long64_t ev_id;
  tree->SetBranchAddress("ev_id", &ev_id);

  Float_t amp[N_CH][SAMPLE_N];
  for (int ch = 0; ch < N_CH; ch++) {
    tree->SetBranchAddress(Form("amp_b%d_ch%02d", BOARD, ch), amp[ch]);
  }

  // -----------------------------------------------------------------------
  // Histograms
  // -----------------------------------------------------------------------

  // --- Min amplitude distribution (all hit channels combined) ---
  TH1F* hMinAmp = new TH1F(
    "hMinAmp",
    "Min amplitude (hit channels, all);Min ADC [counts];Entries",
    200, -500., 0.
  );

  // --- Min amplitude per channel ---
  TH1F* hMinAmp_ch[N_CH];
  for (int ch = 0; ch < N_CH; ch++) {
    hMinAmp_ch[ch] = new TH1F(
      Form("hMinAmp_ch%02d", ch),
      Form("Ch%02d min amplitude;Min ADC [counts];Entries", ch),
      200, -500., 0.
    );
  }

  // --- Number of hit layers per event ---
  TH1F* hNLayers = new TH1F(
    "hNLayers",
    "Hit layers per event;N layers hit;Entries",
    N_LAYERS + 1, -0.5, N_LAYERS + 0.5
  );

  // --- Number of hit channels per layer ---
  TH1F* hNHitCh[N_LAYERS];
  for (int l = 0; l < N_LAYERS; l++) {
    hNHitCh[l] = new TH1F(
      Form("hNHitCh_l%d", l),
      Form("%s: hit channels per event;N channels hit;Entries", LAYER_NAMES[l]),
      CH_PER_LAYER + 1, -0.5, CH_PER_LAYER + 0.5
    );
  }

  // --- Weighted centroid per layer (in local channel index 0..7) ---
  TH1F* hCentroid[N_LAYERS];
  for (int l = 0; l < N_LAYERS; l++) {
    hCentroid[l] = new TH1F(
      Form("hCentroid_l%d", l),
      Form("%s: weighted centroid;Local channel;Entries", LAYER_NAMES[l]),
      80, -0.5, 7.5
    );
  }
  
  // --- Centroid correlation (4-layer events only) ---
  TH2F* hCorX = new TH2F("hCorX",
    "X centroid: Front vs Back (4-layer events);Front X centroid [ch];Back X centroid [ch]",
    8, -0.5, 7.5, 8, -0.5, 7.5);

  TH2F* hCorY = new TH2F("hCorY",
    "Y centroid: Front vs Back (4-layer events);Front Y centroid [ch];Back Y centroid [ch]",
    8, -0.5, 7.5, 8, -0.5, 7.5);


  // -----------------------------------------------------------------------
  // Event loop
  // -----------------------------------------------------------------------
  const Long64_t n_entries = tree->GetEntries();
  cout << "Processing " << n_entries << " events..." << endl;

  for (Long64_t iev = 0; iev < n_entries; iev++) {

    if (iev % 10000 == 0)
      cout << "  Event " << iev << " / " << n_entries << endl;

    tree->GetEntry(iev);

    // --- Find minimum amplitude for each channel ---
    float ch_min[N_CH];
    bool  ch_hit[N_CH];

    for (int ch = 0; ch < N_CH; ch++) {
      float minval = 0.0f;
      for (int s = 0; s < SAMPLE_N; s++) {
        if (amp[ch][s] < minval) minval = amp[ch][s];
      }
      ch_min[ch] = minval;
      ch_hit[ch] = (minval < THRESHOLD);

      if (ch_hit[ch]) {
        hMinAmp->Fill(minval);
        hMinAmp_ch[ch]->Fill(minval);
      }
    }

    // --- Layer analysis ---
    int n_layers_hit = 0;
    double centroid[N_LAYERS]  = {-1., -1., -1., -1.}; // -1 = no hit

    for (int l = 0; l < N_LAYERS; l++) {
      int    n_hit       = 0;
      double weight_sum  = 0.0;
      double pos_sum     = 0.0;

      for (int i = 0; i < CH_PER_LAYER; i++) {
        int ch = LAYER_START[l] + i;
        if (!ch_hit[ch]) continue;

        n_hit++;
        double w = -ch_min[ch]; // weight = |amplitude| (positive)
        weight_sum += w;
        pos_sum    += i * w;
      }

      hNHitCh[l]->Fill(n_hit);

      if (n_hit > 0) {
        n_layers_hit++;
        centroid[l] = pos_sum / weight_sum;
        hCentroid[l]->Fill(centroid[l]);
      }
    }

    hNLayers->Fill(n_layers_hit);

    // --- Fill correlation only when all 4 layers are hit ---
    if (n_layers_hit == 4) {
      hCorX->Fill(centroid[0], centroid[2]); // layer 0: Front X, layer 2: Back X
      hCorY->Fill(centroid[1], centroid[3]); // layer 1: Front Y, layer 3: Back Y
    }


  } // end event loop

  // -----------------------------------------------------------------------
  // Draw and save
  // -----------------------------------------------------------------------

  // --- Canvas 1: Min amplitude (all + per channel) ---
  TCanvas* c1 = new TCanvas("c1", "Min Amplitude", 1600, 1200);
  c1->Divide(8, 4);
  for (int ch = 0; ch < N_CH; ch++) {
    c1->cd(ch + 1);
    gPad->SetLogy();
    hMinAmp_ch[ch]->SetLineColor(kBlue + 1);
    hMinAmp_ch[ch]->Draw();
  }
  c1->SaveAs("minamp_per_ch.png");

  TCanvas* c1b = new TCanvas("c1b", "Min Amplitude All", 800, 600);
  hMinAmp->SetLineColor(kBlue + 1);
  hMinAmp->Draw();
  c1b->SaveAs("minamp_all.png");

  // --- Canvas 2: Number of hit layers ---
  TCanvas* c2 = new TCanvas("c2", "Hit layers", 800, 600);
  hNLayers->SetLineColor(kRed + 1);
  hNLayers->SetFillColorAlpha(kRed, 0.3);
  hNLayers->Draw();
  c2->SaveAs("n_layers_hit.png");

  // --- Canvas 3: Hit channels per layer ---
  TCanvas* c3 = new TCanvas("c3", "Hit channels per layer", 1200, 400);
  c3->Divide(4, 1);
  for (int l = 0; l < N_LAYERS; l++) {
    c3->cd(l + 1);
    hNHitCh[l]->SetLineColor(kGreen + 2);
    hNHitCh[l]->SetFillColorAlpha(kGreen + 1, 0.3);
    hNHitCh[l]->Draw();
  }
  c3->SaveAs("nhit_ch_per_layer.png");

  // --- Canvas 4: Weighted centroid per layer ---
  TCanvas* c4 = new TCanvas("c4", "Weighted centroid", 1200, 400);
  c4->Divide(4, 1);
  for (int l = 0; l < N_LAYERS; l++) {
    c4->cd(l + 1);
    hCentroid[l]->SetLineColor(kMagenta + 1);
    hCentroid[l]->SetFillColorAlpha(kMagenta, 0.3);
    hCentroid[l]->Draw();
  }
  c4->SaveAs("centroid_per_layer.png");

  // --- Canvas 5: Centroid correlations (4-layer events) ---
  TCanvas* c5 = new TCanvas("c5", "Centroid correlation (4-layer events)", 1200, 600);
  c5->Divide(2, 1);

  c5->cd(1);
  gPad->SetRightMargin(0.15);
  hCorX->SetStats(0);
  hCorX->Draw("COLZ");

  c5->cd(2);
  gPad->SetRightMargin(0.15);
  hCorY->SetStats(0);
  hCorY->Draw("COLZ");

  c5->SaveAs("centroid_correlation.png");

  
  cout << "Done. Plots saved." << endl;
  f->Close();
}
