#ifndef SHADEREFFECT_H
#define SHADEREFFECT_H

#include "qzimg.h"
#include "qvenums.h"

/**
 * What a shader effect does to an image. The effects are classified by
 * behaviour instead of by their position in the enumeration.
 */
enum class ShaderEffectKind {
    Unprepared, // no effect chosen yet
    CpuOnly,    // resized by the CPU
    ViewScaled, // drawn at its own size and scaled by the view
};

/**
 * Classifies effect.
 */
ShaderEffectKind shaderEffectKind(qvEnums::ShaderEffect effect);

/**
 * True when the view scales the image instead of the CPU resizing it.
 */
bool scalesInView(qvEnums::ShaderEffect effect);

/**
 * True when the image is resized by the CPU.
 */
bool resizesOnCpu(qvEnums::ShaderEffect effect);

/**
 * Filter the CPU resize uses for effect.
 */
QZimg::FilterMode cpuFilterMode(qvEnums::ShaderEffect effect);

#endif // SHADEREFFECT_H
