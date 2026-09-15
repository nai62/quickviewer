# Contribution for multilingual

Unless otherwise noted, QuickViewer Developer Team has created it.

The file whose author name is specified is the copyrighted work of the author, but when they contribute to the project, they are deemed to have agreed to be redistributed under the same conditions as the license of this project.

- quickviewer_el.ts
    - written by "geogeo.gr" <geogeo.gr@gmail.com>
- quickviewer_zh.ts
    - rewritten by "mcoder2014" <mcoder2014@sina.com>
- qt_el.qm
    - It is part of the Qt SDK, but since it is not included in the current SDK, it is included in this source tree.
    - http://code.qt.io/cgit/qt/qttranslations.git/commit/?id=44647ef2cd1908279f4a1142b7cbe43caede544d

## How to translate

### Get translation tool

Use Qt 6.11.2 for the QuickViewer translation workflow.

- https://www.qt.io/download-open-source/ (official)

### Procedure of work

1. Clone the repository.
1. Run `scripts/update-translations.cmd` when the translation source files need to be refreshed from the application source code. This command updates `.ts` files only.
1. Edit the appropriate `apps/quickviewer/translations/quickviewer_*.ts` file with Qt Linguist or another TS-aware editor.
1. Review and commit the `.ts` changes together with any required project and `languages.ini` updates for a newly added language.

The `quickviewer_*.ts` files are the tracked source of truth for QuickViewer application translations. The normal qmake build runs `lrelease` and generates the corresponding `quickviewer_*.qm` files in the build output. Do not use Qt Linguist's **Release** action to generate QuickViewer `.qm` files for the source tree, and do not commit generated `quickviewer_*.qm` files.

`qt_el.qm` is a special tracked Qt catalog. It is not generated from the QuickViewer application `.ts` files and must not be removed as part of the normal application catalog workflow.

When adding a new language, add its `.ts` file to `TRANSLATIONS` in `apps/quickviewer/QuickViewer.pro` and add it to `languages.ini` as follows.

```
[German]
code=de
caption=German
qm=quickviewer_de.qm
```

You can send the translated `.ts` file to us as an email attachment or submit it with the usual GitHub pull request procedure.
