# Artifact Scripts

This directory contains the scripts used to generate datasets, configure network conditions, run the artifact benchmarks, and parse benchmark logs.

All scripts are intended to be executed from the repository root.

## Scripts

### `benchmark_common.sh`

Provides shared helper functions used by the benchmark runner scripts.

It handles:

- executable checks
- `sudo` credential initialization and refresh
- network profile validation and configuration
- cleanup of loopback traffic-control settings on exit
- execution and synchronization of the three RingOA parties

This file is sourced automatically by `run_oa_bench.sh`, `run_fts_bench.sh`, and `run_range_bench.sh`. It is not intended to be executed directly.

### `gen_dataset.sh`

Generates the datasets required by the artifact experiments.

Typical usage:

```bash
./artifact/scripts/gen_dataset.sh <experiment>
```

The experiment argument selects the dataset to generate. For example:

```bash
./artifact/scripts/gen_dataset.sh oa
```

Run this script before executing a benchmark that requires generated input data.

### `run_oa_bench.sh`

Runs the oblivious access benchmark.

This script benchmarks:

- RingOA
- RingOA with FSC
- Shared OT
- Bai et al.
- DuORAM

The DuORAM baseline must be built before running this script. See `artifact/baseline/README.md`.

Typical usage:

```bash
./artifact/scripts/run_oa_bench.sh
```

To override the database bit sizes:

```bash
DB_BITS=20,26 ./artifact/scripts/run_oa_bench.sh
```

To override the tested networks:

```bash
NETWORKS="LAN WAN" ./artifact/scripts/run_oa_bench.sh
```

The OA runner accepts `IDEAL`, `LAN`, `MAN`, and `WAN` as network profiles. The default is `LAN WAN`.

The script stores raw logs, redirected standard output, metadata, and the parsed result table under `data/results/<run_id>/`.

The aggregated table is written to:

```text
data/results/<run_id>/parsed/oa_table.tsv
```

### `run_fts_bench.sh`

Runs the privacy-preserving full-text search benchmark.

This script benchmarks:

- OblivFMI
- OblivFMI with FSC
- SotFMI

Typical usage:

```bash
./artifact/scripts/run_fts_bench.sh
```

To override the database bit sizes:

```bash
DB_BITS=20,26 ./artifact/scripts/run_fts_bench.sh
```

To override the tested networks:

```bash
NETWORKS="LAN WAN" ./artifact/scripts/run_fts_bench.sh
```

The FTS runner accepts `LAN`, `MAN`, and `WAN`. The default is `LAN WAN`.

The aggregated table is written to:

```text
data/results/<run_id>/parsed/fts_table.tsv
```

### `run_range_bench.sh`

Runs the privacy-preserving range-query benchmark.

This script benchmarks:

- OblivRange with VAF
- OblivRange with FSC and VAF
- SotRange with VAF

Typical usage:

```bash
./artifact/scripts/run_range_bench.sh
```

To override the database bit sizes:

```bash
DB_BITS=20,26 ./artifact/scripts/run_range_bench.sh
```

To override the tested networks:

```bash
NETWORKS="LAN WAN" ./artifact/scripts/run_range_bench.sh
```

The range-query runner accepts `LAN`, `MAN`, and `WAN`. The default is `LAN WAN`.

The aggregated table is written to:

```text
data/results/<run_id>/parsed/range_table.tsv
```

### `parse_oa_log.sh`

Parses the oblivious access benchmark logs for RingOA, RingOA with FSC, Shared OT, and Bai et al.

For all protocols, the script extracts both the execution time and the communication volume reported by P0.

It is normally invoked automatically by `run_oa_bench.sh`.

Manual usage:

```bash
./artifact/scripts/parse_oa_log.sh <log_directory>
```

### `parse_duoram_log.sh`

Parses the DuORAM benchmark log.

The script extracts the execution time reported by P0 and the communication volume reported by P2.

It is normally invoked automatically by `run_oa_bench.sh`, which appends the DuORAM rows to the common oblivious access result table.

Manual usage:

```bash
./artifact/scripts/parse_duoram_log.sh <duoram_log_file>
```

### `parse_fts_log.sh`

Parses the full-text search benchmark logs for OblivFMI, OblivFMI with FSC, and SotFMI.

The script extracts the preprocessing and online execution times reported by P1. For communication, it uses P0 for preprocessing and P1 for the online phase.

It is normally invoked automatically by `run_fts_bench.sh`.

Manual usage:

```bash
./artifact/scripts/parse_fts_log.sh <log_directory>
```

### `parse_range_log.sh`

Parses the range-query benchmark logs.

The script extracts the preprocessing and online execution times reported by P1. For communication, it uses P0 for preprocessing and P1 for the online phase.

It is normally invoked automatically by `run_range_bench.sh`.

Manual usage:

```bash
./artifact/scripts/parse_range_log.sh <log_directory>
```

### `set_network.sh`

Configures loopback network emulation for a benchmark.

Supported profiles are `LAN`, `MAN`, and `WAN`.

Typical usage:

```bash
sudo ./artifact/scripts/set_network.sh LAN
```

The benchmark runner scripts call this script automatically.

### `reset_network.sh`

Removes the network emulation settings applied to the loopback interface.

Typical usage:

```bash
sudo ./artifact/scripts/reset_network.sh
```

The benchmark runner scripts call this script automatically when a benchmark completes or exits.

## Common Environment Variables

The benchmark runner scripts support environment variables for overriding their default configuration.

### `DB_BITS`

Selects the database bit sizes.

A single bit size:

```bash
DB_BITS=20 ./artifact/scripts/run_oa_bench.sh
```

A comma-separated list:

```bash
DB_BITS=20,22,24 ./artifact/scripts/run_oa_bench.sh
```

A contiguous range:

```bash
DB_BITS=20:24 ./artifact/scripts/run_oa_bench.sh
```

A range in `start:end:step` format:

```bash
DB_BITS=20:26:2 ./artifact/scripts/run_oa_bench.sh
```

### `NETWORKS`

Selects the network profiles to benchmark.

```bash
NETWORKS="LAN WAN" ./artifact/scripts/run_oa_bench.sh
```

The OA benchmark also supports `IDEAL`:

```bash
NETWORKS="IDEAL LAN WAN" ./artifact/scripts/run_oa_bench.sh
```

## Output Structure

A benchmark run creates a directory similar to:

```text
data/results/<run_id>/
├── logs/
├── stdout/
├── parsed/
└── run_meta.txt
```

- `logs/` contains benchmark logs used by the parser scripts.
- `stdout/` contains redirected standard output and standard error from each RingOA party.
- `parsed/` contains aggregated TSV result tables.
- `run_meta.txt` records the benchmark configuration and timestamps.

## Network Privileges

The network scripts require `sudo` because they modify Linux traffic-control settings on the loopback interface.
The benchmark runner scripts automatically configure and restore the network settings.
If a benchmark is interrupted and the network configuration is not restored automatically, run:

```bash
sudo ./artifact/scripts/reset_network.sh
```

## Notes

- Do not run multiple benchmark scripts concurrently on the same machine because they share the loopback network configuration.
- The benchmark runners verify that required binaries and parser scripts exist and are executable before starting.
- The benchmark runners resolve the repository location from the script path, so binary and helper-script paths do not depend on the current working directory.
