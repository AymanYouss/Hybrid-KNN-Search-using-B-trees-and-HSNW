#include "dataset.h"
#include <fstream>
#include <iostream>
#include <sstream>


float euclideanDistance(const std::vector<float>& a, const std::vector<float>& b) {
    float sum = 0.0f;
    for (size_t i = 0; i < a.size(); i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrt(sum);
}

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

bool loadDatasetFromBin(const std::string &binFile,
                        std::vector<DataPoint> &dataset,
                        size_t &dimOut)
{
    std::ifstream fin(binFile, std::ios::binary);
    if (!fin.is_open()) {
        std::cerr << "Error opening dataset bin file: " << binFile << std::endl;
        return false;
    }

    uint32_t N = 0;
    fin.read(reinterpret_cast<char*>(&N), sizeof(uint32_t));
    if (!fin.good()) {
        std::cerr << "Error reading num_vectors from " << binFile << std::endl;
        return false;
    }

    // We know each vector is 100-dim. We'll store the "timestamp" in DataPoint.attribute.
    // The bin format has: category (1 float), timestamp (1 float), coords (100 floats)
    const size_t total_floats_per_vector = 102; 
    std::vector<float> buffer(total_floats_per_vector);

    dataset.resize(N);
    for (uint32_t i = 0; i < N; i++) {
        fin.read(reinterpret_cast<char*>(buffer.data()),
                 total_floats_per_vector * sizeof(float));
        if (!fin.good()) {
            std::cerr << "Error reading vector #" << i << " from " << binFile << std::endl;
            return false;
        }
        // ignore buffer[0] (category)
        float timestamp = buffer[1]; // store this in DataPoint.attribute
        DataPoint dp;
        dp.coords.resize(100);
        for (size_t d = 0; d < 100; d++) {
            dp.coords[d] = buffer[2 + d];
        }
        dp.attribute = timestamp;
        dataset[i] = std::move(dp);
    }
    fin.close();
    dimOut = 100;  // fixed dimension
    return true;
}

bool loadQueriesFromBin(const std::string &binFile,
                        std::vector<Query> &queries)
{
    std::ifstream fin(binFile, std::ios::binary);
    if (!fin.is_open()) {
        std::cerr << "Error opening query bin file: " << binFile << std::endl;
        return false;
    }

    uint32_t numQueries = 0;
    fin.read(reinterpret_cast<char*>(&numQueries), sizeof(uint32_t));
    if (!fin.good()) {
        std::cerr << "Error reading num_queries from " << binFile << std::endl;
        return false;
    }

    const size_t floats_per_query = 104; // per code 2
    std::vector<float> buffer(floats_per_query);

    for (uint32_t i = 0; i < numQueries; i++) {
        fin.read(reinterpret_cast<char*>(buffer.data()),
                 floats_per_query * sizeof(float));
        if (!fin.good()) {
            std::cerr << "Error reading query #" << i << " from " << binFile << std::endl;
            return false;
        }

        float query_type = buffer[0];
        // float category = buffer[1]; // ignored
        float l = buffer[2];
        float r = buffer[3];

        // If query_type == 2 or 3, we keep it
        if (static_cast<int>(query_type) == 2 || static_cast<int>(query_type) == 3) {
            Query q;
            q.queryVec.resize(100);
            for (size_t d = 0; d < 100; d++) {
                q.queryVec[d] = buffer[4 + d];
            }
            q.a_min = l;
            q.a_max = r;
            queries.push_back(std::move(q));
        }
    }
    fin.close();
    return true;
}