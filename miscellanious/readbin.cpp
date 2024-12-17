#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <cstring>
#include "utils.h"
//-------------------------------------------------------------------------------------
// Helper function to read the base dataset (dummy-data.bin).
// Each vector in the dataset has the following structure (in float32):
//   [category (1 float), timestamp (1 float), vector_dim=100 floats]
//
// The file format is:
//   [num_vectors (uint32_t)]
//   For each vector (total = num_vectors):
//       102 floats = (1 category) + (1 timestamp) + (100-dim vector)
//
// We want to ignore category, store the 100-dim vector + timestamp.
//-------------------------------------------------------------------------------------
bool ReadBaseData(const std::string &file_path,
                  uint32_t &N,
                  std::vector<float> &all_vectors,
                  std::vector<float> &timestamps)
{
    std::ifstream fin(file_path, std::ios::binary);
    if (!fin) {
        std::cerr << "Error opening base data file: " << file_path << std::endl;
        return false;
    }

    // Read the number of vectors
    fin.read(reinterpret_cast<char*>(&N), sizeof(uint32_t));
    if (!fin.good()) {
        std::cerr << "Error reading num_vectors from " << file_path << std::endl;
        return false;
    }

    // Allocate memory for reading
    all_vectors.resize(N * 100);
    timestamps.resize(N);

    // Each vector has 102 floats: category(1) + timestamp(1) + 100 floats
    const size_t total_floats_per_vector = 102;

    std::vector<float> buffer(total_floats_per_vector);

    for (uint32_t i = 0; i < N; ++i) {
        fin.read(reinterpret_cast<char*>(buffer.data()),
                 total_floats_per_vector * sizeof(float));

        if (!fin.good()) {
            std::cerr << "Error reading vector " << i << " from " << file_path << std::endl;
            return false;
        }

        // The first float is category -> ignore
        // The second float is the timestamp
        float timestamp = buffer[1];

        // The next 100 floats are the vector
        for (int d = 0; d < 100; ++d) {
            all_vectors[i * 100 + d] = buffer[2 + d];
        }
        timestamps[i] = timestamp;
    }

    fin.close();
    return true;
}

//-------------------------------------------------------------------------------------
// Helper function to read the queries (dummy-queries.bin).
// Each query in the dataset has the following structure (in float32):
//   [query_type (1 float), category (1 float), l (1 float), r (1 float), 100-dim query vector]
//
// The file format is:
//   [num_queries (uint32_t)]
//   For each query (total = num_queries):
//       104 floats = (1 query_type) + (1 category) + (l, r) + (100-dim vector)
//
// We only keep queries where query_type == 2 or 3.
//-------------------------------------------------------------------------------------
bool ReadQueryData(const std::string &file_path,
                   std::vector<std::vector<float>> &filtered_queries,
                   std::vector<float> &l_vals,
                   std::vector<float> &r_vals)
{
    std::ifstream fin(file_path, std::ios::binary);
    if (!fin) {
        std::cerr << "Error opening query file: " << file_path << std::endl;
        return false;
    }

    uint32_t num_queries;
    fin.read(reinterpret_cast<char*>(&num_queries), sizeof(uint32_t));
    if (!fin.good()) {
        std::cerr << "Error reading num_queries from " << file_path << std::endl;
        return false;
    }

    // Each query has 104 floats
    const size_t floats_per_query = 104;
    std::vector<float> buffer(floats_per_query);

    for (uint32_t i = 0; i < num_queries; ++i) {
        fin.read(reinterpret_cast<char*>(buffer.data()),
                 floats_per_query * sizeof(float));
        if (!fin.good()) {
            std::cerr << "Error reading query " << i << " from " << file_path << std::endl;
            return false;
        }

        float query_type = buffer[0];
        float category   = buffer[1]; // we mostly ignore
        float l          = buffer[2];
        float r          = buffer[3];

        // If query_type is 2 or 3, we keep it.
        if (static_cast<int>(query_type) == 2 || static_cast<int>(query_type) == 3) {
            // The next 100 floats are the query vector
            std::vector<float> qvec(100);
            for (int d = 0; d < 100; ++d) {
                qvec[d] = buffer[4 + d];
            }
            filtered_queries.push_back(qvec);
            l_vals.push_back(l);
            r_vals.push_back(r);
        }
    }

    fin.close();
    return true;
}

