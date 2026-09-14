#ifndef IMAGEBENCHMARKRUNNER_H
#define IMAGEBENCHMARKRUNNER_H

#include <QtCore>

class ImageBenchmarkRunner
{
public:
    static bool isRequested(const QStringList &arguments);
    static void applyStartupOverrides();
    static int run(const QStringList &arguments);
};

#endif // IMAGEBENCHMARKRUNNER_H
