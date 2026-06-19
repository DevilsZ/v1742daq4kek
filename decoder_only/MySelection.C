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
//


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
  int run = 0;
  sscanf(input_filename.Data(), "out_run%d.root", &run);
  run_number = Form("%05d",run);
}

void MySelection::SlaveBegin(TTree * /*tree*/)
{
   // The SlaveBegin() function is called after the Begin() function.
   // When running with PROOF SlaveBegin() is called on each slave server.
   // The tree argument is deprecated (on PROOF 0 is passed).

   TString option = GetOption();

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
     GetOutputList()->Add(h_sum_charge[L]);
     h_max_charge[L] = new TH1F(Form("h_max_charge_%s", kLayerNames[L]),
				Form("h_max_charge_%s", kLayerNames[L]),
				100, 0, 4000);
     GetOutputList()->Add(h_max_charge[L]);

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
				       1000, 0, 1000, 100, 0, 20);
       h2_Tlead_Amp[L][ch]  = new TH2F(Form("h2_Tlead_Amp_%s_ch%02d", kLayerNames[L], ch),
				       Form("Tlead vs Amp %s ch%02d;t_lead;Amp", kLayerNames[L], ch),
				       1000, 0, 1000, 1000, 0, 1000);
       //
       GetOutputList()->Add(h2_ToT_Charge[L][ch]);
       GetOutputList()->Add(h2_ToT_Amp[L][ch]);
       GetOutputList()->Add(h2_Amp_Charge[L][ch]);
       GetOutputList()->Add(h2_Tlead_ToT[L][ch]);
       GetOutputList()->Add(h2_Tlead_Amp[L][ch]);
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
   // 1111** all strip have hit
   std::string hit_encoder = "000000";

   for (int L = 0; L < kMaxLayers; L++) {
     for (int ch = 0; ch < kNCh[L]; ch++) {
       
       int n = **nArr[L][ch];
       
       if (n > 0)
	 hit_encoder[L] = '1';
     }
   }

   //   if (hit_encoder.starts_with("1111"))
   if (hit_encoder.rfind("1111", 0) == 0) {
     // cout << "Strip Tracking..." << endl;
   }
   
   //
   float hit_position[kMaxLayers] = {-100.0f};
   
   for (int L = 0; L < kMaxLayers; L++) {
     float sum_charge = 0.0f;
     float max_charge = 0.0f;
     // For strip
     float leading_charge = 0.0f;
     float sub_leading_charge = 0.0f;
     int leading_ch = -99;
     int sub_leading_ch = -99;
     
     for (int ch = 0; ch < kNCh[L]; ch++) {
       // get values
       auto n         = **nArr[L][ch];
       // get vectors
       auto& tot      = *totArr[L][ch];
       auto& t_lead   = *t_leadArr[L][ch];
       auto& charge   = *chargeArr[L][ch];
       auto& min_adc  = *min_adcArr[L][ch];
       auto& pedestal = *pedestalArr[L][ch];

       // Process per each pulse
       for (std::size_t i = 0; i < tot.GetSize(); i++) {
	 // Select only beam hits, reduce noise
	 if (t_lead[i] < 2 || t_lead[i] > 250)
	   continue;

	 // this process only for strip
	 if (L<4) { // == Strip layer
	   if (charge[i] > leading_charge) {
	     sub_leading_charge = leading_charge;
	     sub_leading_ch = leading_ch;
	     leading_charge = charge[i];
	     leading_ch = ch;
	   } else if (charge[i] > sub_leading_charge) {
	     sub_leading_charge = charge[i];
	     sub_leading_ch = ch;
	   }
	 } else { // Pixel
	   if (charge[i] > max_charge) {
	     // Update max_charge
	     max_charge = charge[i];
	   }
	 }

	 // Get charge
	 if (tot[i]>2.0 && charge[i]>100.)
	   sum_charge += charge[i];
	 
	 // Skip event if number of hit layers is not 6 for pixel, If all strip have fit fill strip histograms
	 if ((hit_encoder=="111111" && L>3) || (hit_encoder.rfind("1111", 0) == 0 && L<4)) {
	   h2_ToT_Charge[L][ch]->Fill(tot[i], charge[i]);
	   h2_ToT_Amp[L][ch]->Fill(tot[i], -1.0*min_adc[i]);
	   h2_Amp_Charge[L][ch]->Fill(-1.0*min_adc[i], charge[i]);
	   h2_Tlead_ToT[L][ch]->Fill(t_lead[i], tot[i]);
	   h2_Tlead_Amp[L][ch]->Fill(t_lead[i], -1.0*min_adc[i]);
	 }
       } // End of pulse loop
     } // End of channel loop

     // Process
     if (L<4) { // Strip 0,1,2,3
       if (abs(leading_ch-sub_leading_ch)==1) { // Find two successful hits
	 // DEBUG cout << leading_ch << " " << leading_charge << ":" << sub_leading_ch << " " << sub_leading_charge << endl;
	 hit_position[L] = (leading_ch*leading_charge + sub_leading_ch*sub_leading_charge)/(leading_charge + sub_leading_charge) * 0.5;
       } else if (leading_ch>0) { // Find leading channel only
	 hit_position[L] = leading_ch*0.5;
       }
     }

     
     if (sum_charge > 0.0) {
       h_sum_charge[L]->Fill(sum_charge);
     }
     if (max_charge > 100.) {
       h_max_charge[L]->Fill(max_charge);
     }
   } // End of Layer loop
   if (
       hit_position[0]>0.0 &&
       hit_position[1]>0.0 &&
       hit_position[2]>0.0 &&
       hit_position[3]>0.0
       ) {
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
        for (int ch = 0; ch < kNCh[L]; ch++) {
            h2_ToT_Charge[L][ch]->Write();
            h2_ToT_Amp[L][ch]->Write();
            h2_Amp_Charge[L][ch]->Write();
            h2_Tlead_ToT[L][ch]->Write();
	    h2_Tlead_Amp[L][ch]->Write();
        }
    }

    fout->Close();

}
