#include "./hnswlib/hnswlib.h"
#include <vector>
#include <string>

// A simple wrapper around HNSWlib for float vectors using L2 distance
// Usage:
//   1) Construct with dimension, maxElements, M, efConstruction
//   2) addPoint() for each vector
//   3) call finalizeConstruction()
//   4) searchKnn() for queries
//   5) optionally saveIndex() / loadIndex()
class HNSWWrapper {
public:
    // Constructor for building a new index from scratch
    HNSWWrapper(size_t dimension, size_t maxElements);

    // Add a data point with label
    // coords must be dimension floats
    void addPoint(const float* coords, size_t label);

    // Search the top K nearest neighbors
    // Returns a vector of (distance, label) sorted ascending by distance
    std::vector<std::pair<float,size_t>> searchKnn(const float* query, size_t k, int efSearch=50);

private:
    size_t dim;
    size_t maxElements;
    hnswlib::L2Space* space;
    hnswlib::HierarchicalNSW<float>* hnsw;
};
