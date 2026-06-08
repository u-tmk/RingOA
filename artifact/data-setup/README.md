# Real Dataset Preparation

The experiments in the paper use real genomic datasets for the full-text search and range search benchmarks. This directory contains scripts and instructions for preparing those datasets.

The dataset preparation workflow is organized into two subdirectories:

* [`full-text-search/README.md`](full-text-search/README.md): Preparation of the reference genome dataset used for the FM-index benchmarks.
* [`range-search/README.md`](range-search/README.md): Preparation of the variant allele frequency (VAF) dataset used for the range search benchmarks.

After preparing the datasets, generate benchmark datasets with the `real` option:

```bash
./artifact/scripts/gen_dataset.sh fmi real
./artifact/scripts/gen_dataset.sh range real
```

The default artifact workflow does not require these steps and uses randomly generated datasets instead.
