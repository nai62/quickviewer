# RAR benchmark

`rar-benchmark` measures the RAR access patterns used by QuickViewer. It accepts one or more RAR archives and runs the same fixed benchmark sequence for each archive in argument order.

## Usage

```text
rar-benchmark <archive.rar> [archive2.rar ...]
```

Show help with any of the following:

```text
rar-benchmark --help
rar-benchmark -h
rar-benchmark /?
```

Benchmark results are written to standard output. With multiple archive arguments, each archive is benchmarked in the order given.

## Benchmark sequence

For each archive, the benchmark first opens the archive in `OpenModeList`, records the listing time, and collects all non-directory entries into a file list. Directory entries are excluded from the read benchmarks.

For `N` files, three positions are selected:

```text
lower  = N / 3
middle = N / 2
upper  = (N * 2) / 3
```

If `lower` and `upper` are equal while at least two files exist, `upper` is changed to the last file.

The cases then run in this order:

1. **list**
   - Measures opening the archive in `OpenModeList` and scanning its metadata.
   - Reports whether the archive is solid, the total entry count, and the non-directory file count.

2. **cold read**
   - Creates a fresh `RarExtractor`.
   - Opens the archive in `OpenModeList` before timing begins.
   - Reads `files[middle]` with no warm-up read.

3. **forward**
   - Creates a fresh `RarExtractor`.
   - Reads `files[lower]` as an untimed warm-up.
   - Resets access statistics.
   - Measures reading `files[upper]`.
   - This represents a forward jump through the archive.

4. **backward**
   - Creates a fresh `RarExtractor`.
   - Reads `files[upper]` as an untimed warm-up.
   - Resets access statistics.
   - Measures reading `files[lower]`.
   - This represents moving backward in the archive and is especially useful for observing solid-RAR reopen behavior.

   If `lower` and `upper` are not distinct, the forward and backward cases are reported as `n/a`.

5. **sequence**
   - Creates a fresh `RarExtractor`.
   - Measures reading every non-directory file from the first entry to the last entry in archive order.

6. **cache hit**
   - Creates a fresh `RarExtractor`.
   - Reads `files[middle]` as an untimed warm-up.
   - Resets access statistics.
   - Measures reading the same `files[middle]` entry again.
   - For solid archives, this exercises the in-memory extracted-data cache when the entry is cacheable.

## Timing and isolation

Each read case (`cold read`, `forward`, `backward`, `sequence`, and `cache hit`) uses a new `RarExtractor`. State and cache contents are therefore not carried from one case to the next.

Within each read case:

1. The archive is opened in `OpenModeList` before the timer starts.
2. Any warm-up reads are performed before the timer starts.
3. Access statistics are reset after warm-up reads.
4. The timer starts immediately before the measured reads.
5. Only the measured reads contribute to `elapsed` and `bytes`.

The `list` case is different: its timer measures the `OpenModeList` open/list operation itself.

## Reported fields

Each measured case reports:

| Field | Meaning |
| --- | --- |
| `ms` | Elapsed time for the measured operation, in milliseconds. |
| `bytes` | Total uncompressed bytes returned by the measured reads. |
| `reopenCount` | Number of times the extraction strategy reopened the RAR archive during the measured reads. |
| `readHeaderCount` | Number of RAR headers read during the measured reads. |
| `skipCount` | Number of entries skipped with `RAR_SKIP`. |
| `extractCount` | Number of entries processed with `RAR_TEST` to obtain their data. |
| `cacheHitCount` | Number of reads served from the solid-RAR data cache. |
| `success` | Whether the case completed successfully. |
| `error` | `none`, `password`, `unsupported`, `corrupt`, or `io`. |

Example output shape:

```text
archive: C:/path/to/book.rar
solid: true, entries=300, files=300
list: ...
cold read: ...
forward: ...
backward: ...
sequence: ...
cache hit: ...
```

## Exit codes

- `0`: all requested archives completed successfully, or help was displayed.
- `1`: at least one archive benchmark failed.
- `2`: no archive argument was supplied.
