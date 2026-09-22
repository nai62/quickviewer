# QuickViewer Catalogue

What the catalog feature holds, who owns which part of it, and the rules it
follows. The viewer itself is described in [Architecture.md](Architecture.md).

## What a catalogue holds

- A **catalogue** is a folder the user registered in the catalog database.
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
| `catalogrecords.h` | the records the catalog UI passes around |
| `CatalogWindow` | the list, the search box, the tag bar, and the tag editor of one book |
| `VolumeTagDialog` | the title and the tags of one volume |
| `ManageDatabaseDialog` | catalogues: adding and dropping folders, starting and stopping a build, deleting, removing entries whose folders are gone, showing one catalogue's folder where the platform lists files, and the books of one catalog with their covers and the tag editor for each |

`CatalogBuilder` reads the file system and holds no database; the catalog
walks one folder level at a time, one scan per folder on a worker thread, and
stores what the finished scans returned on the thread that owns the
connection.

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
  whatever case is typed.
- The tag bar above the list shows the tags of the books on the list, most
  used first, up to eight of them, and only when more than one tag exists.
  Pressing one adds it to the search words; pressing it again removes it.
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
leaves behind.

Tags are only what the name suggests. The rules exist to fill a new catalogue,
not to model anybody's naming scheme, and the user is expected to correct them
with the tag editor.

## Traversal rules

- The catalogue walks one folder level at a time and scans each folder on a
  worker thread.
- The folder the catalogue was created from is a volume of its own, and it
  holds no front page of its own; the folders below it take their cover from
  their first image.
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
  and a clear button - and searches the titles the list shows.
- Right-clicking a book edits its title and tags. "Manage catalogs" does the
  same for the books of the catalogue selected there, and lists every volume,
  cover or not.
- "Manage catalogs" also lists each catalogue with its name, the time it was
  built and the folder it was created from, and **Open in Explorer** shows that
  folder where the platform lists files.
- Choosing a catalogue there starts on its first book, so the cover pane beside
  the list of books has something to show. Choosing another book shows that
  book's cover, and a book the catalog stored without one says so.

## Tests

| Target | Covers |
| --- | --- |
| `tests/catalogdatabase` | the catalog database: opening and creating the file, refusing an unreadable one, building a catalogue from folders and archives, covers, the volume name rules, editing titles and tags, cancelling and failing a build, removing a catalogue, removing entries whose folders are gone, and what the catalogue manager lists and shows for a selection |

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
