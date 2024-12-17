#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>

// We replicate minimal struct + function definitions that were presumably in "dataset.h"
struct DataPoint {
    std::vector<float> coords; // the 100-dim vector
    float attribute;           // the timestamp from the .bin file
};

// Simple Euclidean distance
float euclideanDistance(const std::vector<float> &a, const std::vector<float> &b) {
    float sum = 0.0f;
    for (size_t i = 0; i < a.size(); i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

// HNSWWrapper, BPlusTree, etc. — same as in code 1 (you said "Include your provided headers").
// For simplicity, let's assume we still have those header implementations available:
#include "hnsw_wrapper.h"  // HNSWWrapper class
#include "bplustreecpp.h"  // BPlusTree class

// Global map: attribute value -> vector of dataset indices
static std::unordered_map<double, std::vector<size_t>> attributeMap;

/********************************************************************/
/***                    HELPER FUNCTIONS                          ***/
/********************************************************************/

// Directly read the dataset from a .bin file
// Format of `.bin` (as in code 2):
//   [num_vectors (uint32_t)]
//   For each vector: 102 floats = (1 category + 1 timestamp + 100 dims)
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

// Directly read queries from a .bin file
// Format of `.bin` (as in code 2):
//   [num_queries (uint32_t)]
//   For each query: 104 floats = (query_type + category + l + r + 100-dim vec)
//
// We'll only keep queries where query_type == 2 or 3. For each query we store:
//   - the 100-dim query vector
//   - the [l, r] range
//   - the user also needs 'k'? In code 1, the query file had the dimension appended at the end 
//     (which was "100"), then user typed 'k' after that. 
//     But there's no 'k' in the binary format from code 2.
//
// **Assumption**: If you need a top-k, we can fix k or parse it from somewhere. 
// For demonstration, let's fix k=100 or read it from the command line, 
// since the original code 2 did not store 'k' in the binary. 
//
// Alternatively, if your .bin queries have been extended to have 'k', 
// you'd have 105 floats, etc. 
// For now, let's emulate code 1's approach: we have a "k" input param to the program or a default.
struct Query {
    std::vector<float> queryVec;
    float a_min;
    float a_max;
};

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

/********************************************************************/
/***                     LOGIC FROM CODE 1                        ***/
/********************************************************************/

// Brute force Top-K within a given subset of indices
std::vector<std::pair<float, size_t>> bruteForceTopK(
    const std::vector<DataPoint> &dataset,
    const std::vector<size_t> &subsetIndices,
    const std::vector<float> &queryVec,
    size_t k)
{
    std::vector<std::pair<float, size_t>> distID;
    distID.reserve(subsetIndices.size());

    for (auto idx : subsetIndices) {
        float dist = euclideanDistance(dataset[idx].coords, queryVec);
        distID.push_back({dist, idx});
    }
    if (k > distID.size()) k = distID.size();

    std::partial_sort(distID.begin(), distID.begin() + k, distID.end(),
        [](auto &a, auto &b) { return a.first < b.first; }
    );
    distID.resize(k);
    return distID;
}

// Brute force Top-K on the ENTIRE dataset
std::vector<std::pair<float, size_t>> bruteForceTopKEntire(
    const std::vector<DataPoint> &dataset,
    const std::vector<float> &queryVec,
    size_t k)
{
    std::vector<size_t> allIndices(dataset.size());
    for (size_t i = 0; i < dataset.size(); i++) {
        allIndices[i] = i;
    }
    return bruteForceTopK(dataset, allIndices, queryVec, k);
}

// Compute Precision@k = (# of correct hits) / (k in approx)
double computePrecisionK(const std::vector<size_t> &approx, const std::vector<size_t> &gt) {
    std::unordered_set<size_t> gtSet(gt.begin(), gt.end());
    size_t correct = 0;
    for (auto &idx: approx){
        if(gtSet.find(idx) != gtSet.end()) correct++;
    }
    return approx.empty() ? 1.0 : (double)correct / (double)approx.size();
}

// Compute Recall@k = (# of correct hits) / (k in ground truth)
double computeRecallK(const std::vector<size_t> &approx, const std::vector<size_t> &gt) {
    std::unordered_set<size_t> approxSet(approx.begin(), approx.end());
    size_t correct = 0;
    for (auto &idx: gt){
        if(approxSet.find(idx) != approxSet.end()) correct++;
    }
    return gt.empty() ? 1.0 : (double)correct / (double)gt.size();
}

// A simple Accuracy metric = intersection_size / k
double computeAccuracy(const std::vector<size_t> &approx, const std::vector<size_t> &gt) {
    std::unordered_set<size_t> gtSet(gt.begin(), gt.end());
    size_t correct = 0;
    for (auto &idx: approx){
        if(gtSet.find(idx) != gtSet.end()) correct++;
    }
    return approx.empty() ? 1.0 : (double)correct / (double)approx.size();
}

/********************************************************************/
/***                           MAIN                               ***/
/********************************************************************/
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <dataset_bin_file> <queries_bin_file> [k]\n";
        return 1;
    }
    std::string datasetBinFile = argv[1];
    std::string queriesBinFile = argv[2];

    // If there's a third arg, interpret it as k; else default k=100
    size_t K = 100;
    if (argc >= 4) {
        K = std::stoul(argv[3]);
    }

    // 1. Load dataset directly from .bin
    std::vector<DataPoint> dataset;
    size_t d;
    std::cout << "Loading dataset from " << datasetBinFile << " (binary file)\n";
    if (!loadDatasetFromBin(datasetBinFile, dataset, d)) {
        std::cerr << "Failed to load dataset from .bin\n";
        return 1;
    }
    size_t N = dataset.size();
    std::cout << "Dataset has " << N << " points in dimension " << d << "\n";

    // 2. Build the B+‐tree. Also fill our attributeMap
    double degree = 500; // arbitrary B+‐tree degree (fanout)
    BPlusTree bpt(degree);

    auto startBPT = std::chrono::steady_clock::now();
    for (size_t i = 0; i < N; i++) {
        double attr = dataset[i].attribute;  // treat timestamp as the 'attribute'
        bpt.insert(attr);
        attributeMap[attr].push_back(i);
    }
    auto endBPT = std::chrono::steady_clock::now();
    double buildTimeBPT_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endBPT - startBPT).count();
    std::cout << "B+ tree built on 'attribute' (timestamp) for " << N << " records.\n";

    // 3. Build global HNSW index on the entire dataset
    std::cout << "Building global HNSW index for " << N << " points...\n";
    auto startHNSW = std::chrono::steady_clock::now();
    HNSWWrapper globalHNSW(d, N);
    for (size_t i = 0; i < N; i++) {
        globalHNSW.addPoint(dataset[i].coords.data(), i);
    }
    auto endHNSW = std::chrono::steady_clock::now();
    double buildTimeHNSW_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endHNSW - startHNSW).count();

    // We'll store all benchmark results in a .txt file
    std::ofstream resultFile("benchmark_results.txt");
    if(!resultFile.is_open()){
        std::cerr << "Could not open benchmark_results.txt for writing.\n";
        return 1;
    }

    // Write build times
    resultFile << "=== Build Times (milliseconds) ===\n";
    resultFile << "B+Tree Build Time: " << buildTimeBPT_ms << " ms\n";
    resultFile << "HNSW Build Time  : " << buildTimeHNSW_ms << " ms\n\n";

    // 4. Load queries from .bin
    std::vector<Query> queries;
    if(!loadQueriesFromBin(queriesBinFile, queries)) {
        std::cerr << "Failed to load queries from .bin\n";
        return 1;
    }
    size_t Q = queries.size();
    std::cout << "Loaded " << Q << " queries (where query_type == 2 or 3) from " << queriesBinFile << "\n";

    // We'll store per‐query metrics in these
    std::vector<double> allPrec, allRecall, allAcc;
    allPrec.reserve(Q);
    allRecall.reserve(Q);
    allAcc.reserve(Q);

    // 5. For each query, do B+‐tree range search and then either brute force or HNSW
    for (size_t qIdx = 0; qIdx < Q; qIdx++) {
        cout << "Query Number :" << qIdx << "\n";
        const auto &q = queries[qIdx];
        const std::vector<float> &queryVec = q.queryVec;
        float a_min = q.a_min;
        float a_max = q.a_max;

        // For demonstration, we'll use the same K for every query
        size_t k = K;

        // 1) B+‐tree range search
        auto startQuery = std::chrono::steady_clock::now();
        auto [count, attrVals] = bpt.rangeSearch(a_min, a_max);

        // Convert attributes -> candidate IDs
        std::vector<size_t> candidateIDs;
        candidateIDs.reserve(count);
        for (double val : attrVals) {
            auto &inds = attributeMap[val];
            candidateIDs.insert(candidateIDs.end(), inds.begin(), inds.end());
        }

        std::vector<size_t> approxIndices;

        const size_t threshold = 20;
        if (candidateIDs.size() <= threshold) {
            // *Brute force* approach on the candidateIDs
            // Ground truth is also from candidateIDs
            auto gtBrute = bruteForceTopK(dataset, candidateIDs, queryVec, k);
            std::vector<size_t> gtIndices; 
            gtIndices.reserve(gtBrute.size());
            for (auto &p: gtBrute) {
                gtIndices.push_back(p.second);
            }

            // The approximate result = the same brute force
            auto approxTopK = bruteForceTopK(dataset, candidateIDs, queryVec, k);
            for (auto &p: approxTopK) {
                approxIndices.push_back(p.second);
            }

            auto endQuery = std::chrono::steady_clock::now();
            double queryTime_ms = std::chrono::duration_cast<std::chrono::microseconds>(endQuery - startQuery).count() / 1000.0;

            // Evaluate metrics
            double precK = computePrecisionK(approxIndices, gtIndices);
            double recallK = computeRecallK(approxIndices, gtIndices);
            double acc    = computeAccuracy(approxIndices, gtIndices);

            allPrec.push_back(precK);
            allRecall.push_back(recallK);
            allAcc.push_back(acc);

            resultFile << "Query #" << (qIdx+1) << " (Brute Force scenario):\n";
            resultFile << "  CandidateIDs.size() = " << candidateIDs.size() << "\n";
            resultFile << "  QueryTime (ms) = " << queryTime_ms << "\n";
            resultFile << "  Precision@k = " << precK << "\n";
            resultFile << "  Recall@k = " << recallK << "\n";
            resultFile << "  Accuracy = " << acc << "\n";
            resultFile << "-----------------------------------\n";
        } else {
            // *HNSW* approach on the entire dataset
            // Ground truth is top‐k from the ENTIRE dataset
            auto gtTopK = bruteForceTopKEntire(dataset, queryVec, k);
            std::vector<size_t> gtIndices; 
            gtIndices.reserve(gtTopK.size());
            for(auto &p: gtTopK) {
                gtIndices.push_back(p.second);
            }

            // Approx retrieval using global HNSW
            auto topk = globalHNSW.searchKnn(queryVec.data(), k, /*efSearch=*/100);
            auto endQuery = std::chrono::steady_clock::now();
            double queryTime_ms = std::chrono::duration_cast<std::chrono::microseconds>(endQuery - startQuery).count() / 1000.0;

            std::vector<size_t> hnswIndices; 
            hnswIndices.reserve(topk.size());
            for (auto &r : topk) {
                hnswIndices.push_back(r.second);
            }
            approxIndices = hnswIndices;

            // Evaluate metrics vs entire‐dataset ground truth
            double precK = computePrecisionK(approxIndices, gtIndices);
            double recallK = computeRecallK(approxIndices, gtIndices);
            double acc    = computeAccuracy(approxIndices, gtIndices);

            allPrec.push_back(precK);
            allRecall.push_back(recallK);
            allAcc.push_back(acc);

            resultFile << "Query #" << (qIdx+1) << " (HNSW scenario):\n";
            resultFile << "  CandidateIDs.size() = " << candidateIDs.size() << "\n";
            resultFile << "  QueryTime (ms) = " << queryTime_ms << "\n";
            resultFile << "  Precision@k = " << precK << "\n";
            resultFile << "  Recall@k = " << recallK << "\n";
            resultFile << "  Accuracy = " << acc << "\n";
            resultFile << "-----------------------------------\n";
        }
    }

    // Summaries
    if (!allPrec.empty()) {
        double avgPrec = 0.0, avgRecall = 0.0, avgAcc = 0.0;
        for (size_t i = 0; i < allPrec.size(); i++){
            avgPrec   += allPrec[i];
            avgRecall += allRecall[i];
            avgAcc    += allAcc[i];
        }
        avgPrec   /= (double) allPrec.size();
        avgRecall /= (double) allRecall.size();
        avgAcc    /= (double) allAcc.size();

        resultFile << "\n=== Aggregate Metrics over " << allPrec.size() << " queries ===\n";
        resultFile << "Average Precision@k : " << avgPrec << "\n";
        resultFile << "Average Recall@k    : " << avgRecall << "\n";
        resultFile << "Average Accuracy    : " << avgAcc << "\n";
    }

    resultFile.close();
    std::cout << "Done. Results written to benchmark_results.txt\n";
    return 0;
}
