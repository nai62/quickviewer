#include "shadereffect.h"

ShaderEffectKind shaderEffectKind(qvEnums::ShaderEffect effect)
{
    switch (effect) {
    case qvEnums::ShaderEffect::UnPrepared:
        return ShaderEffectKind::Unprepared;
    case qvEnums::ShaderEffect::CpuBicubic:
    case qvEnums::ShaderEffect::CpuSpline16:
    case qvEnums::ShaderEffect::CpuSpline36:
    case qvEnums::ShaderEffect::CpuLanczos3:
    case qvEnums::ShaderEffect::CpuLanczos4:
        return ShaderEffectKind::CpuOnly;
    case qvEnums::ShaderEffect::NearestNeighbor:
    case qvEnums::ShaderEffect::Bilinear:
        return ShaderEffectKind::ViewScaled;
    }
    return ShaderEffectKind::Unprepared;
}

bool scalesInView(qvEnums::ShaderEffect effect)
{
    return shaderEffectKind(effect) == ShaderEffectKind::ViewScaled;
}

bool resizesOnCpu(qvEnums::ShaderEffect effect)
{
    const ShaderEffectKind kind = shaderEffectKind(effect);
    return kind == ShaderEffectKind::Unprepared || kind == ShaderEffectKind::CpuOnly;
}

QZimg::FilterMode cpuFilterMode(qvEnums::ShaderEffect effect)
{
    switch (effect) {
    case qvEnums::ShaderEffect::CpuBicubic:
        return QZimg::ResizeBicubic;
    case qvEnums::ShaderEffect::CpuSpline16:
        return QZimg::ResizeSpline16;
    case qvEnums::ShaderEffect::CpuSpline36:
        return QZimg::ResizeSpline36;
    case qvEnums::ShaderEffect::CpuLanczos3:
        return QZimg::ResizeLanczos3;
    case qvEnums::ShaderEffect::CpuLanczos4:
        return QZimg::ResizeLanczos4;
    default:
        return QZimg::ResizeBicubic;
    }
}
