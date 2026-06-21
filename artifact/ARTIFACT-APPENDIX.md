# Artifact Appendix

Paper title: **RingOA: Fast Oblivious Access for Large-Scale Privacy-Preserving Structured Data Analysis**

Requested Badge(s):
  - [x] **Available**
  - [x] **Functional**
  - [ ] **Reproduced**

## Description

Paper: Tomoki Uchiyama and Kana Shimizu. RingOA: Fast Oblivious Access for Large-Scale Privacy-Preserving Structured Data Analysis. PETS 2026.

Artifact Description: RingOA is a C++ library that implements a three-party oblivious access protocol based on replicated secret sharing and distributed point functions. It provides implementations of the core protocol as well as applications such as fully oblivious full-text search and range queries. The artifact also includes unit tests and benchmarking tools for evaluating correctness and performance of the proposed methods.

### Security/Privacy Issues and Ethical Concerns

No security, privacy, or ethical concerns. This artifact does not include any vulnerable code, exploits, or security-disabling mechanisms, and does not use any personally identifiable information.

## Basic Requirements

### Hardware Requirements

The artifact can run on a standard desktop or laptop computer. No special hardware is required beyond an x86_64 processor with SSE2 and AES-NI support.
For the default benchmark configuration, we recommend:

- 32 GB RAM
- 60 GB of available disk space

Experiment 1 (oblivious access) can also be executed on a machine with 16 GB RAM using the default benchmark settings. Additional memory may be required for larger benchmark configurations and dataset generation.

The experiments reported in the paper were conducted on a Linux machine equipped with an AMD Ryzen Threadripper 3970X CPU (3.7 GHz) and 256 GB RAM.

### Software Requirements

#### Native Setup

- OS: Linux (tested on Ubuntu 20.04 and 22.04)
- OS Packages: Git, Make, CMake (version 3.18 or later), Python (version 3.6 or later)
- Compiler: GCC / G++ (version 11 or later, with C++20 support)

#### Docker Setup

- Container Runtime: Docker 28.1.1 (tested)
- Base Image: Ubuntu 22.04

Dependencies:

