#ifndef IMAGEBENCHMARKRUNNER_H
#define IMAGEBENCHMARKRUNNER_H

#include <QtCore>

class ImageBenchmarkRunner
{
public:
    static bool isRequested(const QStringList &arguments);
    /**
     * True in the child process of the empty-window suite, which measures a
     * bare Qt window instead of QuickViewer's startup.
     */
    static bool isEmptyWindowChildRequested();
    static void applyStartupOverrides();
    static int run(const QStringList &arguments);
};

#endif // IMAGEBENCHMARKRUNNER_H
