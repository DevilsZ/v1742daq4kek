//////////////////////////////////////////////////////////
// This class has been automatically generated on
// Fri Jun 12 09:50:11 2026 by ROOT version 6.36.06
// from TTree pulse_tree/Per-channel pulse data
// found on file: out_run05265.root
//////////////////////////////////////////////////////////

#ifndef MySelection_h
#define MySelection_h

#include <TROOT.h>
#include <TChain.h>
#include <TFile.h>
#include <TSelector.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <TTreeReaderArray.h>
#include <TObjString.h>
#include <TH1.h>
#include <TH2.h>
// Headers needed by this particular selector
#include <vector>


class MySelection : public TSelector {
public :
  TTreeReader     fReader;  //!the tree reader
  TTree          *fChain = 0;   //!pointer to the analyzed TTree or TChain
  TString input_filename;
  TString run_number;
  
  // =============================================================================
  //  Cut parameters  (edit here freely)
  // =============================================================================
  static constexpr float kToTMin      =   2.0;  // a.u.
  static constexpr float kToTMax      = 200.0;  // a.u.
  static constexpr float kChargeMin   =   0.5;  // a.u.
  static constexpr float kChargeMax   = 500.0;
  static constexpr float kTleadMin    =  -9999; // ns
  static constexpr float kTleadMax    =   9999;

  // =============================================================================
  //  Helper: layer names
  // =============================================================================
  static constexpr const char* kLayerNames[6] = {
    "strip_front_x","strip_front_y","strip_rear_x","strip_rear_y",
    "pixel_front","pixel_rear"
  };
  
  static constexpr bool kIsPixel[6] = {false,false,false,false,true,true};
  static constexpr int  kNCh[6]     = {8, 8, 8, 8, 16, 16};
  
  // 6 layers × max 16 channels
  static constexpr int kMaxLayers = 6;
  static constexpr int kMaxCh     = 16;

  // 2D histograms
  TH2F* h2_ToT_Charge[kMaxLayers][kMaxCh] = {{nullptr}};
  TH2F* h2_ToT_Amp[kMaxLayers][kMaxCh]    = {{nullptr}};
  TH2F* h2_Amp_Charge[kMaxLayers][kMaxCh] = {{nullptr}};
  TH2F* h2_Tlead_ToT[kMaxLayers][kMaxCh]  = {{nullptr}};
  TH2F* h2_Tlead_Amp[kMaxLayers][kMaxCh]  = {{nullptr}};
  TH2F* h2_Tlead_T0[kMaxLayers][kMaxCh]   = {{nullptr}};

  // 1D histograms / Layer / ch
  TH1F* h_Tlead[kMaxLayers][kMaxCh]  = {{nullptr}};

  // 2D histograms / Layer
  TH2F* h2_Tlead[kMaxLayers] = {nullptr};
  
  // 1D historgams / Layer
  TH1F* h_sum_charge[kMaxLayers] = {nullptr};
  TH1F* h_max_charge[kMaxLayers] = {nullptr};
  // 1D correlation
  TH1F* h_strip_x_cor = nullptr;
  TH1F* h_strip_y_cor = nullptr;
  // 2D correlation
  TH2F* h2_strip_x_cor = nullptr;
  TH2F* h2_strip_y_cor = nullptr;
  
   // Readers to access the data (delete the ones you do not need).
  TTreeReaderValue<Long64_t> ev_id = {fReader, "ev_id"};

  TTreeReaderValue<int>*   nArr[kMaxLayers][kMaxCh];
  TTreeReaderArray<float>* totArr[kMaxLayers][kMaxCh];
  TTreeReaderArray<float>* t_leadArr[kMaxLayers][kMaxCh];
  TTreeReaderArray<float>* chargeArr[kMaxLayers][kMaxCh];
  TTreeReaderArray<float>* min_adcArr[kMaxLayers][kMaxCh];
  TTreeReaderArray<float>* pedestalArr[kMaxLayers][kMaxCh];
    
   MySelection(TTree * /*tree*/ =0) { }
   ~MySelection() override { }
   Int_t   Version() const override { return 2; }
   void    Begin(TTree *tree) override;
   void    SlaveBegin(TTree *tree) override;
   void    Init(TTree *tree) override;
   bool    Notify() override;
   bool    Process(Long64_t entry) override;
   Int_t   GetEntry(Long64_t entry, Int_t getall = 0) override { return fChain ? fChain->GetTree()->GetEntry(entry, getall) : 0; }
   void    SetOption(const char *option) override { fOption = option; }
   void    SetObject(TObject *obj) override { fObject = obj; }
   void    SetInputList(TList *input) override { fInput = input; }
   TList  *GetOutputList() const override { return fOutput; }
   void    SlaveTerminate() override;
   void    Terminate() override;

   ClassDefOverride(MySelection,0);

};

#endif

#ifdef MySelection_cxx
void MySelection::Init(TTree *tree)
{
   // The Init() function is called when the selector needs to initialize
   // a new tree or chain. Typically here the reader is initialized.
   // It is normally not necessary to make changes to the generated
   // code, but the routine can be extended by the user if needed.
   // Init() will be called many times when running on PROOF
   // (once per file to be processed).

   fReader.SetTree(tree);

   for (int L = 0; L < kMaxLayers; L++) {
     for (int ch = 0; ch < kNCh[L]; ch++) {
       
       TString bname_n        = Form("pulse_%s_ch%02d_n",        kLayerNames[L], ch);
       TString bname_tot      = Form("pulse_%s_ch%02d_tot",      kLayerNames[L], ch);
       TString bname_t_lead   = Form("pulse_%s_ch%02d_t_lead",   kLayerNames[L], ch);
       TString bname_charge   = Form("pulse_%s_ch%02d_charge",   kLayerNames[L], ch);
       TString bname_min_adc  = Form("pulse_%s_ch%02d_min_adc",  kLayerNames[L], ch);
       TString bname_pedestal = Form("pulse_%s_ch%02d_pedestal", kLayerNames[L], ch);
       
       nArr[L][ch]        = new TTreeReaderValue<int>(fReader,   bname_n);
       totArr[L][ch]      = new TTreeReaderArray<float>(fReader, bname_tot);
       t_leadArr[L][ch]   = new TTreeReaderArray<float>(fReader, bname_t_lead);
       chargeArr[L][ch]   = new TTreeReaderArray<float>(fReader, bname_charge);
       min_adcArr[L][ch]  = new TTreeReaderArray<float>(fReader, bname_min_adc);
       pedestalArr[L][ch] = new TTreeReaderArray<float>(fReader, bname_pedestal);
     }
   }
   
}

bool MySelection::Notify()
{
   // The Notify() function is called when a new file is opened. This
   // can be either for a new TTree in a TChain or when when a new TTree
   // is started when using PROOF. It is normally not necessary to make changes
   // to the generated code, but the routine can be extended by the
   // user if needed. The return value is currently not used.

   return true;
}

#endif // #ifdef MySelection_cxx
