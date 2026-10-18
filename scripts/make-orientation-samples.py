#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "pillow==12.3.0",
#     "pillow-heif==1.8.1",
# ]
# ///
"""Generate EXIF orientation sample images for interactive checks.

Every file of a set looks the same when a viewer honours its orientation: the
pixels of orientation N are stored turned by the inverse of N, so a viewer that
ignores or misreads the tag shows that file differently from the others. Each
file is read back and compared before the script moves on.

The source picture is the mandrill (USC-SIPI 4.2.03), for example OpenCV's
samples/data/baboon.jpg. The output directory gets a README.txt that lists what
each folder checks. Do not commit the generated images.
"""

from __future__ import annotations

import argparse
import shutil
import zipfile
from pathlib import Path

import pillow_heif
from PIL import Image, ImageChops, ImageDraw, ImageFont, ImageOps, ImageStat

pillow_heif.register_heif_opener()

ORIENTATION_TAG = 0x0112

# Transpose that turns the upright picture into the stored pixels of each
# orientation: the inverse of what the tag asks a viewer to apply.
STORE_TRANSPOSE = {
    1: None,
    2: Image.Transpose.FLIP_LEFT_RIGHT,
    3: Image.Transpose.ROTATE_180,
    4: Image.Transpose.FLIP_TOP_BOTTOM,
    5: Image.Transpose.TRANSPOSE,
    6: Image.Transpose.ROTATE_90,  # counterclockwise; the tag turns it back clockwise
    7: Image.Transpose.TRANSVERSE,
    8: Image.Transpose.ROTATE_270,
}

NAMES = {
    1: "normal",
    2: "mirror horizontal",
    3: "rotate 180",
    4: "mirror vertical",
    5: "transpose",
    6: "rotate 90 CW",
    7: "transverse",
    8: "rotate 270 CW",
}


def font(size: int) -> ImageFont.ImageFont:
    return ImageFont.load_default(size=size)


