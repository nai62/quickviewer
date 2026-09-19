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

/**
 * Measures one whole decode request, which is the interval that ends with the
 * returned image rather than with a single decoder. It writes the measurement to
 * pipelineNanoseconds and, like DecodeMetricsScope, does nothing without a sink.
 */
template <typename Timer = QElapsedTimer>
class PipelineMetricsScope
{
public:
    explicit PipelineMetricsScope(ImageDecodeMetrics *metrics)
        : m_metrics(metrics)
    {
        if (m_metrics) {
            m_timer.start();
        }
    }

    ~PipelineMetricsScope() { finish(); }

    PipelineMetricsScope(const PipelineMetricsScope &) = delete;
    PipelineMetricsScope &operator=(const PipelineMetricsScope &) = delete;
    PipelineMetricsScope(PipelineMetricsScope &&) = delete;
    PipelineMetricsScope &operator=(PipelineMetricsScope &&) = delete;

    // Destruction still accounts for early exits; repeated calls are inert.
    void finish()
    {
        if (m_metrics && !m_finished) {
            m_metrics->pipelineNanoseconds = m_timer.nsecsElapsed();
            m_finished = true;
        }
    }

private:
    ImageDecodeMetrics *m_metrics;
    Timer m_timer;
    bool m_finished = false;
};

} // namespace ImageDecodeDetail

#endif // DECODEMETRICSSCOPE_H
