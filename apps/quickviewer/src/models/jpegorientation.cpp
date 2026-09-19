#include "jpegorientation.h"

#include <cstring>

int jpegExifOrientation(const QByteArray &bytes)
{
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const qsizetype size = bytes.size();
    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return 1;
    }

    auto readBigEndian16 = [](const unsigned char *p) -> quint16 {
        return static_cast<quint16>((p[0] << 8) | p[1]);
    };
    qsizetype offset = 2;
    while (offset + 4 <= size) {
        if (data[offset] != 0xFF) {
            ++offset;
            continue;
        }
        while (offset < size && data[offset] == 0xFF) {
            ++offset;
        }
        if (offset >= size) {
            break;
        }
        const unsigned char marker = data[offset++];
        if (marker == 0xD9 || marker == 0xDA) {
            break;
        }
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            continue;
        }
        if (offset + 2 > size) {
            break;
        }
        const quint16 segmentLength = readBigEndian16(data + offset);
        if (segmentLength < 2 || offset + segmentLength > size) {
            break;
        }
        const unsigned char *payload = data + offset + 2;
        const qsizetype payloadSize = segmentLength - 2;
        if (marker == 0xE1 && payloadSize >= 14 && std::memcmp(payload, "Exif\0\0", 6) == 0) {
            const unsigned char *tiff = payload + 6;
            const qsizetype tiffSize = payloadSize - 6;
            const bool littleEndian = tiffSize >= 8 && tiff[0] == 'I' && tiff[1] == 'I';
            const bool bigEndian = tiffSize >= 8 && tiff[0] == 'M' && tiff[1] == 'M';
            if (!littleEndian && !bigEndian) {
                return 1;
            }
            auto read16 = [littleEndian](const unsigned char *p) -> quint16 {
                return littleEndian ? static_cast<quint16>(p[0] | (p[1] << 8))
                                    : static_cast<quint16>((p[0] << 8) | p[1]);
            };
            auto read32 = [littleEndian](const unsigned char *p) -> quint32 {
                return littleEndian
                           ? static_cast<quint32>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24))
                           : static_cast<quint32>((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
            };
            if (read16(tiff + 2) != 42) {
                return 1;
            }
            // The offset is a 32-bit field, so the bounds check has to widen it
            // before adding: near 4 GiB it used to wrap and read past the segment.
            const qint64 ifdOffset = read32(tiff + 4);
            if (ifdOffset + 2 > tiffSize) {
                return 1;
            }
            const unsigned char *ifd = tiff + ifdOffset;
            const quint16 entryCount = read16(ifd);
            for (quint16 i = 0; i < entryCount; ++i) {
                const qsizetype entryOffset = 2 + static_cast<qsizetype>(i) * 12;
                if (ifdOffset + entryOffset + 12 > tiffSize) {
                    break;
                }
                const unsigned char *entry = ifd + entryOffset;
                if (read16(entry) == 0x0112 && read16(entry + 2) == 3 && read32(entry + 4) == 1) {
                    const int orientation = read16(entry + 8);
                    return orientation >= 1 && orientation <= 8 ? orientation : 1;
                }
            }
            return 1;
        }
        offset += segmentLength;
    }
    return 1;
}
