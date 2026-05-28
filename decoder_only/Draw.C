// Usage: root -l 'Draw.C("data.root")'

void Draw(const char* filename) {

  const int n_events    = 10;
  const int n_boards    = 1;
  const int n_ch        = 32;
  const int sample_n    = 1024;

  TFile* f = TFile::Open(filename);
  if (!f || f->IsZombie()) {
    cerr << "Cannot open file: " << filename << endl;
    return;
  }

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

  for (int iev = 0; iev < n_draw; iev++) {
    tree->GetEntry(iev);

    for (int b = 0; b < n_boards; b++) {

      TCanvas* c = new TCanvas(
        Form("c_ev%d_b%d", iev, b),
        Form("Event %lld  Board %d", ev_id, b),
        1600, 1200
      );
      c->Divide(8, 4);

      for (int ch = 0; ch < n_ch; ch++) {
        c->cd(ch + 1);
        gPad->SetMargin(0.12, 0.05, 0.12, 0.08);

        double y[sample_n];
        for (int s = 0; s < sample_n; s++)
	  y[s] = amp[b][ch][s];

        TGraph* gr = new TGraph(sample_n, x, y);
        gr->SetTitle(Form("b%d ch%02d ev%lld;Sample;ADC", b, ch, ev_id));
        gr->SetLineColor(kBlue + 1);
        gr->SetLineWidth(1);
        gr->Draw("AL");
	gr->GetYaxis()->SetRangeUser(-900., 100.);
	gr->GetXaxis()->SetRangeUser(0, 500);
      }

      c->SaveAs(Form("waveform_ev%03d_b%d.png", iev, b));
    }
  }

  cout << "Done. " << n_draw << " events plotted." << endl;
  f->Close();
}
