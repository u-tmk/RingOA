# RingOA

RingOA implements a three-party oblivious access protocol based on distributed point functions, enabling efficient access to large-scale data, with applications to privacy-preserving full-text search and range search.

This repository contains the reference implementation of the following paper:

> Tomoki Uchiyama and Kana Shimizu. RingOA: Fast Oblivious Access for Large-Scale Privacy-Preserving Structured Data Analysis. PETS 2026.

---

## Artifact Evaluation

For artifact evaluation instructions, see [ARTIFACT-APPENDIX.md](artifact/ARTIFACT-APPENDIX.md).

---

## Building the Project

Before building, install [SDSL](https://github.com/simongog/sdsl-lite), which is required for running the application. Make sure it is installed and visible to CMake (e.g., via system install or `CMAKE_PREFIX_PATH`).

The project can be built using the provided `build.py` script. The build has been tested on Linux (Ubuntu 20.04 and 22.04). CMake 3.18+ and Python 3.6+ are required. The `--setup` option is required only for the first build to set up dependencies, and build outputs are generated under `out/build/linux/`.

```bash
git clone https://github.com/u-tmk/RingOA
cd RingOA

# Install dependencies (cryptoTools)
python build.py --setup

# Build the project
python build.py
```

## Usage

### Testing the RingOA Protocol

The executable `test_RingOA` provides options for running unit tests of the RingOA protocol.

To run all tests in a single terminal:

```bash
./out/build/linux/bin/test_RingOA -u
```

To list available tests:

```bash
./out/build/linux/bin/test_RingOA -l
```

To run a specific test:

```bash
./out/build/linux/bin/test_RingOA -t <test_index>
```

By default, all parties are executed within a single process. For multi-process execution, use the `-party` option and run one terminal per party. (Use `-party 0` and `-party 1` for two-party, and `-party 0`, `-party 1`, and `-party 2` for three-party settings.)


For example, in a three-party setting:

```bash
# Terminal 1
./out/build/linux/bin/test_RingOA -t <test_index> -party 0
# Terminal 2
./out/build/linux/bin/test_RingOA -t <test_index> -party 1
# Terminal 3
./out/build/linux/bin/test_RingOA -t <test_index> -party 2
```

### Benchmarking the RingOA Protocol

The executable `bench_RingOA` provides several options for evaluating the performance of the RingOA protocol.

To list available benchmarks:

```bash
./out/build/linux/bin/bench_RingOA -l
```

To run a specific benchmark:

```bash
./out/build/linux/bin/bench_RingOA -b <benchmark_index>
```

To show the help message for a specific benchmark:

```bash
./out/build/linux/bin/bench_RingOA -b <benchmark_index> -h
```

#### Database size specification

Database sizes are specified using the `-db_bits` option, or its short form `-d`.

The value passed to `-db_bits` represents the bit size `n` of the database, where the database contains `2^n` elements.

The following formats are supported:

```bash
-db_bits 20
-db_bits 20,22,24
-db_bits 20:24
-db_bits 20:24:2
```

The meaning of each format is:

* `20`: run the benchmark with `n = 20`
* `20,22,24`: run the benchmark with `n = 20, 22, 24`
* `20:24`: run the benchmark with all values from `20` to `24`
* `20:24:2`: run the benchmark with values from `20` to `24` using step size `2`

Ranges are inclusive. For example, `20:24:2` expands to `20, 22, 24`.

#### Common benchmark options

The number of measured repetitions can be specified using the `-repeat` option. The default value is `10`.

```bash
./out/build/linux/bin/bench_RingOA -b <benchmark_index> -d 20 -repeat 10
```

The number of warmup repetitions can be specified using the `-warmup` option. The default value is `5`.

```bash
./out/build/linux/bin/bench_RingOA -b <benchmark_index> -d 20 -repeat 10 -warmup 5
```

Multi-process execution can be enabled using the `-party` option, running one terminal per party. This mode is recommended for more stable benchmarking results, especially for preprocessing and online phases.

The `-network` option can be used to attach a network label to output log filenames.

The `-no_log_timestamp` option disables appending timestamps to log filenames.

#### Example: Benchmarking RingOA

In this example, `-b 0` generates input data, `-b 3` runs the preprocessing benchmark, and `-b 4` runs the online benchmark.

All phases must use the same database size specification.

Generate input data:

```bash
./out/build/linux/bin/bench_RingOA -b 0 -d 20:24:2
```

Run the preprocessing phase with one terminal per party:

```bash
# Terminal 1
./out/build/linux/bin/bench_RingOA -b 3 -d 20:24:2 -party 0

# Terminal 2
./out/build/linux/bin/bench_RingOA -b 3 -d 20:24:2 -party 1

# Terminal 3
./out/build/linux/bin/bench_RingOA -b 3 -d 20:24:2 -party 2
```

Run the online phase with one terminal per party:

```bash
# Terminal 1
./out/build/linux/bin/bench_RingOA -b 4 -d 20:24:2 -party 0

# Terminal 2
./out/build/linux/bin/bench_RingOA -b 4 -d 20:24:2 -party 1

# Terminal 3
./out/build/linux/bin/bench_RingOA -b 4 -d 20:24:2 -party 2
```

After running the benchmark, results are printed to standard output and written to the configured log directory.

To extract the main evaluation results, filter the output using `grep`. This shows the averaged execution time of the main evaluation routine. For example:

```bash
./out/build/linux/bin/bench_RingOA -b 4 -d 20:24:2 -party 0 | grep "Summary.*RingOA::Eval"
```

---

## BibTeX

```bibtex
@inproceedings{RingOA,
  title   = {RingOA: Fast Oblivious Access for Large-Scale Privacy-Preserving Structured Data Analysis},
  author  = {Tomoki Uchiyama and Kana Shimizu},
  journal = {Proceedings on Privacy Enhancing Technologies},
  volume  = {2026},
  number  = {},
  pages   = {},
  year    = {2026}
  doi     = {}
}
```
