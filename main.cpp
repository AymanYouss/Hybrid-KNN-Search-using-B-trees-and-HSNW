#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <thread>          // <--- For multithreading
#include <mutex>           // <--- Potentially for shared data protection
#include "hnsw_wrapper.h"  // HNSWWrapper class
#include "bplustreecpp.h"  // BPlusTree class
#include "dataset.h"

static std::unordered_map<double, std::vector<size_t>> attributeMap;

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

double computePrecisionK(const std::vector<size_t> &approx, const std::vector<size_t> &gt) {
    std::unordered_set<size_t> gtSet(gt.begin(), gt.end());
    size_t correct = 0;
    for (auto &idx: approx){
        if(gtSet.find(idx) != gtSet.end()) correct++;
    }
    return approx.empty() ? 1.0 : (double)correct / (double)approx.size();
}

double computeRecallK(const std::vector<size_t> &approx, const std::vector<size_t> &gt) {
    std::unordered_set<size_t> approxSet(approx.begin(), approx.end());
    size_t correct = 0;
    for (auto &idx: gt){
        if(approxSet.find(idx) != approxSet.end()) correct++;
    }
    return gt.empty() ? 1.0 : (double)correct / (double)gt.size();
}

double computeAccuracy(const std::vector<size_t> &approx, const std::vector<size_t> &gt) {
    std::unordered_set<size_t> gtSet(gt.begin(), gt.end());
    size_t correct = 0;
    for (auto &idx: approx){
        if(gtSet.find(idx) != gtSet.end()) correct++;
    }
    return approx.empty() ? 1.0 : (double)correct / (double)approx.size();
}

