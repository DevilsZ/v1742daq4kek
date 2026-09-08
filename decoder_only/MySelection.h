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
#include <TF1.h>
#include <TH1.h>
#include <TH2.h>
// Headers needed by this particular selector
#include <vector>


class MySelection : public TSelector {
public :
  TTreeReader     fReader;      //!the tree reader
  TTree          *fChain = 0;   //!pointer to the analyzed TTree or TChain
  TString         input_filename;
  TString         run_number;
  
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
  
  // 6 layers, max 16 channels
  static constexpr int kMaxLayers = 6;
  static constexpr int kMaxCh     = 16;

  static constexpr int PIXEL_NROW = 4;
  static constexpr int PIXEL_NCOL = 4;
  static constexpr int mapping_row[16] = {0, 0, 1, 1, 2, 2, 3, 3,
					  3, 3, 2, 2, 1, 1, 0, 0};
  static constexpr int mapping_col[16] = {2, 3, 2, 3, 2, 3, 2, 3,
					  0, 1, 0, 1, 0, 1, 0, 1};

  // Z positions [mm] -- adjust to actual geometry
  static constexpr float Z_STRIP_FRONT_X = 0.0f;
  static constexpr float Z_STRIP_FRONT_Y = 24.0f;
  static constexpr float Z_STRIP_BACK_X  = 375.0f;
  static constexpr float Z_STRIP_BACK_Y  = 399.0f;
  static constexpr float Z_PIXEL_FRONT   = 490.5f;
  static constexpr float Z_PIXEL_BACK    = 510.0f;  

  // Alignment correction factor
  //static constexpr float alingment_cf[2] = {0.5, 0.5};
  //static constexpr float alingment_cf[2] = {0.0, 0.0};
  static constexpr float alingment_cf[2][kMaxLayers] = {
    {0.0f, 0.0f, -0.83,  0.0f, -2.41, -2.24},
    {0.0f, 0.0f,  0.0f, -1.44, -3.01, -3.42}
  };

 
  // Time walk correction
  TF1* f_thr_corr[kMaxLayers];
  static constexpr float p0[kMaxLayers] = {-3.75, -1.70, -4.20, -1.20, -1.16, -1.64};
  static constexpr float p1[kMaxLayers] = {  9.8,  13.7,  11.2,  15.9,  9.10,  10.0};
  static constexpr float p2[kMaxLayers] = { 90.9,  42.8,  82.0,  31.3,  29.4,  34.9};
  /* Results from fitting
    p0                        =     -3.74639   +/-   0.218881    
    p1                        =      9.84295   +/-   0.200928    
    p2                        =      90.9227   +/-   6.86942     

    p0                        =      -1.6942   +/-   0.0991329   
    p1                        =      13.7421   +/-   0.951479    
    p2                        =      42.7894   +/-   2.6912     

    p0                        =     -4.19363   +/-   0.272946    
    p1                        =      11.1639   +/-   0.315717    
    p2                        =       81.963   +/-   7.39123
    
    p0                        =      -1.1985   +/-   0.0687073   
    p1                        =      15.9389   +/-   1.46515     
    p2                        =      31.3162   +/-   1.88374   
    
    p0                        =     -1.16149   +/-   0.120147    
    p1                        =      9.10492   +/-   4.69797     
    p2                        =       29.424   +/-   8.03004 
    
    p0                        =     -1.63706   +/-   0.064721    
    p1                        =      9.98694   +/-   1.36857     
    p2                        =      34.8656   +/-   3.25378
  */

  // Sanity checks
  //TH1F* h_
  
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
  TH2F* h2_DUT_x_cor[kMaxLayers] = {nullptr};
  TH2F* h2_DUT_y_cor[kMaxLayers] = {nullptr};
  TH2F* h2_DUT_x_cor_CS[kMaxLayers] = {nullptr};
  TH2F* h2_DUT_y_cor_CS[kMaxLayers] = {nullptr};
  TH1F* h_DUT_x_diff[kMaxLayers] = {nullptr};
  TH1F* h_DUT_y_diff[kMaxLayers] = {nullptr};
  TH1F* h_DUT_x_diff_CS[kMaxLayers] = {nullptr};
  TH1F* h_DUT_y_diff_CS[kMaxLayers] = {nullptr};
  
   // Readers to access the data (delete the ones you do not need).
  TTreeReaderValue<Long64_t> ev_id = {fReader, "ev_id"};

  TTreeReaderValue<int>*   nArr[kMaxLayers][kMaxCh];
  TTreeReaderArray<float>* totArr[kMaxLayers][kMaxCh];
  TTreeReaderArray<float>* t_leadArr[kMaxLayers][kMaxCh];
  TTreeReaderArray<float>* t_trailArr[kMaxLayers][kMaxCh];
  TTreeReaderArray<float>* t_riseArr[kMaxLayers][kMaxCh];
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
   double  extrap_fn(double c_front, double c_back, float z_front, float z_back, float z_target);
  
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
       TString bname_t_trail  = Form("pulse_%s_ch%02d_t_trail",  kLayerNames[L], ch);
       TString bname_t_rise   = Form("pulse_%s_ch%02d_t_rise",   kLayerNames[L], ch);
       TString bname_charge   = Form("pulse_%s_ch%02d_charge",   kLayerNames[L], ch);
       TString bname_min_adc  = Form("pulse_%s_ch%02d_min_adc",  kLayerNames[L], ch);
       TString bname_pedestal = Form("pulse_%s_ch%02d_pedestal", kLayerNames[L], ch);
       
       nArr[L][ch]        = new TTreeReaderValue<int>(fReader,   bname_n);
       totArr[L][ch]      = new TTreeReaderArray<float>(fReader, bname_tot);
       t_leadArr[L][ch]   = new TTreeReaderArray<float>(fReader, bname_t_lead);
       t_trailArr[L][ch]  = new TTreeReaderArray<float>(fReader, bname_t_trail);
       t_riseArr[L][ch]   = new TTreeReaderArray<float>(fReader, bname_t_rise);
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
