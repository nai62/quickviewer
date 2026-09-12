# Image loading benchmarks

QuickViewer has a command-line image loading benchmark for comparing decoder
and source-loading performance without opening the viewer window. Use a Release
build for performance measurements.

On the default Windows verification layout, build with:

```bat
scripts\verify-windows.cmd release
```

The executable is then normally located at:

```text
C:\build\quickviewer-msvc2022_64-release\bin\QuickViewer.exe
```

## Basic usage

Benchmark an image, directory, or archive:

```bat
C:\build\quickviewer-msvc2022_64-release\bin\QuickViewer.exe ^
  --benchmark C:\path\to\images ^
  --recursive ^
  --runs 5 ^
  --warmup 2 ^
  --output results\image-benchmark.csv
```

Multiple input paths may be specified. Directories are non-recursive unless
`--recursive` is present. Archive inputs benchmark supported image entries in
the archive; nested archives are not opened recursively.

If `--output` is omitted, a timestamped CSV file is written in the current
working directory. A human-readable summary is written next to the CSV as
`<csv-path>.summary.txt`. The summary includes the parsed benchmark options,
aggregate results, and any paired decoder comparison results.

## Benchmark modes

`source-decode` is the default mode. Each measured iteration loads or extracts
the compressed image data and then decodes it:

```bat
QuickViewer.exe --benchmark C:\images --benchmark-mode source-decode
```

`decode-only` loads the compressed bytes once per image before warm-up and
reuses the same bytes for every decode iteration:

```bat
QuickViewer.exe --benchmark C:\images --benchmark-mode decode-only
```

Use `decode-only` when comparing decoder cost without filesystem or archive
extraction time.

## Paired JPEG decoder comparison

`decoder-compare` performs a paired JPEG comparison. Each JPEG is loaded or
extracted once, then the exact same compressed bytes are passed to both
requested decoders. Decoder order is reversed on alternating iterations to
reduce ordering bias. Non-JPEG inputs are skipped in this mode.

The default comparison is Qt versus TurboJPEG:

```bat
QuickViewer.exe ^
  --benchmark C:\images ^
  --recursive ^
  --benchmark-mode decoder-compare ^
  --jpeg-decoders qt,turbojpeg ^
  --runs 10 ^
  --warmup 2 ^
  --output results\jpeg-compare.csv
```

`--jpeg-decoders` must contain `qt` and `turbojpeg` exactly once each. Their
order defines the baseline and candidate in the paired summary. With the
default `qt,turbojpeg`, speedup is calculated for each image as:

```text
median Qt decode time / median TurboJPEG decode time
```

The reported median speedup is the median of those per-image ratios, rather
than a ratio of aggregate medians. An image is included in the paired result
only when every measured run used the requested backend successfully for both
decoders.

TurboJPEG requests may fall back to the Qt image reader for unsupported inputs.
The CSV records requested and actual backends separately. The benchmark also
classifies common JPEG exclusions as `icc-profile`, `four-component-jpeg`, or
`other`; these classifications are intended for benchmark diagnostics rather
than as a complete JPEG validator.

`--jpeg-decoder` controls ordinary `source-decode` and `decode-only` runs. In
`decoder-compare` mode, the JPEG decoder is selected separately for each side
of the comparison from `--jpeg-decoders`.

## Decoder selection

For ordinary modes, JPEG can be selected with:

```text
--jpeg-decoder auto|qt|turbojpeg
```

WebP can be selected with:

```text
--webp-decoder auto|qt|libwebp
```

The CSV always records the backend that actually decoded the image, so native
decoder fallbacks remain visible.

## Output

The CSV is the machine-readable per-run output. It includes source and output
sizes, source-load time, decode time, post-processing time, total time,
throughput, requested decoder, actual decoder, and fallback reason.

The `.summary.txt` file is intended for people. Its tables are aligned with
spaces rather than tab delimiters. It contains:

- the parsed command-line options;
- aggregate median and p95 decode timing grouped by format, container, requested
  backend, actual backend, and size bucket;
- median total time and decode throughput;
- for `decoder-compare`, paired-image counts, per-backend decode medians,
  median/p10/p90 speedup, and excluded/fallback image counts.

For reproducible comparisons, keep the build, input set, run count, QuickViewer
settings, and machine load consistent between runs.
