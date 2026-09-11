#ifndef STARTUPPROFILER_H
#define STARTUPPROFILER_H

class StartupProfiler
{
public:
    static void start();
    static bool enabled();
    static void mark(const char *label);
    static void flush();
};

#endif // STARTUPPROFILER_H