//-------------------------------------------------------------------------------------
// Write the base data to a text file of the form:
//
//   N 100
//   v1_0 v1_1 ... v1_99 T1
//   v2_0 v2_1 ... v2_99 T2
//   ...
//   vN_0 ... vN_99 TN
//
// Here we store only the 100-dim vector and the timestamp, ignoring the category.
//-------------------------------------------------------------------------------------
bool WriteBaseDataToTxt(const std::string &out_path,
                        uint32_t N,
                        const std::vector<float> &all_vectors,
                        const std::vector<float> &timestamps)
{
    std::ofstream fout(out_path);
    if (!fout) {
        std::cerr << "Error opening output file: " << out_path << std::endl;
        return false;
    }

    // First line: N 100
    fout << N << " 100" << std::endl;

    // Next N lines: each line has 100 floats for the vector + 1 float for the timestamp
    for (uint32_t i = 0; i < N; ++i) {
        // The 100-dim vector
        for (int d = 0; d < 100; ++d) {
            fout << all_vectors[i * 100 + d] << " ";
        }
        // The timestamp
        fout << timestamps[i] << std::endl;
    }

    fout.close();
    return true;
}

//-------------------------------------------------------------------------------------
// Write the filtered queries to a text file of the form:
//
//   Q
//   q1_0 q1_1 ... q1_99 l1 r1 100
//   q2_0 q2_1 ... q2_99 l2 r2 100
//   ...
//   qQ_0 ... qQ_99 lQ rQ 100
//
// Where Q is the number of queries with query_type 2 or 3.
//-------------------------------------------------------------------------------------
bool WriteQueryDataToTxt(const std::string &out_path,
                         const std::vector<std::vector<float>> &filtered_queries,
                         const std::vector<float> &l_vals,
                         const std::vector<float> &r_vals)
{
    if (filtered_queries.size() != l_vals.size() ||
        filtered_queries.size() != r_vals.size())
    {
        std::cerr << "Error: mismatch in filtered queries and timestamp ranges." << std::endl;
        return false;
    }

    std::ofstream fout(out_path);
    if (!fout) {
        std::cerr << "Error opening output file: " << out_path << std::endl;
        return false;
    }

    // First line: number of filtered queries
    size_t Q = filtered_queries.size();
    fout << Q << std::endl;

    // Each line: 100 floats for the query vector, l, r, and then "100" at the end
    for (size_t i = 0; i < Q; ++i) {
        for (int d = 0; d < 100; ++d) {
            fout << filtered_queries[i][d] << " ";
        }
        fout << l_vals[i] << " " << r_vals[i] << " 100" << std::endl;
    }

    fout.close();
    return true;
}

//-------------------------------------------------------------------------------------
// Combined main()
// This reads the base data from "dummy-data.bin", writes it out to "base_data.txt"
// ignoring the category attribute. Then reads the queries from "dummy-queries.bin",
// filters queries of type 2 or 3, and writes them to "query_data.txt" in the desired
// format.
//-------------------------------------------------------------------------------------
int main()
{
    const std::string base_file_path  = "../data/sigmod/contest-data-release-1m.bin";
    const std::string query_file_path = "../data/sigmod/contest-queries-release-1m.bin";

    const std::string base_out_txt    = "../data/sigmodtxt/contest-data-release-1m.txt";
    const std::string query_out_txt   = "../data/sigmodtxt/contest-queries-release-1m.txt";

    // Read base data
    uint32_t N = 0;
    std::vector<float> base_vectors;   // size N * 100
    std::vector<float> base_timestamps; // size N

    if (!ReadBaseData(base_file_path, N, base_vectors, base_timestamps)) {
        std::cerr << "Failed to read base data from " << base_file_path << std::endl;
        return 1;
    }

    // Write base data to text file
    if (!WriteBaseDataToTxt(base_out_txt, N, base_vectors, base_timestamps)) {
        std::cerr << "Failed to write base data to " << base_out_txt << std::endl;
        return 1;
    }

    // Read query data
    std::vector<std::vector<float>> filtered_queries; // will hold the 100-dim vectors
    std::vector<float> l_vals;
    std::vector<float> r_vals;

    if (!ReadQueryData(query_file_path, filtered_queries, l_vals, r_vals)) {
        std::cerr << "Failed to read query data from " << query_file_path << std::endl;
        return 1;
    }

    // Write the filtered queries (type 2 or 3) to text file
    if (!WriteQueryDataToTxt(query_out_txt, filtered_queries, l_vals, r_vals)) {
        std::cerr << "Failed to write query data to " << query_out_txt << std::endl;
        return 1;
    }

    std::cout << "Successfully wrote base data to " << base_out_txt
              << " and query data to " << query_out_txt << std::endl;
    return 0;
}