// A struct to hold each query's results
struct QueryResult {
    double queryTimeMs = 0.0;
    double precisionK  = 0.0;
    double recallK     = 0.0;
    double accuracy    = 0.0;
    double filteredPrecisionK = -1.0;  // only relevant for HNSW scenario
    double filteredRecallK    = -1.0;
    double filteredAccuracy   = -1.0;
    size_t candidateSize = 0;
    bool usedHNSW = false;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <dataset_bin_file> <queries_bin_file> [k]\n";
        return 1;
    }
    std::string datasetBinFile = argv[1];
    std::string queriesBinFile = argv[2];

    size_t K = 100;
    if (argc >= 4) {
        K = std::stoul(argv[3]);
    }

    // 1. Load dataset
    std::vector<DataPoint> dataset;
    size_t d;
    std::cout << "Loading dataset from " << datasetBinFile << "\n";
    if (!loadDatasetFromBin(datasetBinFile, dataset, d)) {
        std::cerr << "Failed to load dataset from .bin\n";
        return 1;
    }
    size_t N = dataset.size();
    std::cout << "Dataset has " << N << " points in dimension " << d << "\n";

    // 2. Build B+‐tree & fill attributeMap
    double degree = 500; 
    BPlusTree bpt(degree);

    auto startBPT = std::chrono::steady_clock::now();
    for (size_t i = 0; i < N; i++) {
        double attr = dataset[i].attribute; 
        bpt.insert(attr);
        attributeMap[attr].push_back(i);
    }
    auto endBPT = std::chrono::steady_clock::now();
    double buildTimeBPT_ms = 
        std::chrono::duration_cast<std::chrono::milliseconds>(endBPT - startBPT).count();
    std::cout << "B+ tree built.\n";

    // 3. Build global HNSW
    std::cout << "Building global HNSW index...\n";
    auto startHNSW = std::chrono::steady_clock::now();
    HNSWWrapper globalHNSW(d, N);
    for (size_t i = 0; i < N; i++) {
        globalHNSW.addPoint(dataset[i].coords.data(), i);
    }
    auto endHNSW = std::chrono::steady_clock::now();
    double buildTimeHNSW_ms = 
        std::chrono::duration_cast<std::chrono::milliseconds>(endHNSW - startHNSW).count();

    // 4. Prepare output file
    std::ofstream resultFile("benchmark.txt");
    if(!resultFile.is_open()){
        std::cerr << "Could not open benchmark.txt for writing.\n";
        return 1;
    }
    resultFile << "=== Build Times (milliseconds) ===\n";
    resultFile << "B+Tree Build Time: " << buildTimeBPT_ms << " ms\n";
    resultFile << "HNSW Build Time  : " << buildTimeHNSW_ms << " ms\n\n";

    // 5. Load queries
    std::vector<Query> queries;
    if(!loadQueriesFromBin(queriesBinFile, queries)) {
        std::cerr << "Failed to load queries from .bin\n";
        return 1;
    }
    size_t Q = queries.size();
    std::cout << "Loaded " << Q << " queries.\n";

    // This will store per‐query results
    std::vector<QueryResult> allResults(Q);

    // ----------------- NEW: Start the total query timer ----------------- 
    auto startAllQueries = std::chrono::steady_clock::now();
    // --------------------------------------------------------------------

    // 6. For each query, we spawn a thread:
    std::vector<std::thread> threads;
    threads.reserve(Q);

    for (size_t qIdx = 0; qIdx < Q; qIdx++) {
        threads.emplace_back([&, qIdx]() {
            // Everything inside here runs in parallel for each query
            const auto &q = queries[qIdx];
            const std::vector<float> &queryVec = q.queryVec;
            float a_min = q.a_min;
            float a_max = q.a_max;

            // 1) B+‐tree range search
            auto startQuery = std::chrono::steady_clock::now();
            auto [count, attrVals] = bpt.rangeSearch(a_min, a_max);

            std::vector<size_t> candidateIDs;
            candidateIDs.reserve(count);
            for (double val : attrVals) {
                auto &inds = attributeMap[val];
                candidateIDs.insert(candidateIDs.end(), inds.begin(), inds.end());
            }

            std::vector<size_t> approxIndices;
            const size_t threshold = 100;
            
            if (candidateIDs.size() <= threshold) {
                // Brute force
                auto approxTopK = bruteForceTopK(dataset, candidateIDs, queryVec, K);
                for (auto &p : approxTopK) {
                    approxIndices.push_back(p.second);
                }

                auto endQuery = std::chrono::steady_clock::now();
                double queryTime_ms = 
                    std::chrono::duration_cast<std::chrono::microseconds>(endQuery - startQuery).count() / 1000.0;

                // Ground truth also from candidateIDs
                auto gtBrute = bruteForceTopK(dataset, candidateIDs, queryVec, K);
                std::vector<size_t> gtIndices;
                gtIndices.reserve(gtBrute.size());
                for (auto &p: gtBrute) {
                    gtIndices.push_back(p.second);
                }

                // Evaluate
                double precK = computePrecisionK(approxIndices, gtIndices);
                double recallK = computeRecallK(approxIndices, gtIndices);
                double acc    = computeAccuracy(approxIndices, gtIndices);

                // Store in our results array
                allResults[qIdx].queryTimeMs = queryTime_ms;
                allResults[qIdx].precisionK  = precK;
                allResults[qIdx].recallK     = recallK;
                allResults[qIdx].accuracy    = acc;
                allResults[qIdx].candidateSize = candidateIDs.size();
                allResults[qIdx].usedHNSW   = false;
            } 
            else {
                // HNSW scenario
                auto topk = globalHNSW.searchKnn(queryVec.data(), 3*K, /*efSearch=*/100);
                auto endQuery = std::chrono::steady_clock::now();
                double queryTime_ms = 
                    std::chrono::duration_cast<std::chrono::microseconds>(endQuery - startQuery).count() / 1000.0;

                // Ground truth is top‐k from entire dataset
                auto gtTopK = bruteForceTopKEntire(dataset, queryVec, K);
                std::vector<size_t> gtIndices;
                gtIndices.reserve(gtTopK.size());
                for(auto &p: gtTopK) {
                    gtIndices.push_back(p.second);
                }

                std::vector<size_t> hnswIndices; 
                hnswIndices.reserve(topk.size());
                for (auto &r : topk) {
                    hnswIndices.push_back(r.second);
                }
                approxIndices = hnswIndices;

                double precK = computePrecisionK(approxIndices, gtIndices);
                double recallK = computeRecallK(approxIndices, gtIndices);
                double acc    = computeAccuracy(approxIndices, gtIndices);

                // Filter to keep only the vectors in [a_min, a_max]
                std::vector<size_t> filteredIndices;
                filteredIndices.reserve(approxIndices.size());
                for (auto idx : approxIndices) {
                    double attr = dataset[idx].attribute;
                    if (attr >= a_min && attr <= a_max) {
                        filteredIndices.push_back(idx);
                    }
                }
                // Filter GT similarly
                std::vector<size_t> filteredGTIndices;
                for (auto idx : gtIndices) {
                    double attr = dataset[idx].attribute;
                    if (attr >= a_min && attr <= a_max) {
                        filteredGTIndices.push_back(idx);
                    }
                }
                double precK2   = computePrecisionK(filteredIndices, filteredGTIndices);
                double recallK2 = computeRecallK(filteredIndices, filteredGTIndices);
                double acc2     = computeAccuracy(filteredIndices, filteredGTIndices);

                // Store results
                allResults[qIdx].queryTimeMs        = queryTime_ms;
                allResults[qIdx].precisionK         = precK;
                allResults[qIdx].recallK            = recallK;
                allResults[qIdx].accuracy           = acc;
                allResults[qIdx].filteredPrecisionK = precK2;
                allResults[qIdx].filteredRecallK    = recallK2;
                allResults[qIdx].filteredAccuracy   = acc2;
                allResults[qIdx].candidateSize      = candidateIDs.size();
                allResults[qIdx].usedHNSW           = true;
            }
        });
    }

    // 7. Join all threads
    for (auto &t : threads) {
        t.join();
    }

    // ------------------ NEW: Stop the total query timer ------------------
    auto endAllQueries = std::chrono::steady_clock::now();
    double totalQueriesTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endAllQueries - startAllQueries).count();
    // ---------------------------------------------------------------------

    // 8. Now that all queries are done, write results to file (single-threaded)
    for (size_t qIdx = 0; qIdx < Q; qIdx++) {
        const auto &r = allResults[qIdx];
        if (!r.usedHNSW) {
            // Brute Force scenario
            resultFile << "Query #" << (qIdx+1) << " (Brute Force):\n";
            resultFile << "  CandidateIDs.size() = " << r.candidateSize << "\n";
            resultFile << "  QueryTime (ms)      = " << r.queryTimeMs << "\n";
            resultFile << "  Precision@K         = " << r.precisionK << "\n";
            resultFile << "  Recall@K            = " << r.recallK << "\n";
            resultFile << "  Accuracy            = " << r.accuracy << "\n";
            resultFile << "-----------------------------------\n";
        } else {
            // HNSW scenario
            resultFile << "Query #" << (qIdx+1) << " (HNSW):\n";
            resultFile << "  CandidateIDs.size() = " << r.candidateSize << "\n";
            resultFile << "  QueryTime (ms)      = " << r.queryTimeMs << "\n";
            resultFile << "  Precision@K         = " << r.precisionK << "\n";
            resultFile << "  Recall@K            = " << r.recallK << "\n";
            resultFile << "  Accuracy            = " << r.accuracy << "\n";
            resultFile << "  Filtered Precision@K= " << r.filteredPrecisionK << "\n";
            resultFile << "  Filtered Recall@K   = " << r.filteredRecallK << "\n";
            resultFile << "  Filtered Accuracy   = " << r.filteredAccuracy << "\n";
            resultFile << "-----------------------------------\n";
        }
    }

    // Compute & write aggregate metrics
    if (!allResults.empty()) {
        double sumPrec = 0.0, sumRecall = 0.0, sumAcc = 0.0;
        for (auto &r : allResults) {
            sumPrec   += r.precisionK;
            sumRecall += r.recallK;
            sumAcc    += r.accuracy;
        }
        size_t count = allResults.size();
        double avgPrec   = sumPrec   / count;
        double avgRecall = sumRecall / count;
        double avgAcc    = sumAcc    / count;

        resultFile << "\n=== Aggregate Metrics over " << count << " queries ===\n";
        resultFile << "Average Precision@K : " << avgPrec << "\n";
        resultFile << "Average Recall@K    : " << avgRecall << "\n";
        resultFile << "Average Accuracy    : " << avgAcc << "\n";
    }

    // ------------------- NEW: Print the total queries time --------------------
    resultFile << "\nTotal Queries Time (ms): " << totalQueriesTimeMs << "\n";
    // -------------------------------------------------------------------------

    resultFile.close();
    std::cout << "Done. Results written to benchmark.txt\n";
    std::cout << "Total time for all queries (ms): " << totalQueriesTimeMs << std::endl;

    return 0;
}

// #include <iostream>
// #include <fstream>
// #include "bplustreecpp.h"

// int main() {
//     BPlusTree bptree(2);

//     // Insert some sample keys to build a basic tree
//     // (You'll need to provide your own insert method calls or adapt them as appropriate.)
//     bptree.insert(10.0f);
//     bptree.insert(20.0f);
//     bptree.insert(5.0f);
//     bptree.insert(15.0f);
//     bptree.insert(25.0f);

//     // Option 1: Print DOT code to the console (stdout)
//     // bptree.generateDot(std::cout);

//     // Option 2: Print DOT code to a file
//     std::ofstream outFile("bptree.dot");
//     if (outFile.is_open()) {
//         bptree.generateDot(outFile);
//         outFile.close();
//         std::cout << "DOT file bptree.dot generated successfully.\n"
//                   << "Use 'dot -Tpng bptree.dot -o bptree.png' to create an image.\n";
//     } else {
//         std::cerr << "Failed to open bptree.dot for writing.\n";
//     }

//     return 0;
// }
