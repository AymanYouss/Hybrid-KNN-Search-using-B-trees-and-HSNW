# Hybrid B+ Tree and HNSW-based k-Nearest Neighbors Retrieval System

This repository implements a hybrid query pipeline that combines:

1. **B+ Tree** – For range filtering on a continuous attribute (e.g., timestamps).
2. **Hierarchical Navigable Small World (HNSW)** – For approximate k-nearest neighbor (ANN) search in high-dimensional space.

## System Overview
The system is designed to:
- Filter incoming queries based on a timestamp range using a B+ tree.
- Switch to HNSW-based approximate search for large candidate sets, or perform brute-force nearest-neighbor checks for smaller sets.

---

## 📂 Repository Structure

```
|-- bplustreecpp.h / bplustreecpp.cpp
|   -- B+ tree implementation (insertion, splitting, search, range search, etc.)
|
|-- dataset.h / dataset.cpp
|   -- Load binary dataset and query files.
|   -- Structures: DataPoint, Query, and corresponding read/parse functions.
|
|-- hnsw_wrapper.h / hnsw_wrapper.cpp
|   -- Interface to HNSWlib or internal HNSW code.
|   -- Functions to build, add points, and perform kNN searches with HNSW.
|
|-- main.cpp
|   -- Entry point for the hybrid system.
|   -- Builds the B+ tree and HNSW index, processes queries.
|
|-- other folders/
|   
```

---

## 🛠️ Building and Running

### Prerequisites
- **C++20** compiler (e.g., g++ version 10+).
- 
### Compilation
```bash
g++ -std=c++20 main.cpp dataset.cpp hnsw_wrapper.cpp bplustreecpp.cpp -o hybrid_knn
```

### Usage
Run the generated executable by passing the dataset and query binary files as arguments:
```bash
./hybrid_knn ./data/sigmod/dummy-data.bin ./data/sigmod/dummy-queries.bin
```
Optionally, specify K (number of nearest neighbors):
```bash
./hybrid_knn ./data/sigmod/dummy-data.bin ./data/sigmod/dummy-queries.bin
```

### Output
The program:
1. Loads the dataset into memory.
2. Builds the B+ tree for timestamp filtering.
3. Builds the HNSW index for approximate nearest neighbor search.
4. Processes queries and selects brute force or HNSW dynamically.
5. Outputs performance metrics and results to files (e.g., `bnch.txt`).

---

## 📁 Datasets
### Provided Datasets
- `dummy-data.bin` and `dummy-queries.bin` – Small test set.

### Binary File Format
- **First 4 bytes** – Number of vectors (dataset) or queries (queries).
- **Dataset records** – 102 floats (1 categorical, 1 timestamp, 100-dim vector).
- **Query records** – 104 floats (query type, category, timestamp range, 100-dim query vector).

---

## 🤝 Contributing
- Report issues or suggest enhancements through GitHub pull requests or issues.

---


