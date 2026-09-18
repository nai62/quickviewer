#include "shadereffect.h"

ShaderEffectKind shaderEffectKind(qvEnums::ShaderEffect effect)
{
    switch (effect) {
    case qvEnums::UnPrepared:
        return ShaderEffectKind::Unprepared;
    case qvEnums::CpuBicubic:
    case qvEnums::CpuSpline16:
    case qvEnums::CpuSpline36:
    case qvEnums::CpuLanczos3:
    case qvEnums::CpuLanczos4:
        return ShaderEffectKind::CpuOnly;
    case qvEnums::NearestNeighbor:
    case qvEnums::Bilinear:
        return ShaderEffectKind::FixedShader;
    case qvEnums::BilinearAndCpuBicubic:
    case qvEnums::BilinearAndCpuSpline16:
    case qvEnums::BilinearAndCpuSpline36:
    case qvEnums::BilinearAndCpuLanczos:
        return ShaderEffectKind::CpuResizeAfterPreview;
#ifndef QV_WITHOUT_OPENGL
    case qvEnums::Bicubic:
    case qvEnums::Lanczos:
        return ShaderEffectKind::GlShader;
#endif
    case qvEnums::UsingFixedShader:
    case qvEnums::UsingCpuResizer:
    case qvEnums::UsingSomeShader:
        break;
    }
    return ShaderEffectKind::Unprepared;
}

bool usesGpuRendering(qvEnums::ShaderEffect effect)
{
    const ShaderEffectKind kind = shaderEffectKind(effect);
    return kind == ShaderEffectKind::FixedShader || kind == ShaderEffectKind::CpuResizeAfterPreview || kind == ShaderEffectKind::GlShader;
}

bool resizesOnCpu(qvEnums::ShaderEffect effect)
{
    const ShaderEffectKind kind = shaderEffectKind(effect);
    return kind == ShaderEffectKind::Unprepared || kind == ShaderEffectKind::CpuOnly;
}

QZimg::FilterMode cpuFilterMode(qvEnums::ShaderEffect effect)
{
    switch (effect) {
    case qvEnums::CpuBicubic:
    case qvEnums::BilinearAndCpuBicubic:
        return QZimg::ResizeBicubic;
    case qvEnums::CpuSpline16:
    case qvEnums::BilinearAndCpuSpline16:
        return QZimg::ResizeSpline16;
    case qvEnums::CpuSpline36:
    case qvEnums::BilinearAndCpuSpline36:
        return QZimg::ResizeSpline36;
    case qvEnums::CpuLanczos3:
    case qvEnums::BilinearAndCpuLanczos:
        return QZimg::ResizeLanczos3;
    case qvEnums::CpuLanczos4:
        return QZimg::ResizeLanczos4;
    default:
        return QZimg::ResizeBicubic;
    }
}
