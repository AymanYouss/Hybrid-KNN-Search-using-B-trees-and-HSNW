Hybrid B+ Tree and HNSW-based k-Nearest Neighbors Retrieval System

This repository implements a hybrid query pipeline that combines:
1) B+ tree for range filtering on a continuous attribute (e.g., timestamps).
2) Hierarchical Navigable Small World (HNSW) graphs for approximate k-nearest neighbor (ANN) search in high-dimensional space.

The system is designed to:
- Filter an incoming query based on a timestamp range using a B+ tree.
- If the candidate set is large, switch to HNSW-based approximate search; if small, do a brute-force nearest-neighbor check.

==================================================
Repository Structure
==================================================
bplustreecpp.h / bplustreecpp.cpp
    B+ tree implementation (insertion, splitting, search, range search, etc.)

dataset.h / dataset.cpp
    Code to load binary dataset files and query files.
    Structures: DataPoint, Query, and corresponding read/parse functions.

hnsw_wrapper.h / hnsw_wrapper.cpp
    Interface layer to HNSWlib (or an internal HNSW code).
    Functions to build, add points, and perform kNN searches with HNSW.

main.cpp
    Entry point for the hybrid system.
    Builds the B+ tree, builds the HNSW index, and processes queries.
    Contains the logic for hybrid switching between brute force vs. HNSW based on candidate set size.

data/
    sigmod/
        dummy-data.bin
        dummy-queries.bin
        (other dataset/query files)
    (Any additional data or query files)

README.md
    (This file)

(Optional) docs/
    Contains LaTeX files for the report, figures, etc.

==================================================
Building and Running
==================================================
Prerequisites:
- C++20 compiler (e.g., g++ version 10+).
- CMake (optional, if you prefer a CMake build system).
- For HNSW, either a local copy of HNSWlib or the built-in HNSW code in hnsw_wrapper.cpp.

Compilation (simple command-line example):
g++ -std=c++20 main.cpp dataset.cpp hnsw_wrapper.cpp bplustreecpp.cpp -o hybrid_knn

Usage:
Run the generated executable (hybrid_knn) by passing the dataset and query binary files as arguments, for example:

./hybrid_knn ./data/sigmod/dummy-data.bin ./data/sigmod/dummy-queries.bin

Optionally, you can specify K (the number of nearest neighbors) as a third argument:
./hybrid_knn ./data/sigmod/dummy-data.bin ./data/sigmod/dummy-queries.bin 100

Output:
After running, the program:
1) Loads the dataset into memory.
2) Builds the B+ tree (for timestamp filtering).
3) Builds the HNSW index (for approximate NN).
4) Processes each query, deciding whether to use brute force or HNSW.
5) Outputs performance metrics and query results to a file such as bnch.txt, or multiple files if you’re sweeping parameters.

==================================================
Parameter Sweeping
==================================================
Within main.cpp (or a specialized file), you can adjust:
- efSearch for HNSW accuracy/speed trade-off.
- threshold to decide when to switch from brute force to HNSW.
- B+ tree order (degree = 2 * order) for controlling leaf and internal node capacities.

In the LaTeX report, there is a code snippet demonstrating how to systematically sweep over these parameters (efSearch in {100, 200}, threshold in {100, 500, 1000}, order in {200, 300}) and store results in separate output files.

==================================================
Datasets
==================================================
We have provided two example datasets:
- dummy-data.bin and dummy-queries.bin (small test).
- (Optional) contest-data-release-1m.bin and contest-queries-release-1m.bin (larger dataset).

Binary file format:
- First 4 bytes: number of vectors (for dataset) or queries (for queries).
- For dataset: Each record has 102 floats (1 categorical, 1 timestamp, 100-dim vector).
- For queries: Each record has 104 floats (query type, category, timestamp range, 100-dim query vector).

==================================================
Contributing
==================================================
Please file issues or pull requests on GitHub if you find bugs or wish to suggest enhancements.

==================================================
License
==================================================
This project is released under the MIT License (or whichever license you choose). See LICENSE for details.

==================================================
Acknowledgments
==================================================
- SIGMOD 2024 Programming Competition for providing the original dataset format and partial code structure for reading binary files.
- HNSWlib for the ANN algorithms and original implementations.
- Prof. Karima Echihabi and Anas Ait Aomar for supervision and guidance.

For a deeper technical description, usage examples, and performance results, please refer to the accompanying LaTeX report (docs/ folder) or the PDF generated from it.

Enjoy using the hybrid B+ Tree + HNSW retrieval system! Contributions and feedback are welcome.
