// Usage: root -l 'Draw.C("data.root")'

void Draw(const char* filename) {

  const int n_events    = 10000;
  const int n_boards    = 2;
  const int n_ch        = 32;
  const int sample_n    = 1024;

  TFile* f = TFile::Open(filename);
  if (!f || f->IsZombie()) {
    cerr << "Cannot open file: " << filename << endl;
    return;
  }

  // Get run number
  char run_number[32];
  sscanf(filename, "%[^.].root", run_number);
  
  TTree* tree = (TTree*)f->Get("tree");
  if (!tree) {
    cerr << "TTree 'tree' not found." << endl;
    return;
  }

  // Set Branch
  Long64_t ev_id;
  tree->SetBranchAddress("ev_id", &ev_id);

  // amp[board][ch][sample]
  Float_t amp[n_boards][n_ch][sample_n];
  for (int b = 0; b < n_boards; b++) {
    for (int ch = 0; ch < n_ch; ch++) {
      string bname = Form("amp_b%d_ch%02d", b, ch);
      tree->SetBranchAddress(bname.c_str(), amp[b][ch]);
    }
  }

  // 
  double x[sample_n];
  for (int s = 0; s < sample_n; s++) x[s] = s;

  // 
  int n_draw = min((Long64_t)n_events, tree->GetEntries());

  TCanvas* c[2];

  for (int b = 0; b < n_boards; b++) {
    c[b] = new TCanvas(Form("c_b%d", b),
		       Form("Board %d", b),
		       1600, 1200);
    c[b]->Divide(8, 4);
  }

  for (int iev = 0; iev < n_draw; iev++) {
    tree->GetEntry(iev);

    for (int b = 0; b < n_boards; b++) {
      for (int ch = 0; ch < n_ch; ch++) {
        c[b]->cd(ch + 1);
        gPad->SetMargin(0.12, 0.05, 0.12, 0.08);

        double y[sample_n];
        for (int s = 0; s < sample_n; s++)
	  y[s] = amp[b][ch][s];

        TGraph* gr = new TGraph(sample_n, x, y);
        gr->SetTitle(Form("b%d ch%02d ev%lld;Sample;ADC", b, ch, ev_id));
        gr->SetLineColor(kBlue + 1);
        gr->SetLineWidth(1);
	if (iev==0)
	  gr->Draw("AL");
	else
	  gr->Draw("Lsame");
	gr->GetYaxis()->SetRangeUser(-200., 100.);
	gr->GetXaxis()->SetRangeUser(0, 1024);
      }
    }
  }

  for (int b = 0; b < n_boards; b++) {
    c[b]->SaveAs(Form("waveform_%s_b%d.png", run_number, b));
  }

  cout << "Done. " << n_draw << " events plotted." << endl;
  f->Close();
}
