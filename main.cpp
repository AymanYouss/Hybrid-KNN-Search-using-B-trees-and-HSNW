#include "dataset.h"       // DataPoint struct, loadDataset(), euclideanDistance
#include "hnsw_wrapper.h"  // Our HNSWlib wrapper
#include "bplustreecpp.h"  // Your B+‐tree code with insert(value) and rangeSearch()
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

static unordered_map<double, vector<size_t>> attributeMap;

int main(int argc, char** argv) {

    if (argc < 2) {
        cout << "Returning 1 because of file mismatch\n";
        cerr << "Usage: " << argv[0] << " <dataset_file> [queries_file]\n";
        return 1;
    }
    string datasetFile = argv[1];

    // 1. Load dataset
    vector<DataPoint> dataset;
    size_t d;
    cout << "Loading dataset from " << datasetFile << "\n";
    if (!loadDataset(datasetFile, dataset, d)) {
        cout << "Returning 1 because of loadDataset\n";
        cerr << "Failed to load dataset.\n";    
        return 1;
    }
    size_t N = dataset.size();
    cout << "Dataset has " << N << " points in dimension " << d << "\n";

    // 2. Build the B+‐tree (the constructor expects a double "degree"/fanout)
    //    Also fill our attributeMap
    double degree = 4; // You can pick any suitable "degree" (fanout)
    BPlusTree bpt(degree);

    for (size_t i = 0; i < N; i++) {
        double attr = dataset[i].attribute;
        bpt.insert(attr);  // Insert the attribute as the key
        attributeMap[attr].push_back(i); 
    }
    cout << "B+ tree built on 'attribute' for " << N << " records.\n";

    // If we have a queries_file as second argument, load queries from that
    // otherwise do interactive queries
    if (argc >= 3) {
        // File-based queries
        ifstream qin(argv[2]);
        if(!qin.is_open()){
            cout << "Returning 1 because of not opening the file\n";
            cerr << "Could not open queries file: " << argv[2] << "\n";
            return 1;
        }
        size_t Q;
        qin >> Q; // number of queries
        cout << "Read " << Q << " queries from file.\n";

        while (Q--) {
            vector<float> queryVec(d);
            for(size_t j = 0; j < d; j++){
                qin >> queryVec[j];
            }
            float a_min, a_max;
            qin >> a_min >> a_max;
            size_t k;
            qin >> k;

            if(!qin) {
                cerr << "Error reading query\n";
                break;
            }

            // 3. B+‐tree range search
            auto [count, attrVals] = bpt.rangeSearch(a_min, a_max); 
            // rangeSearch returns a pair of (#results, vector<double>)

            // Convert attributes -> candidate IDs 
            vector<size_t> candidateIDs;
            candidateIDs.reserve(count);
            for (double val : attrVals) {
                auto &inds = attributeMap[val];
                // gather them
                candidateIDs.insert(candidateIDs.end(), inds.begin(), inds.end());
            }

            // 4. If candidate set is small, do brute force; if large, HNSW
            const size_t threshold = 4;
            if(candidateIDs.size() <= threshold) {
                // Brute force
                vector<pair<float,size_t>> distID;
                distID.reserve(candidateIDs.size());
                for(auto cid : candidateIDs) {
                    float dist = euclideanDistance(dataset[cid].coords, queryVec);
                    distID.push_back({dist, cid});
                }
                if(k > distID.size()) k = distID.size();
                std::partial_sort(distID.begin(), distID.begin()+k, distID.end(),
                                  [](auto &a, auto &b){return a.first < b.first;});
                distID.resize(k);

                cout << "Query result (brute force):\n";
                for(auto &res : distID){
                    cout << "  idx=" << res.second << " dist=" << res.first << "\n";
                }
            } else {
                // Build an HNSW index on candidate set
                cout << "Building HNSW index on " << candidateIDs.size() << " candidates...\n";
                HNSWWrapper hnsw(d, candidateIDs.size()+1);
                for(auto cid : candidateIDs) {
                    hnsw.addPoint(dataset[cid].coords.data(), cid);
                }

                auto topk = hnsw.searchKnn(queryVec.data(), k, /*efSearch=*/100);
                cout << "Query result (HNSW):\n";
                for (auto &r : topk) {
                    cout << "  idx=" << r.second << " dist=" << sqrt(r.first) << "\n";
                }
            }

            cout << "------\n";
        }
        qin.close();
    } else{
        cout << "no queries file provided, interactive mode\n";
    }


    cout << "Done.\n";

    return 0;
}
