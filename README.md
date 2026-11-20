# Geometry Retrieval - INFOMR

> **Geometry Retrieval** is an application built for the **Multimedia Retrieval (INFOMR)** course at **Utrecht University**.

![C++23](https://img.shields.io/badge/c%2B%2B-%2300599C.svg?style=for-the-badge&logo=cplusplus&logoColor=white)
![Qt](https://img.shields.io/badge/Qt-%23217346.svg?style=for-the-badge&logo=Qt&logoColor=white)
![OpenGL](https://img.shields.io/badge/opengl-%235586A4.svg?style=for-the-badge&logo=opengl&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake&logoColor=white)


## Overview

The Geometry retrieval application lets you:
- Load and cache the [INFOMR](https://github.com/MaxRee94/ShapeDatabase_INFOMR) shape database
- **Browse** the shapes in that database per class
- Inspect a single mesh in a **Workshop** viewport, supporting many operators to build a **normalization pipeline**, and options to visually inspect mesh properties.
- Compute various scalar and histogram **shape descriptors**
- **Query** using a mesh to find similar meshes in the database, using brute-force or KD-tree-accelerated k-NN.
- Use a **Statistics** page to visualizes and compare descriptors from pipeline runs.
- Run a CLI `--evaluate` mode can be used that produces precision/recall/F1/AUROC metrics and ROC-curve CSVs.

---

## Usage

### 1. Build the application

Most dependencies are included as submodules. Only **Qt** needs to be installed manually, make sure that it is installed correctly and passed along to the cmake command.
A compiler with **C++23** support is also required.

```bash
git clone --recurse-submodules https://github.com/dafitius/INFOMR.git
cd INFOMR

# If you cloned without recursive-submodules use
git submodule update --init --recursive

# Build the cmake environment
cmake -B build -S . -DCMAKE_PREFIX_PATH=/path/to/Qt/.../lib/cmake

# Build the application
cmake --build build --config Release

```

### 2. Run the application
To use the application the following commands can be used:
```bash

# Main application
./build/MultimediaRetrieval

# (Re)generate the cached database (optional)
./build/generate_database

# evaluation mode
./build/MultimediaRetrieval --evaluate <query_count> <k> <weights as csv> <mode 0-5> <out_file>
# example usage: --evaluate 0 0 "0.5,0.3,0.6" 0 roc.csv   (0 query_count/k = use all classes)
```

## Additional tools
`.clang-format` / `.clang-tidy`:
Used to ensure that the codebase uses consistent formatting and style
```bash
clang-format -i $(find src tools -name '*.cpp' -o -name '*.h' -o -name '*.hpp')
clang-tidy src/**/*.cpp -p build
```

`tools/plot_roc.py` / `plot_descriptor_histograms.py`:
Used to visualize the CSVs produced by --evaluate and the Statistics page's "export descriptor histograms" button.