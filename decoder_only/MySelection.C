#define MySelection_cxx
// The class definition in MySelection.h has been generated automatically
// by the ROOT utility TTree::MakeSelector(). This class is derived
// from the ROOT class TSelector. For more information on the TSelector
// framework see $ROOTSYS/README/README.SELECTOR or the ROOT User Manual.


// The following methods are defined in this file:
//    Begin():        called every time a loop on the tree starts,
//                    a convenient place to create your histograms.
//    SlaveBegin():   called after Begin(), when on PROOF called only on the
//                    slave servers.
//    Process():      called for each event, in this function you decide what
//                    to read and fill your histograms.
//    SlaveTerminate: called at the end of the loop on the tree, when on PROOF
//                    called only on the slave servers.
//    Terminate():    called at the end of the loop on the tree,
//                    a convenient place to draw/fit your histograms.
//
// To use this file, try the following session on your Tree T:
//
// root> T->Process("MySelection.C")
// root> T->Process("MySelection.C","some options")
// root> T->Process("MySelection.C+")
#include "MySelection.h"
#include <TH2.h>
#include <TStyle.h>

void MySelection::Begin(TTree * /*tree*/)
{
  // The Begin() function is called at the start of the query.
  // When running with PROOF Begin() is only called on the client.
  // The tree argument is deprecated (on PROOF 0 is passed).
  TString option = GetOption();

  //  if (fInput && fInput->GetEntries() > 0) {
  //input_filename = ((TObjString*)fInput->At(0))->GetString();
  input_filename = fOption;
  cout << "input_filename: " << input_filename << endl;
  run_number = input_filename(input_filename.Index("waveform_run") + 12, 5);
  cout << "run_number: " << run_number << endl;
  
  //int run = 0;
  //sscanf(input_filename.Data(), "waveform_run%d.root", &run);
  //run_number = Form("%05d",run);
}

