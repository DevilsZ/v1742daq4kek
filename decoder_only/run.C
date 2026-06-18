void run(const char* filename) {
    TChain ch("pulse_tree");
    ch.Add(filename);
    ch.Process("MySelection.C+", filename);
}
