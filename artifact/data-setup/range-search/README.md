# Preparing Range Search Data

The artifact already includes the precomputed file: `vaf_values.txt`. 

For artifact evaluation, no data preprocessing is required. Simply copy the file to the benchmark data directory:

```bash
mkdir -p ./data/icgc
cp ./artifact/data-setup/range-search/vaf_values.txt ./data/icgc/vaf_values.txt
```

The remainder of this document describes how vaf_values.txt was generated from the original ICGC MAF file.

---

## Reference: Source data

The ICGC 25K release data can be downloaded from the ICGC ARGO documentation: [https://docs.icgc-argo.org/docs/data-access/icgc-25k-data](https://docs.icgc-argo.org/docs/data-access/icgc-25k-data). The original MAF file is: `final_consensus_passonly.snv_mnv_indel.icgc.public.maf`. The precomputed `vaf_values.txt` file was generated from a sorted version of the MAF file: `final_consensus_passonly.snv_mnv_indel.icgc.public.maf.sorted`. Users do not need to regenerate `vaf_values.txt` for artifact evaluation. The following sections document how it was produced.

## Files

| File                  | Purpose                                                                                             |
| --------------------- | --------------------------------------------------------------------------------------------------- |
| `vaf_values.txt`      | Precomputed VAF input values used in the range search experiments.                                  |
| `extract_vaf_only.sh` | Extracts VAF values from the sorted MAF file and writes them as integers in the range `0` to `100`. |
| `maf_vaf_extract.sh`  | Extracts gene name, chromosome, position, read counts, and VAF from the sorted MAF file.            |
| `gene_line_ranges.sh` | Finds line ranges for selected genes, such as `BRCA1` and `BRCA2`.                                  |
| `maf_vaf.tsv`         | Intermediate TSV file used when computing gene line ranges.                                         |
| `brca_ranges.tsv`     | Line ranges for `BRCA1` and `BRCA2` in `maf_vaf.tsv`.                                               |

## Reference: Sorting the MAF file

Before extracting VAF values, sort the original MAF file by genomic position. The MAF header contains `Chromosome` as the second column and `Start_position` as the third column. The following command sorts the file by chromosome and start position while preserving the header line:

```bash
{
  head -n 1 final_consensus_passonly.snv_mnv_indel.icgc.public.maf
  tail -n +2 final_consensus_passonly.snv_mnv_indel.icgc.public.maf \
    | sort -t $'\t' -k2,2V -k3,3n
} > final_consensus_passonly.snv_mnv_indel.icgc.public.maf.sorted
```

The sorted file is: `final_consensus_passonly.snv_mnv_indel.icgc.public.maf.sorted`.

The VAF values and BRCA line ranges are generated from this sorted MAF file.

## Reference: Generating `vaf_values.txt`

Generate `vaf_values.txt` from the sorted MAF file using:

```bash
./extract_vaf_only.sh final_consensus_passonly.snv_mnv_indel.icgc.public.maf.sorted vaf_values.txt
```

The script extracts VAF values and converts them to integer values from `0` to `100`.
The resulting file contains one integer VAF value per line:

```text
0
13
42
100
...
```

This file is already included in the artifact. For benchmark execution, copy it to `./data/icgc/vaf_values.txt` as described in the setup step.

## Reference: Generating BRCA line ranges

For experiments that use BRCA-related ranges, first generate `maf_vaf.tsv` from the sorted MAF file:

```bash
./maf_vaf_extract.sh final_consensus_passonly.snv_mnv_indel.icgc.public.maf.sorted maf_vaf.tsv
```

Then compute the line ranges for `BRCA1` and `BRCA2`:

```bash
./gene_line_ranges.sh maf_vaf.tsv brca_ranges.tsv "BRCA1,BRCA2"
```

The output file has the following format:

```text
BRCA1    <start_line>    <end_line>
BRCA2    <start_line>    <end_line>
```

These line ranges identify the entries corresponding to `BRCA1` and `BRCA2` in `maf_vaf.tsv`.
