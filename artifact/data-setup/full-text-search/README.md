# Preparing GRCh38 Chromosome Data

This directory contains scripts for preparing chromosome FASTA files from the GRCh38 human reference genome.
The prepared FASTA files are normalized so that sequence lines contain only uppercase `A`, `C`, `G`, `T`, and `N` bases.
The experiments evaluate datasets up to `2^30` bases. To construct a dataset of this size from GRCh38, this artifact uses chromosomes `chr1` through `chr6`.

## Files

| File                              | Purpose                                                                                                                         |
| --------------------------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| `extract_and_clean_grch38_chr.sh` | Downloads GRCh38, builds the FASTA index, extracts selected chromosomes, normalizes FASTA files, and writes base count reports. |
| `count_base.py`                   | Counts base characters in a FASTA file.                                                                                         |
| `to_upper_fasta.py`               | Converts FASTA sequence lines to uppercase.                                                                                     |
| `replace_ambiguous_base.py`       | Replaces ambiguous IUPAC base symbols with `N`.                                                                                 |

## Requirements

On Ubuntu, install the required packages with:

```bash
sudo apt update
sudo apt install -y wget gzip samtools python3
```

## Recommended workflow

Run the provided script from the project root:

```bash
bash ./artifact/data-setup/full-text-search/extract_and_clean_grch38_chr.sh
```

The script uses `./data/bench/grch38` as the working directory. The downloaded GRCh38 FASTA file, FASTA index, extracted chromosome FASTA files, normalized FASTA files, cleaned FASTA files, and base count reports are all written there.

By default, the script prepares chromosomes `chr1` through `chr6`, which are sufficient for constructing datasets up to `2^30` bases.

The script performs the following steps:

1. Downloads the GRCh38 reference genome from NCBI if it is not already present.
2. Decompresses the FASTA file.
3. Builds a FASTA index with `samtools faidx`.
4. Extracts the selected chromosomes into temporary files.
5. Converts sequence lines to uppercase.
6. Replaces ambiguous IUPAC base symbols with `N`.
7. Writes the final cleaned FASTA files and base count reports.

## Output files

All output files are written to: `./data/bench/grch38`
For each chromosome, the script produces:

| Output                 | Description                                                           |
| ---------------------- | --------------------------------------------------------------------- |
| `chr*_clean.fa`        | Final cleaned FASTA file containing only `A`, `C`, `G`, `T`, and `N`. |
| `chr*_clean.count.txt` | Base count report for the cleaned FASTA file.                         |

For the default setting, the final cleaned files are:

```text
./data/bench/grch38/chr1_clean.fa
./data/bench/grch38/chr2_clean.fa
./data/bench/grch38/chr3_clean.fa
./data/bench/grch38/chr4_clean.fa
./data/bench/grch38/chr5_clean.fa
./data/bench/grch38/chr6_clean.fa
```

The downloaded reference genome and FASTA index are also stored in the same directory:

```text
./data/bench/grch38/GCF_000001405.26_GRCh38_genomic.fna
./data/bench/grch38/GCF_000001405.26_GRCh38_genomic.fna.fai
```

---

## Optional: Using different chromosomes

By default, the benchmark datasets are generated from chromosomes 1–6 of GRCh38.
If you would like to prepare datasets from a different set of chromosomes, pass chromosome identifiers as arguments:

```bash
bash ./artifact/data-setup/full-text-search/extract_and_clean_grch38_chr.sh 1 2 X Y MT
```

The script uses RefSeq accessions for GRCh38, such as:

| Chromosome | Accession      |
| ---------- | -------------- |
| chr1       | `NC_000001.11` |
| chr2       | `NC_000002.12` |
| chr3       | `NC_000003.12` |
| chr4       | `NC_000004.12` |
| chr5       | `NC_000005.10` |
| chr6       | `NC_000006.12` |
| chrX       | `NC_000023.11` |
| chrY       | `NC_000024.10` |
| chrM       | `NC_012920.1`  |

## Optional: Manual extraction

The script is the recommended way to prepare the data. For reference, a chromosome can be extracted manually with `samtools faidx`.

First, move to the working directory:

```bash
mkdir -p ./data/bench/grch38
cd ./data/bench/grch38
```

Then extract a chromosome:

```bash
samtools faidx GCF_000001405.26_GRCh38_genomic.fna NC_000001.11 > chr1.fa
```

The helper scripts can then be applied manually from the project root:

```bash
python3 ./artifact/data-setup/full-text-search/count_base.py ./data/bench/grch38/chr1.fa
python3 ./artifact/data-setup/full-text-search/to_upper_fasta.py ./data/bench/grch38/chr1.fa ./data/bench/grch38/chr1_upper.fa
python3 ./artifact/data-setup/full-text-search/replace_ambiguous_base.py ./data/bench/grch38/chr1_upper.fa ./data/bench/grch38/chr1_clean.fa
python3 ./artifact/data-setup/full-text-search/count_base.py ./data/bench/grch38/chr1_clean.fa
```

## Notes

`N` represents an unknown or unresolved base. Lowercase bases in reference FASTA files are often used to indicate repeat-masked regions. This workflow converts them to uppercase so that the final sequence alphabet is uniform. Ambiguous IUPAC base symbols, such as `R`, `Y`, `K`, `M`, `S`, `W`, `B`, `D`, `H`, and `V`, are replaced with `N`. The final cleaned FASTA files are intended to contain only: `A`, `C`, `G`, `T`, and `N`.
