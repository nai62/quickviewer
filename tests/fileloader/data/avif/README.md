# AVIF fixtures

`red.avif` is a generated 12 by 8 red still image (no external artwork).
It was created with FFmpeg's libaom encoder:

```sh
ffmpeg -f lavfi -i color=c=red:s=12x8 -frames:v 1 -c:v libaom-av1 -still-picture 1 -crf 0 red.avif
```

`images.zip` contains the same bytes as `001.avif` and `002.AVIF`, plus
`notes.txt`, to exercise archive filtering and extraction.