void MySelection::SlaveBegin(TTree * /*tree*/)
{
   // The SlaveBegin() function is called after the Begin() function.
   // When running with PROOF SlaveBegin() is called on each slave server.
   // The tree argument is deprecated (on PROOF 0 is passed).

   TString option = GetOption();

   for (int L = 0; L < kMaxLayers; ++L) {
     f_thr_corr[L] = new TF1(Form("f_thr_corr_L%d", L), "[0] + [1]*exp(-x/[2])",
			     0.0, 4000.);
     f_thr_corr[L]->SetParameters(p0[L], p1[L], p2[L]);
   }
   
   h_strip_x_cor = new TH1F("h_strip_x_cor",
			    "h_strip_x_cor;front x - rear x",
			    80,-4,4);
   h_strip_y_cor = new TH1F("h_strip_y_cor",
			    "h_strip_y_cor;front y - rear y",
			    80,-4,4);
   h2_strip_x_cor = new TH2F("h2_strip_x_cor",
			     "h2_strip_x_cor;front x; rear x",
			     8,0,4,8,0,4);
   h2_strip_y_cor = new TH2F("h2_strip_y_cor",
			     "h2_strip_y_cor;front y; rear y",
			     8,0,4,8,0,4);

   for (int L = 0; L < kMaxLayers; L++) {
     h_sum_charge[L] = new TH1F(Form("h_sum_charge_%s", kLayerNames[L]),
				Form("h_sum_charge_%s", kLayerNames[L]),
				100, 0, 20000);
     h_max_charge[L] = new TH1F(Form("h_max_charge_%s", kLayerNames[L]),
				Form("h_max_charge_%s", kLayerNames[L]),
				100, 0, 4000);
     h2_Tlead[L]     = new TH2F(Form("h2_Tlead_%s", kLayerNames[L]),
				Form("T_lead vs ch %s;Channel;\{t_lead - t_average};", kLayerNames[L]),
				kNCh[L], 0, kNCh[L], 100, -50, 50);
     GetOutputList()->Add(h_sum_charge[L]);
     GetOutputList()->Add(h_max_charge[L]);
     GetOutputList()->Add(h2_Tlead[L]);
     for (int ch = 0; ch < kNCh[L]; ch++) {
       h2_ToT_Charge[L][ch] = new TH2F(Form("h2_ToT_Charge_%s_ch%02d", kLayerNames[L], ch),
				       Form("ToT vs Charge %s ch%02d;ToT;Charge", kLayerNames[L], ch),
				       100, 0, 20, 100, 0, 8000);
       h2_ToT_Amp[L][ch]    = new TH2F(Form("h2_ToT_Amp_%s_ch%02d", kLayerNames[L], ch),
				       Form("ToT vs Amp %s ch%02d;ToT;Min_amp", kLayerNames[L], ch),
				       100, 0, 20, 1000, 0, 1000);
       h2_Amp_Charge[L][ch] = new TH2F(Form("h2_Amp_Charge_%s_ch%02d", kLayerNames[L], ch),
				       Form("Amp vs Charge %s ch%02d;Min_amp;Charge", kLayerNames[L], ch),
				       1000, 0, 1000, 100, 0, 8000);
       h2_Tlead_ToT[L][ch]  = new TH2F(Form("h2_Tlead_ToT_%s_ch%02d", kLayerNames[L], ch),
				       Form("Tlead vs ToT %s ch%02d;t_lead;ToT", kLayerNames[L], ch),
				       130, -300, 1000, 100, 0, 20);
       h2_Tlead_Amp[L][ch]  = new TH2F(Form("h2_Tlead_Amp_%s_ch%02d", kLayerNames[L], ch),
				       Form("Tlead vs Amp %s ch%02d;Amp;t_lead - t_average", kLayerNames[L], ch),
				       100, 0, 500, 100, -10, 10);
       h2_Tlead_T0[L][ch]   = new TH2F(Form("h2_Tlead_T0_%s_ch%02d", kLayerNames[L], ch),
				       Form("Tlead vs T0 %s ch%02d;t_lead;t_0", kLayerNames[L], ch),
				       240,    0, 240, 240, 0, 240);
       h_Tlead[L][ch]       = new TH1F(Form("h_Tlead_%s_ch%02d", kLayerNames[L], ch),
				       Form("T_lead %s ch%02d;t_lead - t_0;", kLayerNames[L], ch),
				       1300, -300, 1000);
       //
       GetOutputList()->Add(h2_ToT_Charge[L][ch]);
       GetOutputList()->Add(h2_ToT_Amp[L][ch]);
       GetOutputList()->Add(h2_Amp_Charge[L][ch]);
       GetOutputList()->Add(h2_Tlead_ToT[L][ch]);
       GetOutputList()->Add(h2_Tlead_Amp[L][ch]);
       GetOutputList()->Add(h2_Tlead_T0[L][ch]);
       GetOutputList()->Add(h_Tlead[L][ch]);
     } 
   }
}

