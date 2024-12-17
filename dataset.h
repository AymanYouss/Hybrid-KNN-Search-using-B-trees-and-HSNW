#ifndef DATASET_H
#define DATASET_H

#include <vector>
#include <string>
#include <cmath>
// A data point in d-dimensions, plus a continuous attribute
struct DataPoint {
    std::vector<float> coords;  // length d
    float attribute;            // the continuous attribute indexed by B+ tree
};

struct Query {
    std::vector<float> queryVec;
    float a_min;
    float a_max;
};

// A simple function to compute Euclidean distance
float euclideanDistance(const std::vector<float>& a, const std::vector<float>& b);

// Load a dataset from a text file
// Format assumed:
//   First line: N d
//   Then N lines, each containing d floats for coords + 1 float for attribute
// The function will populate "dataset" and set "d_out" to the dimensionality
bool loadDataset(const std::string& filename, std::vector<DataPoint>& dataset, size_t& d_out);

bool loadQueriesFromBin(const std::string &binFile,
                        std::vector<Query> &queries);

bool loadDatasetFromBin(const std::string &binFile,
                        std::vector<DataPoint> &dataset,
                        size_t &dimOut);

#endif
