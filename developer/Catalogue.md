# QuickViewer Catalogue

What the catalog feature holds, who owns which part of it, and the rules it
follows. The viewer itself is described in [Architecture.md](Architecture.md).

## What a catalogue holds

- A **catalogue** is a folder or archive the user registered in the catalog database.
- A **volume** is one folder or archive below it that holds images. The folder
  the catalogue was created from is a volume of its own.
- A **cover** is the first image of a volume in display order, stored as a
  JPEG thumbnail 96 pixels wide. A volume whose folder holds no image, or
  whose first image cannot be read, is stored without a cover.
- A **tag** belongs to one volume. Tags carry a type (0 normal, 1
  publisher(Author), 2 publisher, 3 author, 4 rate) which the list does not
  show today.

The catalog database keeps its historical file name (`thumbnail.sqlite3.db`).
The settings default, the resource the application copies and the packaging
scripts all point at it, so the name stays even though the file holds
catalogues rather than thumbnails.

## Who owns what

| Object | Owns |
| --- | --- |
| `CatalogDatabase` | the connection, transactions, the statements that fill a catalogue, and every query the catalog windows run |
| `CatalogBuilder` | reading folders and archives, and encoding covers |
| `VolumeNameParser` (`volumenameparser.{h,cpp}`) | the title and the tags a volume name suggests |
| `SearchWords` (`searchwords.{h,cpp}`) | which titles one search of the catalog list asks for |
| `catalogrecords.h` | the records the catalog UI passes around |
| `CatalogWindow` | the list, the search box, the tag bar, and the tag editor of one book |
| `VolumeTagDialog` | the title and the tags of one volume |
| `ManageDatabaseDialog` | catalogues: adding and dropping folders, starting and stopping a build, deleting, removing entries whose folders are gone, showing one catalogue's folder where the platform lists files, and the books of one catalog with their covers and the tag editor for each |

`CatalogBuilder` reads the file system and holds no database; the catalog
walks one folder level at a time, one scan per folder on a worker thread, and
stores what the finished scans returned on the thread that owns the
connection. An asynchronous build opens its own connection on its worker
thread and closes it there. The manager keeps editing and new builds disabled
until a cancelled build has finished rolling back.

## Tags

Tags belong to one volume; there is no inheritance, and a tag means the same
thing across catalogues. What the user should know:

- A catalog stores the tags a volume name suggested when it created the
  volume. Nothing re-reads the name afterwards, so a name that suggests the
  wrong tags keeps them until the user edits them.
- The user edits them from either list: right-click a book in the catalog list,
  or pick a catalogue in "Manage catalogs" and use **Edit tags...** on one of
  the books it lists. The dialog sets the title the catalogue shows for that
  book and the tags it carries, reusing a tag the catalog already knows
  whatever case is typed. The title and tags are saved in one transaction;
  a failed save leaves both unchanged. Enter in the new-tag field adds a tag
  and keeps the editor open.
- The tag bar initially shows the most-used tags across all catalogues, up to
  eight of them, and only when more than one tag exists. Opening a book replaces
  the bar with that book's tags. A pressed button requires that stored tag on
  each result, independently of the title search. Multiple selected tags must
  all match; a tag containing spaces stays one tag.
- A tag that no book carries any more is dropped from the catalog as soon as
  the book that had it loses it, or the book leaves the catalogue.
- Which title the list shows (the catalog title or the folder name) is the
  view option "Remove parenthesized text from book title", not a property of
  the volume.

### What a volume name suggests

The parser follows the shapes the catalog has always documented:

```
(First) [Publisher (Author)] Book Title (Last)
# [First] [Second] [Publisher (Author)] Book Title (Last) [Third]
```

| Shape | Title | Tags |
| --- | --- | --- |
| `Book Title` | `Book Title` | — |
| `Book Title (2017)` | `Book Title` | `2017` |
| `[Sample] Book Title` | `Book Title` | `Sample` |
| `[Publisher (Author)] Book Title` | `[Publisher (Author)] Book Title` | `Publisher`, `Author`, `Publisher (Author)` |
| `#Series Book Title` | `Book Title` | `Series` |
| `# [Series] Book Title` | `Book Title` | `Series` |

In words: a parenthesized field is a tag and leaves the title; a bracketed
field holding `publisher (author)` is the one field that stays in the title,
and it yields the publisher, the author and the pair as one tag each; any
other bracketed field is a tag; the word after a leading `#` is a tag; tag
texts are trimmed, and the title does not keep the empty space a dropped field
leaves behind. An unfinished bracketed or parenthesized field stays literal
title text.

Tags are only what the name suggests. The rules exist to fill a new catalogue,
not to model anybody's naming scheme, and the user is expected to correct them
with the tag editor.

## Traversal rules

- The catalogue walks one folder level at a time and scans each folder on a
  worker thread.
- The folder the catalogue was created from is a volume of its own. It and
  the folders below it take their cover from their first image.
- Invalid or missing source paths are rejected. A folder named with an archive
  extension is still a folder. Symlinks and Windows junctions are followed
  unless they lead back to an ancestor.
- Archive files are catalogued as volumes in the folder the catalogue was
  created from. Deeper archives are not listed as volumes.
- The recorded parent ids follow the traversal rather than the folder tree.
  The catalog list reads the flat list of volumes and does not walk them.

## The database file

