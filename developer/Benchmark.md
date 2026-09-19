# QuickViewer benchmarks

QuickViewer provides a command-line benchmark interface for measuring distinct
parts of the image-opening pipeline. Use a Release build for performance
measurements.

On the default Windows verification layout:

```bat
scripts\verify-windows.cmd release
```

The executable is normally located at:

```text
C:\build\quickviewer-msvc2022_64-release\bin\QuickViewer.exe
```

## Command shape

```text
QuickViewer.exe --benchmark <suite> [options] <input...>
```

The suite selects the pipeline region being measured. Decoder choice, page
selection, sorting, run count, warmups, and output destination are independent
conditions.

Supported suites:

- `decode`: decode and image-pipeline post-processing only. File reads and
  archive extraction happen before the timer.
- `entry-load`: file read or archive entry extraction plus decode and
  post-processing. Archive opening/indexing happens before the per-entry timer.
- `archive-open`: archive opening/indexing and production page-list creation.
  Selected-image extraction and decoding are excluded.
- `first-image`: opening an input through obtaining the selected decoded image.
  Rendering is excluded.
- `first-paint`: fresh-process startup through the first actual image paint.

The old `--benchmark-mode`, format-specific decoder flags, and
`decoder-compare` mode are not part of this interface.

## Common options

```text
--runs <N>       Measured runs. Default: 5.
--warmup <N>     Unmeasured warmup runs. Default: 2.
--output <path>  Raw CSV output.
--recursive      Recursively scan directory inputs.
--page <value>   first, resume, or a zero-based page index.
--sort <mode>    name, name-desc, size, size-desc, mtime, or mtime-desc.
```

If `--output` is omitted, QuickViewer creates a timestamped file such as:

```text
results/quickviewer-benchmark-20260914-091500.csv
```

A human-readable summary is saved next to the CSV with `.summary.txt` appended
to its name, for example:

```text
results/quickviewer-benchmark-20260914-091500.csv.summary.txt
```

CSV is the canonical raw output.

## Decoder selection

Decoder choice uses a repeatable option:

```text
--decoder <format>=<backend>[,<backend>...]
```

Examples:

```text
--decoder jpeg=auto
--decoder jpeg=turbojpeg
--decoder jpeg=qt,turbojpeg
--decoder png=qt,libspng
--decoder webp=qt,libwebp
```

Multiple formats may be specified independently:

```text
--decoder jpeg=qt,turbojpeg --decoder png=qt,libspng
```

If decoder selection is omitted, the normal automatic decoder choice is used.

When multiple backends are requested for the selected image format, QuickViewer
runs them under equivalent benchmark conditions and writes a relative speed
ratio to the summary. The raw CSV records requested and actual decoders where
the suite can observe the decoder metrics directly. A native request may fall
back to Qt when the input requires behavior unsupported by the native fast path.

## Page selection

```text
--page first
--page resume
--page 0
--page 100
```

`first` is the default and always selects page 0 after production sorting.

`resume` uses the same saved read-progress rule as normal volume startup. If no
usable saved position exists, it resolves to page 0. The CSV records both the
request and the resolved page index/source.

Numeric pages are zero-based. An out-of-range page is an error; the benchmark
does not silently substitute another page.

## Sorting

`--sort` maps directly to QuickViewer's production page sorting:

```text
name
name-desc
size
size-desc
mtime
mtime-desc
```

The default is `name`.

## Suite details

### `decode`

```bat
QuickViewer.exe --benchmark decode ^
  --decoder jpeg=qt,turbojpeg ^
  --runs 20 ^
  benchmark-images\
```

For normal files, file reads happen before the timed decode. For archive input,
the archive is opened and encoded entry bytes are extracted before warmup and
measurement of that image.

### `entry-load`

```bat
QuickViewer.exe --benchmark entry-load ^
  --runs 10 ^
  book.zip
```

For a normal image, each measured run includes the file read. For an archive
entry, each measured run includes entry extraction/decompression. Archive
opening, indexing, and page-list creation remain outside the per-entry timer.