bool MySelection::Process(Long64_t entry)
{
   // The Process() function is called for each entry in the tree (or possibly
   // keyed object in the case of PROOF) to be processed. The entry argument
   // specifies which entry in the currently loaded tree is to be processed.
   // When processing keyed objects with PROOF, the object is already loaded
   // and is available via the fObject pointer.
   //
   // This function should contain the \"body\" of the analysis. It can contain
   // simple or elaborate selection criteria, run algorithms on the data
   // of the event and typically fill histograms.
   //
   // The processing can be stopped by calling Abort().
   //
   // Use fStatus to set the return value of TTree::Process().
   //
   // The return value is currently not used.
   fReader.SetLocalEntry(entry);

   // Hit encoder
   // assuming;
   // 1111** all strip have hit, 111111 hits in all layers
   std::string hit_encoder = "000000";
   /* hit decision to be changed
   for (int L = 0; L < kMaxLayers; L++) {
     for (int ch = 0; ch < kNCh[L]; ch++) {
       
       int n = **nArr[L][ch];
       
       if (n > 0)
	 hit_encoder[L] = '1';
     }
   }
   */

   //   if (hit_encoder.starts_with("1111"))
   if (hit_encoder.rfind("1111", 0) == 0) {
     // TODO: Strip tracking for future
     // cout << "Strip Tracking..." << endl;
   }
   
   //
   float hit_position[kMaxLayers] = {-100.0f};
   // global timing reference
   float t0 = -100.0f;
   float t_average[kMaxLayers] = {0.0f};
   int   n_int[kMaxLayers] = {0};
   // local timing reference per layer
   float t0_local[kMaxLayers] = {-100.0f};
   float sum_charge[kMaxLayers] = {0.0f};
   float max_charge[kMaxLayers] = {0.0f};
   // For strip and partially strip
   float leading_charge[kMaxLayers] = {0.0f};
   float sub_leading_charge[kMaxLayers] = {0.0f};
   int leading_ch[kMaxLayers] = {-99};
   int sub_leading_ch[kMaxLayers] = {-99};
   // For pixel
   int adjacent_ch_col[kMaxLayers][2] = { {-99, -99} }; // X
   int adjacent_ch_row[kMaxLayers][2] = { {-99, -99} }; // Y
   float adjacent_charge_col[kMaxLayers][2] = { {0.0f, 0.0f} }; // X
   float adjacent_charge_row[kMaxLayers][2] = { {0.0f, 0.0f} }; // Y
   
   for (int L = 0; L < kMaxLayers; L++) {
     // 1.
     //  The first loop to determine leanding channel in the L's layer
     for (int ch = 0; ch < kNCh[L]; ch++) {
       // get values
       auto n         = **nArr[L][ch];
       // get vectors
       auto& tot      = *totArr[L][ch];
       auto& t_lead   = *t_leadArr[L][ch];
       auto& t_trail  = *t_trailArr[L][ch];
       auto& t_rise   = *t_riseArr[L][ch];
       auto& charge   = *chargeArr[L][ch];
       auto& min_adc  = *min_adcArr[L][ch];
       auto& pedestal = *pedestalArr[L][ch];

       // Process per each pulse
       for (std::size_t i = 0; i < tot.GetSize(); i++) {
	 // Select only beam hit like pulse, reduce noise
	 if (t_lead[i] < 110. || t_lead[i] > 220.)
	   continue;

	 // requirement for ToT and min_adc
	 //if (tot[i]<2.5 && min_adc[i] < -20.0*tot[i] - 20.0)
	 if (tot[i] < 4.0 && t_rise[i] > 30.)
	   continue;

	 if (charge[i]  < 100.)
	   continue;

	 if (min_adc[i] < 50.) // Leading strip condition
	   continue;

	 // Find leading channel, and sub-leading channel for stip
	 if (L<4) { // Strip layer; this process only for strip
	   if (charge[i] > leading_charge[L]) { // Update leading strip 
	     sub_leading_charge[L] = leading_charge[L];
	     sub_leading_ch[L] = leading_ch[L];
	     leading_charge[L] = charge[i];
	     leading_ch[L] = ch;
	     // Get t0
	     if (L==0) {
	       t0 = t_lead[i];
	     }
	     t0_local[L] = t_lead[i];
	   } else if (charge[i] > sub_leading_charge[L]) { // Update sub-leading strip
	     sub_leading_charge[L] = charge[i];
	     sub_leading_ch[L] = ch;
	   }
	 } else { // == Pixel layer
	   if (charge[i] > max_charge[L]) {
	     // Update max_charge
	     max_charge[L] = charge[i];
	     leading_ch[L] = ch;
	   }
	 }

	 // Get charge --> should be leading hit only
	 sum_charge[L] += charge[i];
	 t_average[L]  += t_lead[i];
	 n_int[L]      += 1;
       }
     } // End of channel loop
     
     // Set the hit_encoder[L] to 1 if the layer has any hits
     if (n_int[L] > 0) {
       t_average[L] /= n_int[L];
       hit_encoder[L] = '1';
     }
     // Set adjacent channel for found leanding channel
     for (int ch = 0; ch < kNCh[L]; ch++) {
       if (mapping_row[ch] == mapping_row[leading_ch[L]]) {
	 if (mapping_col[ch] == mapping_col[leading_ch[L]] - 1)
	   adjacent_ch_col[L][0] = ch;
	 if (mapping_col[ch] == mapping_col[leading_ch[L]] + 1)
	   adjacent_ch_col[L][1] = ch;
       }
       if (mapping_col[ch] == mapping_col[leading_ch[L]]) {
	 if (mapping_row[ch] == mapping_row[leading_ch[L]] - 1)
	   adjacent_ch_row[L][0] = ch;
	 if (mapping_row[ch] == mapping_row[leading_ch[L]] + 1)
	   adjacent_ch_row[L][1] = ch;
       }
     }
   } // End of 1st Layer loop

   
   for (int L = 0; L < kMaxLayers; L++) {
     // 2.
     //  The second loop; 
     for (int ch = 0; ch < kNCh[L]; ch++) {
       // get values
       auto n         = **nArr[L][ch];
       // get vectors
       auto& tot      = *totArr[L][ch];
       auto& t_lead   = *t_leadArr[L][ch];
       auto& t_trail  = *t_trailArr[L][ch];
       auto& t_rise   = *t_riseArr[L][ch];
       auto& charge   = *chargeArr[L][ch];
       auto& min_adc  = *min_adcArr[L][ch];
       auto& pedestal = *pedestalArr[L][ch];

       // Process per each pulse
       for (std::size_t i = 0; i < tot.GetSize(); i++) {
	 // Select only beam hit like pulse, reduce noise
	 if (t_lead[i] < 110. || t_lead[i] > 220.)
	   continue;
	 
	 // requirement for ToT and min_adc
	 //if (tot[i]<2.5 && min_adc[i] < -20.0*tot[i] - 20.0)
	 if (tot[i] < 4.0 && t_rise[i] > 30.)
	   continue;

	 if (charge[i]  < 10.) // Very loose condition
	   continue;

	 if (min_adc[i] < 20.) // Sub-Leading strip condition
	   continue;

	 // Get charge
	 if (ch == adjacent_ch_row[L][0])
	   adjacent_charge_row[L][0] = charge[i];
	 if (ch == adjacent_ch_row[L][1])
	   adjacent_charge_row[L][1] = charge[i];
	 if (ch == adjacent_ch_col[L][0])
	   adjacent_charge_col[L][0] = charge[i];
	 if (ch == adjacent_ch_col[L][1])
	   adjacent_charge_col[L][1] = charge[i];
	 
	 // Skip event if number of hit layers is not 6 for pixel, If all strip have fit fill strip histograms
	 //if ((hit_encoder=="111111" && L>3) || (hit_encoder.rfind("1111", 0) == 0 && L<4)) {
	 if (L<4) { // Strip
	   if (hit_encoder.rfind("1111", 0) == 0) { // All strip layers have hits
	     // Fill histograms
	     h2_ToT_Charge[L][ch]->Fill(tot[i], charge[i]);
	     h2_ToT_Amp[L][ch]->Fill(tot[i], -1.0*min_adc[i]);
	     h2_Amp_Charge[L][ch]->Fill(-1.0*min_adc[i], charge[i]);
	     if (min_adc[i]<-20.) {
	       h2_Tlead_ToT[L][ch]->Fill(t_lead[i]/2.0 + t_trail[i]/2.0, tot[i]);
	       if (t_lead[i] -t_average[L] != 0)
		 h2_Tlead_Amp[L][ch]->Fill(-1.0*min_adc[i], t_lead[i] - t_average[L]);
	       
	       h2_Tlead_T0[L][ch]->Fill(t_lead[i],  t0);
	       float tlead_corr = t_lead[i] - f_thr_corr[L]->Eval(-1.0*min_adc[i]);
	       //h_Tlead[L][ch]->Fill(t_lead[i]-1.0*t0);
	       h_Tlead[L][ch]->Fill(tlead_corr - t_average[L]);
	       h2_Tlead[L]->Fill(ch, t_lead[i]-t_average[L]);
	     }
	   }
	 } else { // Pixel
	   if (1) {
	     // Fill histograms
	     h2_ToT_Charge[L][ch]->Fill(tot[i], charge[i]);
	     h2_ToT_Amp[L][ch]->Fill(tot[i], -1.0*min_adc[i]);
	     h2_Amp_Charge[L][ch]->Fill(-1.0*min_adc[i], charge[i]);
	     if (min_adc[i]<-20.) {
	       h2_Tlead_ToT[L][ch]->Fill(t_lead[i]/2.0 + t_trail[i]/2.0, tot[i]);
	       if (t_lead[i] -t_average[L] != 0)
		 h2_Tlead_Amp[L][ch]->Fill(-1.0*min_adc[i], t_lead[i] - t_average[L]);
	     
	       h2_Tlead_T0[L][ch]->Fill(t_lead[i],  t0);
	       float tlead_corr = t_lead[i] - f_thr_corr[L]->Eval(-1.0*min_adc[i]);
	       //h_Tlead[L][ch]->Fill(t_lead[i]-1.0*t0);
	       h_Tlead[L][ch]->Fill(tlead_corr - t_average[L]);
	       h2_Tlead[L]->Fill(ch, t_lead[i]-t_average[L]);
	     }
	   }
	 }
       } // End of pulse loop
       
     } // End of channel loop
     
     // Process
     if (L<4) { // Strip 0,1,2,3
       if (abs(leading_ch[L]-sub_leading_ch[L])==1) { // Find two successful hits
	 // DEBUG cout << leading_ch << " " << leading_charge << ":" << sub_leading_ch << " " << sub_leading_charge << endl;
	 hit_position[L] = (leading_ch[L]*leading_charge[L] + sub_leading_ch[L]*sub_leading_charge[L])/(leading_charge[L] + sub_leading_charge[L]) * 0.5;
       } else if (leading_ch[L]>0) { // Find leading channel only
	 hit_position[L] = leading_ch[L]*0.5;
       }
       if (leading_charge[L] > 30.) {
	 h_max_charge[L]->Fill(leading_charge[L]);
       }

     } else { // Pixel
       if (max_charge[L] > 100.) {
	 h_max_charge[L]->Fill(max_charge[L]);
       }
     }

     if (sum_charge[L] > 0.0) {
       h_sum_charge[L]->Fill(sum_charge[L]);
     }
   } // End of 2nd Layer loop

   if (hit_position[0]>0.0 && hit_position[1]>0.0 && hit_position[2]>0.0 && hit_position[3]>0.0) {
     h_strip_x_cor->Fill(hit_position[0]-hit_position[2]);
     h_strip_y_cor->Fill(hit_position[1]-hit_position[3]);
     h2_strip_x_cor->Fill(hit_position[0],hit_position[2]);
     h2_strip_y_cor->Fill(hit_position[1],hit_position[3]);
   }
   return true;
}