- The catalog database is opened when the catalog feature is first used. If
  the file does not exist, it is written from the database the application
  bundles, through a temporary file that is renamed into place.
- A file that is already there is never replaced or deleted. If it cannot be
  opened, or does not hold the catalog schema, the operation stops and the
  window says why, leaving the file for the user to move aside.
- With a catalog database in place, the catalog list shows every volume of
  every catalogue. A volume without a cover is not listed; the status bar
  counts the volumes that can be listed and says how many stored volumes it
  leaves out.

## The panel

- The list of books keeps wheel input to itself, the way the folder panel
  does: scrolling it at its end never turns a page in the viewer.
- Its search field looks like one - a magnifier, a hint of what it searches
  and a clear button. The "Ignore parenthesized text when searching titles"
  option chooses between the catalog title and the original folder name,
  independently of the option controlling which title the list displays. A
  search is a list of words that all have to appear in that title, plus words
  that must not appear at all: a word written with a leading `-` only excludes,
  and a lone `-` stays an ordinary word.
- The list reads a cover from the database once and keeps what it fitted, so
  repainting a row does not decode its JPEG again. The cache is bounded, and
  its keys are the rows the covers are stored in, which the database never
  hands out twice.
- Right-clicking a book edits its title and tags. "Manage catalogs" does the
  same for the books of the catalogue selected there, and lists every volume,
  cover or not.
- "Manage catalogs" lists each catalogue with its name, the time it was built
  and the folder it was created from. **Add folder** and **Start creating** are
  together above the list. Start stays visible, is enabled when folders are
  waiting, and shows the pending count; during a build it becomes **Stop creating**.
- Right-click a catalogue for **Edit**, **Open in Explorer**, or **Delete**.
  The clicked row becomes the selection before the menu opens. The menu is
  also available from the keyboard, with F2 and Delete as direct shortcuts.
- **More** holds **Remove missing entries** and, after a separator, **Delete all
  catalogs**. Destructive database operations retain their confirmation dialogs.
  The manager has one **Close** button; closing with it, Escape, or the title-bar
  close button asks before discarding unbuilt requests.
- The catalogue list and the book pane have a draggable divider. Opening the
  manager selects a catalogue; adding a folder selects its pending row. Refreshing
  the list or editing a book retains the current selection when it still exists.
  Completion and cancellation are shown inline; failures still open a message.
- Choosing a catalogue there starts on its first book, so the cover pane beside
  the list of books has something to show. Choosing another book shows that
  book's cover, and a book the catalog stored without one says so.

## Tests

| Target | Covers |
| --- | --- |
| `tests/catalogdatabase` | the catalog database: opening and creating the file, refusing an unreadable one, preserving existing temporary files, building a catalogue from folders and archives, covers, the volume name rules, editing titles and tags, worker connections, cancellation, folder cycles, transaction failures, tag-cache rollback, removing a catalogue, removing entries whose folders are gone, and what the catalogue manager lists and shows for a selection, context menu targeting, closing with pending requests, and build-button state transitions |
| `tests/windowstartup` | catalog model index validity, stored-tag filtering, empty-list status, search, cover caching and placement, wheel handling and panel lifetime |

Run the catalog tests on Windows with
`scripts\verify-windows.cmd debug --test catalogdatabase`, and the whole suite
with `scripts\verify-windows.cmd debug --tests-only`; see
[Testing.md](Testing.md) for the log handling an agent has to follow.

## Known limits

- The catalog list is flat and covers every catalogue at once: registering a
  folder that another catalogue already covers shows it twice.
- A volume whose folder disappeared stays listed until the user removes it
  with "Remove missing entries" in the catalogue manager.
- The catalog database holds one cover per volume, not a file list. Reading a
  volume's pages belongs to the viewer, and listing a folder belongs to the
  folder view.
- `volumes()` reads every volume with its thumbnail, and creating catalogues
  rewrites the whole volume order, so a very large catalogue answers slowly.


## Review items requiring product decisions

- There is no incremental rescan; recreating a catalogue loses manually edited titles and
  tags. A future rescan needs rules for retaining edits and recognizing moves.
- Archives below the first folder level are deliberately not discovered.
  Changing this would expand existing catalogues and needs a traversal policy.
- Overlapping registrations are allowed, while the main list has no catalogue
  filter or path tooltip. A grouping or filtering UI could distinguish books
  with the same title without forbidding intentional duplicate catalogues.
- Coverless books are hidden from the main list, including unreadable books.
  Whether to show placeholder entries, and how to distinguish a missing image
  from a decode failure, remains a UI decision.
- Tag types are stored but not exposed in the editor. Equal names with different
  types collapse to one choice on save; preserving or retiring those types
  needs a data-model decision. Existing duplicate tag rows are not migrated.
- Large catalogues still load every cover into memory and queue a scan for every
  folder at the current level. The fitted-pixmap cache is bounded, but those
  inputs are not. Lazy covers and bounded scan submission need workload-based
  measurements before choosing limits.

Interactive Windows checks after catalog changes: create and cancel a catalogue,
try editing while cancellation is pending, close the manager during a build,
filter using a manually added multiword tag, add tags with Enter, clear the last
catalogue, and inspect list/icon modes and the manager at different DPI settings.
Run `scripts\verify-windows.cmd debug --test windowstartup` for the automated
UI regressions; it does not replace those visual checks.
