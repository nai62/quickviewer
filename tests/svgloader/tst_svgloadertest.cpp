#include <QtTest>

#include "svgloader.h"

class SvgLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void storageValuesAreIndependentOfDisplayLabels();
    void rasterDimensionsAreValidated();
    void rasterSizeFitsWithinBothLimits();
    void rendersWithResvg();
    void parseFailureFallsBackToQtSvg();
#ifdef Q_OS_WIN
    void rendersJapaneseTextWithResvg();
#endif
};

void SvgLoaderTest::storageValuesAreIndependentOfDisplayLabels()
{
    QCOMPARE(SvgLoader::storageValue(qvEnums::Resvg), QString("resvg"));
    QCOMPARE(SvgLoader::storageValue(qvEnums::QtSvg), QString("qtsvg"));
    QCOMPARE(SvgLoader::backendFromStorageValue("resvg"), qvEnums::Resvg);
    QCOMPARE(SvgLoader::backendFromStorageValue("qtsvg"), qvEnums::QtSvg);
    QCOMPARE(SvgLoader::backendFromStorageValue("imageformat"), qvEnums::Resvg);
    QCOMPARE(SvgLoader::backendFromStorageValue("svg-native-loader"), qvEnums::Resvg);
}

void SvgLoaderTest::rasterDimensionsAreValidated()
{
    QCOMPARE(SvgLoader::validatedRasterDimension(100, 1920), 100);
    QCOMPARE(SvgLoader::validatedRasterDimension(10000, 1920), 10000);
    QCOMPARE(SvgLoader::validatedRasterDimension(99, 1920), 1920);
    QCOMPARE(SvgLoader::validatedRasterDimension(10001, 1080), 1080);
}

void SvgLoaderTest::rasterSizeFitsWithinBothLimits()
{
    QCOMPARE(
        SvgLoader::fittedRasterSize(QSizeF(4000, 3000), QSize(1920, 1080)),
        QSize(1440, 1080));
    QCOMPARE(
        SvgLoader::fittedRasterSize(QSizeF(100, 200), QSize(1920, 1080)),
        QSize(540, 1080));
}

void SvgLoaderTest::rendersWithResvg()
{
    const QByteArray svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="400" height="300">
        <rect width="400" height="300" fill="#4080c0"/>
    </svg>)";
    const SvgLoader::RenderResult result = SvgLoader::render(
        svg, QString(), QSize(1920, 1080), qvEnums::Resvg);

    QCOMPARE(result.backend, qvEnums::Resvg);
    QVERIFY2(result.resvgError.isEmpty(), qPrintable(result.resvgError));
    QCOMPARE(result.image.size(), QSize(1440, 1080));
    QVERIFY(!result.image.isNull());
}

void SvgLoaderTest::parseFailureFallsBackToQtSvg()
{
    const SvgLoader::RenderResult result = SvgLoader::render(
        QByteArray("not an svg"), QString(), QSize(1920, 1080), qvEnums::Resvg);

    QCOMPARE(result.backend, qvEnums::QtSvg);
    QVERIFY(!result.resvgError.isEmpty());
    QVERIFY(result.image.isNull());
}

#ifdef Q_OS_WIN
void SvgLoaderTest::rendersJapaneseTextWithResvg()
{
    const QByteArray svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="300" height="100">
        <text x="10" y="65" font-family="Yu Gothic" font-size="52">&#x6F22;&#x5B57;</text>
    </svg>)";
    const SvgLoader::RenderResult result = SvgLoader::render(
        svg, QString(), QSize(300, 100), qvEnums::Resvg);

    QCOMPARE(result.backend, qvEnums::Resvg);
    QVERIFY2(result.resvgError.isEmpty(), qPrintable(result.resvgError));
    QVERIFY(!result.image.isNull());
    int opaquePixels = 0;
    for (int y = 0; y < result.image.height(); ++y) {
        for (int x = 0; x < result.image.width(); ++x) {
            opaquePixels += qAlpha(result.image.pixel(x, y)) > 0 ? 1 : 0;
        }
    }
    QVERIFY(opaquePixels > 100);
}
#endif

QTEST_GUILESS_MAIN(SvgLoaderTest)

#include "tst_svgloadertest.moc"
