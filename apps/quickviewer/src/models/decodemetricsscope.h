#ifndef DECODEMETRICSSCOPE_H
#define DECODEMETRICSSCOPE_H

#include "imageloadmetrics.h"

#include <utility>

namespace ImageDecodeDetail {

// Timer is replaceable so accumulation and disabled instrumentation can be
// tested without sleeps or wall-clock assertions.
template <typename Timer = QElapsedTimer>
class DecodeMetricsScope
{
public:
    explicit DecodeMetricsScope(ImageDecodeMetrics *metrics)
        : m_metrics(metrics)
    {
        if (m_metrics) {
            m_timer.start();
        }
    }

    ~DecodeMetricsScope() { finish(); }

    DecodeMetricsScope(const DecodeMetricsScope &) = delete;
    DecodeMetricsScope &operator=(const DecodeMetricsScope &) = delete;
    DecodeMetricsScope(DecodeMetricsScope &&) = delete;
    DecodeMetricsScope &operator=(DecodeMetricsScope &&) = delete;

    // End the decode interval before moving images or preparing display data.
    // Destruction still accounts for early exits; repeated finish calls are inert.
    void finish()
    {
        if (m_metrics && !m_finished) {
            m_metrics->decodeNanoseconds += m_timer.nsecsElapsed();
            m_finished = true;
        }
    }

    template <typename BackendFactory>
    void recordBackend(BackendFactory &&backend)
    {
        if (m_metrics) {
            m_metrics->decoderBackend = std::forward<BackendFactory>(backend)();
        }
    }

    template <typename BackendFactory>
    void recordBackendOnSuccess(bool succeeded, BackendFactory &&backend)
    {
        if (succeeded) {
            recordBackend(std::forward<BackendFactory>(backend));
        }
    }

private:
    ImageDecodeMetrics *m_metrics;
    Timer m_timer;
    bool m_finished = false;
};

} // namespace ImageDecodeDetail

#endif // DECODEMETRICSSCOPE_H
