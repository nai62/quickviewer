# Image loading benchmarks

QuickViewer has a command-line image loading benchmark for comparing decoder
and source-loading performance without opening the viewer window. Use a Release
build for performance measurements.

On the default Windows verification layout, build with:

```bat
scripts\verify-windows.cmd release
```

The libspng backend is built from pinned submodules: libspng v0.7.4 and
miniz 2.2.0. Initialize submodules before configuring a fresh checkout:

```bat
git submodule update --init --recursive
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

## Paired decoder comparison

`decoder-compare` performs a paired comparison for one image format. Each
selected image is loaded or extracted once, then the exact same compressed
bytes are passed to both requested decoders. Decoder order is reversed on
alternating iterations to reduce ordering bias. Other image formats are
skipped.

JPEG is the default comparison format. Qt versus TurboJPEG can be measured
with:

```bat
QuickViewer.exe ^
  --benchmark C:\images ^
  --recursive ^
  --benchmark-mode decoder-compare ^
  --compare-format jpeg ^
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

PNG can be compared in the same way with Qt versus libspng:

```bat
QuickViewer.exe ^
  --benchmark C:\images ^
  --recursive ^
  --benchmark-mode decoder-compare ^
  --compare-format png ^
  --png-decoders qt,libspng ^
  --runs 10 ^
  --warmup 2 ^
  --output results\png-compare.csv
```

`--png-decoders` must contain `qt` and `libspng` exactly once each. With
`qt,libspng`, the reported per-image speedup is the median Qt decode time
divided by the median libspng decode time.

For both formats, the reported median speedup is the median of the per-image
ratios rather than a ratio of aggregate medians. An image is included in the
paired result only when every measured run used the requested backend
successfully for both decoders.

Native decoder requests may fall back to the Qt image reader when preserving
viewer behavior requires features the fast path does not handle. The CSV
records requested and actual backends separately. Common JPEG exclusions are
classified as `icc-profile`, `four-component-jpeg`, or `other`. Common libspng
exclusions are classified as `animated-png`, `icc-profile`, `gamma-chunk`,
`chromaticities`, `high-bit-depth`, or `other`. These classifications are
benchmark diagnostics rather than complete format validators.

`--jpeg-decoder` and `--png-decoder` control ordinary `source-decode` and
`decode-only` runs. In `decoder-compare` mode, the decoder is selected
separately for each side of the comparison from the format-specific decoder
pair.

## Decoder selection

For ordinary modes, JPEG can be selected with:

```text
--jpeg-decoder auto|qt|turbojpeg
```

PNG can be selected with:

```text
--png-decoder auto|qt|libspng
```

`auto` uses libspng for eligible static PNG images. Animated PNG, color-managed
PNG variants that require Qt metadata handling, and 16-bit PNG fall back to Qt.

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
