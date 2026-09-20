# QuickViewer Architecture

This document defines the main domain terms and responsibility boundaries used
by QuickViewer. It intentionally describes concepts rather than individual APIs.

## Core terminology

### Viewer session

A `ViewerSession` coordinates one viewing session. It owns the current viewer
state, navigation position, visible content, and transitions between volumes.

### Volume

A `Volume` is an ordered collection of pages backed by a folder or archive. It
knows how to list and load its images and maintains the image prefetch cache. A
volume does not define which page is currently selected by the viewer.

### Volume loader

A `VolumeLoader` creates and initializes a volume from a path. It also supports
the specialized loading paths used for a directly opened image, a cover, or a
thumbnail source.

A folder volume lists one directory, except when that directory holds nothing
but one more directory: a download or export wrapper is then unwrapped into the
directory it wraps, at most a few levels deep. The limit is what keeps a link
that points back at one of its own ancestors from following itself for as long
as the filesystem allows. Pages report the path they really have, which is
inside the unwrapped directory rather than the path the reader opened.

### Volume cache

A `VolumeCache` reuses completed or in-progress volume loads. Cache identity
includes the normalized path and the loading options that affect volume
contents.

### Page and image

A **page** is a position in a volume's ordered file list. An **image** is the
decoded content loaded for that page. Page-oriented names refer to ordering or
navigation; image-oriented names refer to loading or decoded data.

`ImageContent` carries decoded image data, metadata, and reusable processing
results. `RenderedPage` owns the presentation state needed to display one
`ImageContent` in the graphics scene.

### Page index and page number

A **page index** is zero-based and is used internally. The current page index is
the index of the first visible page. A **page number** is user-facing and is
normally one-based.

### Visible pages

`VisiblePages` is the bounded set of pages currently presented by the viewer.
It contains one page in single-page cases and at most two pages for a spread.
`VisiblePageComposer` decides which page indexes form that set; it does not load
or render them.

### Page navigation

`PageNavigator` stores and validates the current page index. Higher-level
navigation rules, such as moving through spreads or switching volumes, belong
to `ViewerSession`.

### Prefetching

`PrefetchMode` describes the expected navigation direction and speed.
`PrefetchPlanner` converts that intent into page indexes, while `Volume` performs
the corresponding image loads and cache updates.

### Read progress

`ReadProgress` represents the saved reading position and completion state for a
volume. `ReadProgressStore` indexes these records by volume path and persists
them to `progress.ini`. Public C++ names do not define the file format; existing
INI keys remain compatibility constraints.

## Responsibility boundary

The viewer's mutable reading state belongs to `ViewerSession`. A `Volume`
provides ordered content and loading facilities, and may be shared or cached
without carrying a session's current-page state. Rendering state belongs to
`RenderedPage`, outside both the volume and navigation model.

### Folder text rendering

FolderItemModel keeps disk names separate from safe display text and rasterized
text. Characters missing from either weight of the view's primary font initially
use a replacement; file operations always use the disk name. FolderTextCache is
owned by the application, so destroying or replacing a panel cannot abandon a
shared request. Results are keyed by the name, resolved font and device pixel
ratio, and hold one white coverage mask per weight: the delegate tints a mask
with the palette colour its row needs, so a result serves every state. The list
reports the rows it shows, and only those ask for text, so a folder with
thousands of names renders what is on screen; requests outstanding at once are
bounded and the model asks for the rest as results arrive.

The same executable runs a private `--folder-text-helper` mode to shape and
rasterize missing-glyph names. A Qt worker thread would still contend with the
GUI's font database mutex and would not populate its thread-local font engines.
The helper has a separate font database, receives framed requests over standard
input and returns images over standard output. The delegate never passes the
resolved rare text back to GUI-side font shaping, including in size hints. Cache
storage is bounded, a model retains its current images across list resets, and
the helper leaves after an idle period - the next request starts it again.
Failed requests leave safe display text; one key is attempted twice and then
keeps its placeholder instead of asking on every paint. The startup profile
driver lives in `src/benchmark/startupfoldertextprofile.cpp`; see Testing.md.
