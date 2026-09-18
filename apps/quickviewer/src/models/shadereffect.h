#ifndef SHADEREFFECT_H
#define SHADEREFFECT_H

#include "qzimg.h"
#include "qvenums.h"

/**
 * What a shader effect does to an image. The effects are classified by
 * behaviour instead of by their position in the enumeration.
 */
enum class ShaderEffectKind {
    Unprepared,            // no effect chosen yet
    CpuOnly,               // resized by the CPU
    FixedShader,           // scaled by the fixed function GPU pipeline
    CpuResizeAfterPreview, // GPU preview, replaced by a CPU resize
    GlShader,              // scaled by a fragment shader
};

/**
 * Classifies effect. The marker values of the enumeration are not effects;
 * they are reported as Unprepared.
 */
ShaderEffectKind shaderEffectKind(qvEnums::ShaderEffect effect);

/**
 * True when the scaling happens on the GPU, so the OpenGL renderer is needed.
 */
bool usesGpuRendering(qvEnums::ShaderEffect effect);

/**
 * True when the image is resized by the CPU.
 */
bool resizesOnCpu(qvEnums::ShaderEffect effect);

/**
 * Filter the CPU resize uses for effect.
 */
QZimg::FilterMode cpuFilterMode(qvEnums::ShaderEffect effect);

#endif // SHADEREFFECT_H