def upright_picture(mandrill: Image.Image, size: tuple[int, int], title: str) -> Image.Image:
    """The picture as it has to be displayed: asymmetric, so turns and mirrors show."""
    width, height = size
    canvas = Image.new("RGB", size, (40, 40, 48))
    draw = ImageDraw.Draw(canvas)
    unit = min(width, height)

    # The mandrill fills the square at the start of the long side.
    face = mandrill.resize((unit, unit), Image.Resampling.LANCZOS)
    canvas.paste(face, (0, 0))

    # Markers: red bar along the top edge, blue block in the top-left corner.
    bar = max(4, unit // 40)
    draw.rectangle((0, 0, width - 1, bar - 1), fill=(220, 30, 30))
    draw.rectangle((0, 0, unit // 6, unit // 6), fill=(30, 80, 230))

    # The rest of the long side holds the label and an arrow pointing up.
    if width >= height:
        panel = (unit, 0, width, height)
    else:
        panel = (0, unit, width, height)
    left, top, right, bottom = panel
    panel_width = right - left
    panel_height = bottom - top
    cx = left + panel_width // 2
    arrow = min(panel_width, panel_height) // 3
    ay = top + bar + arrow // 6
    draw.polygon(
        [(cx, ay), (cx - arrow // 2, ay + arrow // 2), (cx + arrow // 2, ay + arrow // 2)],
        fill=(250, 210, 40),
    )
    draw.rectangle(
        (cx - arrow // 8, ay + arrow // 2, cx + arrow // 8, ay + arrow), fill=(250, 210, 40)
    )
    draw.text(
        (cx, ay + arrow + arrow // 6),
        "UP",
        font=font(max(12, arrow // 3)),
        fill="white",
        anchor="mt",
    )
    # Shrink the label until it fits the panel with a margin.
    size = max(10, panel_height // 10)
    while size > 10:
        box = draw.multiline_textbbox((0, 0), title, font=font(size), align="center")
        if box[2] - box[0] <= panel_width * 0.9:
            break
        size -= 2
    draw.multiline_text(
        (cx, bottom - bar * 3), title, font=font(size), fill="white", anchor="md", align="center"
    )
    return canvas


def stored_pixels(upright: Image.Image, orientation: int) -> Image.Image:
    transpose = STORE_TRANSPOSE[orientation]
    return upright if transpose is None else upright.transpose(transpose)


def orientation_exif(orientation: int) -> bytes:
    exif = Image.Exif()
    exif[ORIENTATION_TAG] = orientation
    return exif.tobytes()


def save(image: Image.Image, path: Path, orientation: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    suffix = path.suffix.lower()
    if suffix == ".jpg":
        image.save(path, quality=92, exif=orientation_exif(orientation))
    elif suffix == ".tif":
        image.save(path, compression="tiff_lzw", tiffinfo={ORIENTATION_TAG: orientation})
    elif suffix == ".png":
        image.save(path, exif=orientation_exif(orientation))
    elif suffix == ".webp":
        image.save(path, quality=92, exif=orientation_exif(orientation))
    elif suffix == ".heic":
        # pillow-heif hands the EXIF orientation to libheif, which writes it as
        # irot/imir boxes and keeps the EXIF tag, the way an iPhone does.
        image.save(path, format="HEIF", quality=92, exif=orientation_exif(orientation))
    else:
        raise ValueError(f"unknown format: {path}")


def check(path: Path, upright: Image.Image, orientation: int) -> None:
    """Reads the file back the way a conforming viewer does and compares."""
    if path.suffix.lower() == ".heic":
        # libheif applies irot/imir while decoding, and the EXIF tag is ignored.
        data = path.read_bytes()
        if orientation != 1 and b"irot" not in data and b"imir" not in data:
            raise SystemExit(f"{path}: no irot/imir box for orientation {orientation}")
        with Image.open(path) as written:
            read_orientation = written.info.get("original_orientation") or 1
            shown = written.convert("RGB")
    else:
        with Image.open(path) as written:
            read_orientation = written.getexif().get(ORIENTATION_TAG, 1)
            shown = ImageOps.exif_transpose(written).convert("RGB")
    if read_orientation != orientation:
        raise SystemExit(f"{path}: orientation tag reads {read_orientation}, wrote {orientation}")
    if shown.size != upright.size:
        raise SystemExit(f"{path}: displays as {shown.size}, expected {upright.size}")
    difference = max(ImageStat.Stat(ImageChops.difference(shown, upright)).mean)
    if difference > 4.0:
        raise SystemExit(f"{path}: differs from the upright picture by {difference:.1f}")


def write_set(
    mandrill: Image.Image,
    out: Path,
    folder: str,
    prefix: str,
    size: tuple[int, int],
    suffix: str,
    orientations: range | list[int],
) -> list[Path]:
    paths = []
    for orientation in orientations:
        label = f"{orientation}: {NAMES[orientation]}\n{suffix[1:].upper()} {size[0]}x{size[1]}"
        upright = upright_picture(mandrill, size, label)
        path = out / folder / f"{prefix}_{orientation}{suffix}"
        save(stored_pixels(upright, orientation), path, orientation)
        check(path, upright, orientation)
        paths.append(path)
    return paths


README = """EXIF orientation samples for QuickViewer
=========================================

Within each folder every file must look the same: the mandrill at the top-left
(landscape) or top (portrait), a red bar along the top edge, a blue block in the
top-left corner, a yellow arrow pointing UP, and readable (not mirrored) text.
A file that looks different is drawn with the wrong orientation. Compare with
the Explorer thumbnails, which honour the tag for JPEG.

Files are named <set>_<EXIF orientation>.<ext>. The pixels of orientation N are
stored turned by the inverse of N.

jpeg-landscape/   1200x800 displayed, orientations 1-8
jpeg-portrait/    800x1200 displayed, orientations 1-8 (stored sideways for 5-8)
                  Use for dual view pairing and "separate wide pages": these
                  must be treated as portrait pages and never split.
jpeg-large/       6000x4000 displayed, orientations 1-8
                  Larger than the viewport and the usual maximum texture size:
                  check the first (preview) paint is upright and sharp, manual
                  zoom scrolling, and user rotation combined with the tag.
tiff-landscape/   1200x800 displayed TIFF, orientations 1-8 (Qt TIFF plug-in)
heic-landscape/   1200x800 and 800x1200 displayed HEIC, orientations 1-8, laid
heic-portrait/    out like iPhone photos: stored pixels, irot/imir boxes, and
                  the same EXIF orientation. libheif applies the boxes while
                  decoding and the EXIF tag must be ignored, so a file turned
                  twice means the tag was applied on top of the boxes.
unsupported/      PNG (eXIf) and WebP (EXIF) tagged 6. QuickViewer does not
                  read these tags, so they are expected to appear turned
                  (arrow pointing left).
catalog/          Library for a catalog: "Folder book" holds a portrait page
                  tagged 6, "Zip book.zip" holds the same page in an archive.
                  Both covers must be upright portraits.

Other checks: copy a tagged page and paste it elsewhere (must be upright); the
delete/trash confirmation icon (must be upright).
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("mandrill", type=Path, help="source picture (the mandrill)")
    parser.add_argument("output", type=Path, help="directory to write the samples into")
    args = parser.parse_args()
    out = args.output
    with Image.open(args.mandrill) as image:
        mandrill = image.convert("RGB")
    out.mkdir(parents=True, exist_ok=True)

    all_orientations = range(1, 9)
    write_set(mandrill, out, "jpeg-landscape", "Landscape", (1200, 800), ".jpg", all_orientations)
    portraits = write_set(
        mandrill, out, "jpeg-portrait", "Portrait", (800, 1200), ".jpg", all_orientations
    )
    write_set(mandrill, out, "jpeg-large", "Large", (6000, 4000), ".jpg", all_orientations)
    write_set(mandrill, out, "tiff-landscape", "Landscape", (1200, 800), ".tif", all_orientations)
    write_set(mandrill, out, "heic-landscape", "Landscape", (1200, 800), ".heic", all_orientations)
    write_set(mandrill, out, "heic-portrait", "Portrait", (800, 1200), ".heic", all_orientations)
    write_set(mandrill, out, "unsupported", "Png", (1200, 800), ".png", [6])
    write_set(mandrill, out, "unsupported", "Webp", (1200, 800), ".webp", [6])

    portrait_6 = next(path for path in portraits if path.stem.endswith("_6"))
    folder_book = out / "catalog" / "Folder book"
    folder_book.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(portrait_6, folder_book / "01.jpg")
    with zipfile.ZipFile(out / "catalog" / "Zip book.zip", "w", zipfile.ZIP_STORED) as archive:
        archive.write(portrait_6, "01.jpg")

    (out / "README.txt").write_text(README, encoding="utf-8")
    print(f"Wrote samples to {out}")


if __name__ == "__main__":
    main()
