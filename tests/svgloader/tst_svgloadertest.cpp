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
    void rendersTransformedNestedSvg();
    void parseFailureFallsBackToQtSvg_data();
    void parseFailureFallsBackToQtSvg();
#ifdef Q_OS_WIN
    void rendersJapaneseTextWithResvg();
#endif
};

void SvgLoaderTest::storageValuesAreIndependentOfDisplayLabels()
{
    QCOMPARE(SvgLoader::storageValue(qvEnums::SvgLoaderBackend::Resvg), QString("resvg"));
    QCOMPARE(SvgLoader::storageValue(qvEnums::SvgLoaderBackend::QtSvg), QString("qtsvg"));
    QCOMPARE(SvgLoader::backendFromStorageValue("resvg"), qvEnums::SvgLoaderBackend::Resvg);
    QCOMPARE(SvgLoader::backendFromStorageValue("qtsvg"), qvEnums::SvgLoaderBackend::QtSvg);
    QCOMPARE(SvgLoader::backendFromStorageValue("imageformat"), qvEnums::SvgLoaderBackend::Resvg);
    QCOMPARE(SvgLoader::backendFromStorageValue("svg-native-loader"),
             qvEnums::SvgLoaderBackend::Resvg);
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
    QCOMPARE(SvgLoader::fittedRasterSize(QSizeF(4000, 3000), QSize(1920, 1080)), QSize(1440, 1080));
    QCOMPARE(SvgLoader::fittedRasterSize(QSizeF(100, 200), QSize(1920, 1080)), QSize(540, 1080));
}

void SvgLoaderTest::rendersWithResvg()
{
    const QByteArray svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="400" height="300">
        <rect width="400" height="300" fill="#4080c0"/>
    </svg>)";
    const SvgLoader::RenderResult result =
        SvgLoader::render(svg, QString(), QSize(1920, 1080), qvEnums::SvgLoaderBackend::Resvg);

    QCOMPARE(result.backend, qvEnums::SvgLoaderBackend::Resvg);
    QVERIFY2(result.resvgError.isEmpty(), qPrintable(result.resvgError));
    QCOMPARE(result.image.size(), QSize(1440, 1080));
    QVERIFY(!result.image.isNull());
}

void SvgLoaderTest::rendersTransformedNestedSvg()
{
    const QByteArray svg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="20" height="10">
        <svg width="10" height="10" transform="translate(5 0)">
            <rect width="10" height="10" fill="red"/>
        </svg>
    </svg>)svg";
    const SvgLoader::RenderResult result =
        SvgLoader::render(svg, QString(), QSize(20, 10), qvEnums::SvgLoaderBackend::Resvg);

    QCOMPARE(result.backend, qvEnums::SvgLoaderBackend::Resvg);
    QVERIFY2(result.resvgError.isEmpty(), qPrintable(result.resvgError));
    QCOMPARE(result.image.size(), QSize(20, 10));
    QCOMPARE(qAlpha(result.image.pixel(2, 5)), 0);
    QCOMPARE(result.image.pixelColor(7, 5), QColor(Qt::red));
    QCOMPARE(qAlpha(result.image.pixel(17, 5)), 0);
}

void SvgLoaderTest::parseFailureFallsBackToQtSvg_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<QString>("error");
    QTest::newRow("invalid-xml") << QByteArray("not an svg")
                                 << QStringLiteral("Failed to parse the SVG data");
    QTest::newRow("invalid-size")
        << QByteArray(R"(<svg xmlns="http://www.w3.org/2000/svg" width="0" height="0"/>)")
        << QStringLiteral("The SVG has an invalid size");
    QTest::newRow("malformed-gzip")
        << QByteArray::fromHex("1f8b0800") << QStringLiteral("The SVGZ data is malformed");
}

void SvgLoaderTest::parseFailureFallsBackToQtSvg()
{
    QFETCH(QByteArray, data);
    QFETCH(QString, error);
    const SvgLoader::RenderResult result =
        SvgLoader::render(data, QString(), QSize(1920, 1080), qvEnums::SvgLoaderBackend::Resvg);

    QCOMPARE(result.backend, qvEnums::SvgLoaderBackend::QtSvg);
    QCOMPARE(result.resvgError, error);
    QVERIFY(result.image.isNull());
}

#ifdef Q_OS_WIN
void SvgLoaderTest::rendersJapaneseTextWithResvg()
{
    const QByteArray svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="300" height="100">
        <text x="10" y="65" font-family="Yu Gothic" font-size="52">&#x6F22;&#x5B57;</text>
    </svg>)";
    const SvgLoader::RenderResult result =
        SvgLoader::render(svg, QString(), QSize(300, 100), qvEnums::SvgLoaderBackend::Resvg);

    QCOMPARE(result.backend, qvEnums::SvgLoaderBackend::Resvg);
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
