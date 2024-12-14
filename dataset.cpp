#include "dataset.h"
#include <fstream>
#include <iostream>
#include <sstream>


// Implementation of loadDataset
bool loadDataset(const std::string& filename, std::vector<DataPoint>& dataset, size_t& d_out) {
    std::ifstream fin(filename);
    if(!fin.is_open()) {
        std::cerr << "Error opening file: " << filename << "\n";
        return false;
    }
    size_t N, d;
    fin >> N >> d;
    if(!fin) {
        std::cerr << "Error reading N and d from dataset file\n";
        return false;
    }
    d_out = d;
    dataset.resize(N);
    for(size_t i = 0; i < N; i++) {
        dataset[i].coords.resize(d);
        for(size_t j = 0; j < d; j++){
            fin >> dataset[i].coords[j];
        }
        fin >> dataset[i].attribute;
        if(!fin) {
            std::cerr << "Error reading data point " << i << "\n";
            return false;
        }
    }
    fin.close();
    std::cout << "Loaded " << N << " data points of dimension " << d << " from " << filename << "\n";
    return true;
}
