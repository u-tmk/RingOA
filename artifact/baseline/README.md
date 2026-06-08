# Baseline Implementations

This directory contains third-party baseline implementations used in the RingOA artifact evaluation.

## DuORAM (PRAC)

The `prac/` directory contains a modified copy of the PRAC repository used to obtain the DuORAM results reported in the paper.

* Original repository: https://git-crysp.uwaterloo.ca/iang/prac/
* Original commit: `12d1f91e51165cfcaf250ac4b4fe48ce29fb18dc`

The PRAC project is distributed under the MIT License. The original license and documentation are preserved in the `prac/` directory. Minor modifications were applied to integrate the implementation into the benchmarking workflow of this artifact. These modifications add measurement output and do not change the core DuORAM protocol.

## Building PRAC

PRAC requires the following packages:

```bash
sudo apt update
sudo apt install -y libbsd-dev libboost-all-dev
```

Build the executable with:

```bash
cd baseline/prac
make -j$(nproc)
```
This will produce the `out/prac` executable, which can be used to run the DuORAM protocol and obtain performance measurements.