- [cryptoTools](https://github.com/ladnir/cryptoTools) (automatically installed via `build.py`)
- [SDSL](https://github.com/simongog/sdsl-lite) (must be installed separately and visible to CMake)

### Estimated Time and Storage Consumption

- Build time: ~5–10 minutes
- Test execution time: <1 minute
- Running all benchmark experiments with the default setting (in our environment): ~2 hours

Disk usage:

- Build directory (including dependencies): ~1.5 GB
- Generated benchmark data: ~60 GB

Most of the storage is used by files storing secret-shared benchmark databases.

## Environment

### Accessibility

The artifact is publicly available at: https://github.com/u-tmk/RingOA

### Native Setup

Install the required system packages:

```bash
sudo apt update
sudo apt install -y build-essential cmake libboost-all-dev libbsd-dev
```

Install [SDSL](https://github.com/simongog/sdsl-lite), which is required for running the application:

```bash
git clone https://github.com/simongog/sdsl-lite.git
cd sdsl-lite
sudo ./install.sh /usr/local/
cd ..
```

After installing SDSL, clone the repository and build the project:

```bash
git clone https://github.com/u-tmk/RingOA
cd RingOA

# Install dependencies (e.g., cryptoTools)
python build.py --setup

# Build the project
python build.py
```

The `--setup` option is required only for the first build. Build outputs are generated under `out/build/linux/`.

### Docker Setup

Build the Docker image:

```bash
./docker/build-docker
```

Start an interactive shell inside the container:

```bash
./docker/start-docker
```

The host directory `./data` is mounted to `/RingOA/data` inside the container.


### Testing the Environment

Run unit tests to verify correctness:

```bash
./out/build/linux/bin/test_RingOA -u
```

When using Docker, run:

```bash
./docker/run-unit-test
```

## Artifact Evaluation

### Evaluation Overview

This artifact provides three benchmark experiments for evaluating functionality and reproducing the main results of the paper: oblivious access, full-text search, and range search. Unless otherwise specified, the benchmark scripts use the `medium` setting by default. For oblivious access and full-text search, the `medium` setting evaluates database sizes `N = 2^20, 2^22, 2^24, 2^26`.

The benchmark scripts support reproducing the network environments used in the paper. The experiments are executed under the same latency and bandwidth settings as those used to generate the reported results.

Each benchmark prints the aggregated results to standard output at the end of execution. The results are also stored under `./data/results/<run_id>/`, where `<run_id>` identifies the timestamped result directory generated for the current benchmark run. The experiments can be executed either natively or through Docker. See [Running Benchmarks with Docker](#running-benchmarks-with-docker).

#### Dataset Note

The full-text search and range search experiments in the paper use real datasets. To simplify artifact evaluation, the default workflow uses randomly generated datasets and does not require downloading or preprocessing external data. Using random datasets does not affect the reported performance measurements. To use the same real datasets as in the paper, follow the instructions in [artifact/data-setup/README.md](data-setup/README.md) and generate datasets with the `real` option.

### Main Results and Claims

#### Main Result 1: Oblivious Access Performance

Our paper claims that `RingOA` and `RingOA-FSC` provide more efficient oblivious access than prior protocols while maintaining constant communication complexity. Figure 2 and Table 5 report preprocessing time, online time, and communication cost as the database size `N` increases. These results can be reproduced by executing [Experiment 1: Oblivious Access Benchmark](#experiment-1-oblivious-access-benchmark).

#### Main Result 2: Full Text Search Performance

Our paper claims that the RingOA based oblivious FM-index protocols, `OFMI` and `OFMI-FSC`, achieve practical full-text search performance with communication independent of the database size. Figure 4 and Table 6 report preprocessing time, online query time, and communication cost as the database size `N` increases. These results can be reproduced by executing [Experiment 2: Full Text Search Benchmark](#experiment-2-full-text-search-benchmark).

#### Main Result 3: Range Search Performance

Our paper claims that the RingOA based oblivious range query protocols, `ORQ` and `ORQ-FSC`, efficiently support range queries while hiding the query interval and the order parameter. Table 7 reports online time and communication cost. These results can be reproduced by executing [Experiment 3: Range Search Benchmark](#experiment-3-range-search-benchmark).

### Experiment 1: Oblivious Access Benchmark

* Time: 5 human minutes + 15 compute minutes
* Storage: 10GB

This experiment reproduces [Main Result 1: Oblivious Access Performance](#main-result-1-oblivious-access-performance). The measured preprocessing time, online time, and communication cost should follow the same trends reported in Figure 2 and Table 5.


Before running the benchmark, build the included DuORAM baseline implementation:

```bash
cd artifact/baseline/prac
make -j$(nproc)
cd ../../..
```

Additional information about the DuORAM baseline is available in [artifact/baseline/README.md](baseline/README.md).

Generate the benchmark datasets:

```bash
./artifact/scripts/gen_dataset.sh oa
```

Run the benchmark:

```bash
./artifact/scripts/run_oa_bench.sh
```

The aggregated results are printed to standard output at the end of the benchmark execution. The parsed results are also stored in `./data/results/<run_id>/parsed/oa_table.tsv`.


#### Larger Database Sizes

To evaluate larger database sizes, set `DB_BITS` before running both dataset generation and the benchmark. For example, the following commands evaluate `N = 2^28` and `N = 2^30`.

```bash
DB_BITS=28,30 ./artifact/scripts/gen_dataset.sh oa
DB_BITS=28,30 ./artifact/scripts/run_oa_bench.sh
```

The generated datasets occupy approximately 40 GB for `N = 2^28` and approximately 160 GB for `N = 2^30`. Dataset generation and benchmark execution times increase accordingly with the database size.


### Experiment 2: Full Text Search Benchmark

* Time: 5 human minutes + 1.5 compute hours
* Storage: 25GB

This experiment reproduces [Main Result 2: Full Text Search Performance](#main-result-2-full-text-search-performance). The measured preprocessing time, online query time, and communication cost should follow the same trends reported in Figure 4 and Table 6. All queries use a fixed pattern length of 16 characters, matching the evaluation setting used in the paper.

Generate the benchmark datasets:

```bash
./artifact/scripts/gen_dataset.sh fts
```

Run the benchmark:

```bash
./artifact/scripts/run_fts_bench.sh
```

The aggregated results are printed to standard output at the end of the benchmark execution. The parsed results are also stored in `./data/results/<run_id>/parsed/fts_table.tsv`.

The benchmark reproduces the `OFMI`, `OFMI-FSC`, and `SotFMI` entries in Figure 4 and Table 6. See the Limitations section for additional baselines not included in this artifact.

#### Larger Database Sizes

To evaluate larger database sizes, set `DB_BITS` before running both dataset generation and the benchmark. For example, the following commands evaluate `N = 2^28` and `N = 2^30`.

```bash
DB_BITS=28,30 ./artifact/scripts/gen_dataset.sh fts
DB_BITS=28,30 ./artifact/scripts/run_fts_bench.sh
```

The generated datasets occupy approximately 75 GB for `N = 2^28` and approximately 290 GB for `N = 2^30`. Dataset generation and benchmark execution times increase accordingly with the database size.

### Experiment 3: Range Search Benchmark

* Time: 5 human minutes + 5 compute minutes
* Storage: 20GB

This experiment reproduces [Main Result 3: Range Search Performance](#main-result-3-range-search-performance). The measured online time, preprocessing time, and communication cost can be compared against the results reported in Table 7 and Appendix Table 9 of the paper.

Generate the benchmark datasets:

```bash
./artifact/scripts/gen_dataset.sh range
```

Run the benchmark:

```bash
./artifact/scripts/run_range_bench.sh
```

The aggregated results are printed to standard output at the end of the benchmark execution. The parsed results are also stored in `./data/results/<run_id>/parsed/range_table.tsv`, where `<run_id>` identifies the current benchmark run.

### Running Benchmarks with Docker

The same experiments can also be executed inside the Docker container.

```bash
./docker/run-oa-bench
./docker/run-fts-bench
./docker/run-range-bench
```

These wrapper scripts generate the required datasets and then run the corresponding benchmark. Results are stored under `./data/results/<run_id>/` on the host machine. The scripts accept the same environment variables as the native benchmark scripts. For example:

```bash
DB_BITS=28,30 ./docker/run-oa-bench
```

## Limitations

The artifact includes the complete implementation of RingOA, OFMI, OFMI-FSC, and the Shared OT based baseline SotFMI.

Two baselines reported in Table 6, SecureLPM and sWM, are implemented in external codebases and are therefore not distributed as part of this artifact:

* SecureLPM: https://waseda.box.com/v/wabi2021-supplppgs-src
* sWM: https://github.com/cBioLab/sWM

Reproducing the SecureLPM and sWM results requires obtaining these implementations separately. All other results covered by the submitted artifact can be evaluated using the provided scripts.

## Notes on Reusability

The artifact is organized into separate modules for function secret sharing, secret sharing, oblivious access protocols, and application-level constructions. This modular design allows individual components to be reused or replaced independently. In addition to reproducing the results of the paper, the RingOA implementation can serve as a building block for other privacy-preserving applications. The provided oblivious FM-index and oblivious range query implementations illustrate how new applications can be constructed on top of the RingOA protocol layer.