### `archive-open`

```bat
QuickViewer.exe --benchmark archive-open ^
  --runs 10 ^
  huge.zip huge.rar
```

A fresh `Volume`/archive loader is created for every warmup and measured run.
Selected-image extraction and image decoding are never performed by this suite.

The current archive loaders perform part of their enumeration/filtering while
their archive object is being constructed. Consequently, `archive_open_us`
covers that constructor work, while `page_list_us` covers QuickViewer's
production `Volume::loadPageList()` step. More granular CSV stage columns remain
empty when the production layer does not expose a separate boundary.

### `first-image`

```bat
QuickViewer.exe --benchmark first-image ^
  --runs 10 ^
  --page first ^
  book.zip
```

Each iteration creates fresh input/Volume state, performs production page-list
sorting, resolves the requested page, reads/extracts it, and decodes it through
QuickViewer's normal image pipeline. Rendering-only work is excluded.

To exercise saved reading progress:

```bat
QuickViewer.exe --benchmark first-image ^
  --runs 10 ^
  --page resume ^
  book.zip
```

### `first-paint`

```bat
QuickViewer.exe --benchmark first-paint ^
  --runs 10 ^
  --page first ^
  book.zip
```

`first-paint` accepts exactly one positional input. Every warmup and measured
run launches a fresh QuickViewer process. The child process uses the existing
startup profiler and exits automatically after the first decoded image has
actually completed its initial paint.

The timer begins at QuickViewer's internal `main.entry` marker. Explorer or
shell launch latency before process execution is outside the benchmark.

Standard interpretation is process-cold with the operating-system filesystem
cache potentially warm. The benchmark does not flush the Windows filesystem
cache.

## CSV output

Each measured row records at least:

```text
suite
input
run
success
total_us
```

Relevant rows also contain:

```text
container
archive_size
archive_entry_count
image_count
selected_entry
selected_uncompressed_size
image_format
width
height
requested_page
resolved_page
page_source
requested_decoder
actual_decoder
decoder_fallback_reason
sort
```

Timing columns include:

```text
library_init_us
archive_open_us
enumeration_us
filter_us
archive_sort_us
page_list_us
page_sort_us
page_select_us
source_load_us
extract_us
decode_us
postprocess_us
decode_pipeline_us
total_us
```

A blank timing field means the stage is not applicable or is not separately
observable in that suite.

`first-paint` rows also contain milestone columns ending in `_at_us`, from
`application_constructed_at_us` through `first_image_painted_at_us`. Each value
is elapsed microseconds from the child process's internal `main.entry` marker,
not a stage duration. Subtract adjacent milestone values to locate startup,
volume loading, image preparation, and initial painting costs. A blank milestone
means that marker was not reached or was not recorded during the run.

Milestones inside the startup phases split the two blocks that dominate a cold
process:

- `application_base_ready_at_us` ends Qt's own application setup, so
  `application_construct_begin_at_us` to `application_base_ready_at_us` is Qt
  and `application_base_ready_at_us` to
  `application_settings_loaded_at_us` is QuickViewer's settings, theme, and
  key-map loading.
- `mainwindow_ui_setup_at_us` ends `setupUi()` and
  `mainwindow_actions_registered_at_us` ends the action registration inside the
  MainWindow constructor.
- `volume_prefetch_begin_at_us` and `volume_prefetch_page_ready_at_us` bracket
  the startup volume prefetch that runs on a worker thread while the window is
  still being created. When it finished before the startup load ran,
  `volume_loader_begin_at_us` appears before `startup_volume_begin_at_us` and
  `session_volume_built_at_us` follows `startup_volume_begin_at_us` closely.

## Reproducibility

Run warmups before measured iterations and keep excluded setup work outside the
timed region. `archive-open` and `first-image` recreate the Volume state on each
iteration. `first-paint` recreates the entire QuickViewer process.

For comparisons, keep the build, corpus, QuickViewer settings, run count,
machine load, and command line identical. Do not change production behavior
merely to make benchmark results faster.
