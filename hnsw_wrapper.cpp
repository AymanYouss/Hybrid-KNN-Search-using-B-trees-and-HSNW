#include "hnsw_wrapper.h"
#include <iostream>
#include <queue> 

HNSWWrapper::HNSWWrapper(size_t dimension, size_t maxElements)
    : dim(dimension), maxElements(maxElements)
{
    space = new hnswlib::L2Space(dim);
    hnsw = new hnswlib::HierarchicalNSW<float>(space, maxElements);
}

void HNSWWrapper::addPoint(const float* coords, size_t label) {
    if(hnsw->cur_element_count >= maxElements){
        std::cerr << "Warning: index is at capacity, cannot add more points.\n";
        return;
    }
    hnsw->addPoint(coords, label);
}



std::vector<std::pair<float,size_t>> HNSWWrapper::searchKnn(const float* query, size_t k, int efSearch) {
    hnsw->ef_ = efSearch; // efSearch controls accuracy/performance tradeoff

    auto result = hnsw->searchKnn(query, k);

    // Convert the result (priority_queue) into a vector of (distance, label)
    // Note: the top of the priority_queue is the *farthest* point in the result set
    std::vector<std::pair<float,size_t>> topk;
    topk.reserve(k);

    while(!result.empty()) {
        auto &it = result.top();
        float dist = it.first;
        size_t label = it.second;
        topk.push_back({dist, label});
        result.pop();
    }
    // Now topk is in reverse order (farthest to nearest). Reverse it
    std::reverse(topk.begin(), topk.end());
    return topk;
}