void MySelection::SlaveTerminate()
{
   // The SlaveTerminate() function is called after all entries or objects
   // have been processed. When running with PROOF SlaveTerminate() is called
   // on each slave server.

}

void MySelection::Terminate()
{
   // The Terminate() function is the last function to be called during
   // a query. It always runs on the client, it can be used to present
   // the results graphically or save the results to file.
  TFile *fout = new TFile("histograms_"+run_number+".root", "RECREATE");

  h_strip_x_cor->Write();
  h_strip_y_cor->Write();
  h2_strip_x_cor->Write();
  h2_strip_y_cor->Write();
  
    for (int L = 0; L < kMaxLayers; L++) {
      h_sum_charge[L]->Write();
      h_max_charge[L]->Write();
      h2_Tlead[L]->Write();
        for (int ch = 0; ch < kNCh[L]; ch++) {
            h2_ToT_Charge[L][ch]->Write();
            h2_ToT_Amp[L][ch]->Write();
            h2_Amp_Charge[L][ch]->Write();
            h2_Tlead_ToT[L][ch]->Write();
	    h2_Tlead_Amp[L][ch]->Write();
	    h2_Tlead_T0[L][ch]->Write();
	    h_Tlead[L][ch]->Write();
        }
    }

    fout->Close();

}
