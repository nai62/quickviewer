#ifndef IMAGESHADEREFFECT_H
#define IMAGESHADEREFFECT_H

#include <QtWidgets>
#include "qvenums.h"

/**
 * @brief The ShaderManager class
 * Tells the pixmap item of each page how to draw the image it holds: the
 * effects the view scales ask for a smooth (or a fast) transformation, and the
 * CPU effects keep the default, because their pages are already resized.
 */
class ShaderManager : public QObject
{
    Q_OBJECT
public:
    ShaderManager(QObject *parent = nullptr);
    /**
     * @brief prepare the transformation mode for one page
     */
    void prepare(QGraphicsPixmapItem *item);
    /**
     * @brief prepareFinished must be called once after all prepare()
     */
    void prepareFinished();
    void prepareInitialize() { m_oldEffect = qvEnums::ShaderEffect::UnPrepared; }

    static QString shaderEffectToString(qvEnums::ShaderEffect effect)
    {
        QMetaEnum metaEnum = QMetaEnum::fromType<qvEnums::ShaderEffect>();
        return metaEnum.valueToKey(static_cast<int>(effect));
    }
    static qvEnums::ShaderEffect stringToShaderEffect(QString effect)
    {
        QMetaEnum metaEnum = QMetaEnum::fromType<qvEnums::ShaderEffect>();
        bool ok = false;
        const int value = metaEnum.keysToValue(effect.toLatin1(), &ok);
        if (!ok) {
            return qvEnums::ShaderEffect::Bilinear;
        }
        return static_cast<qvEnums::ShaderEffect>(value);
    }

private:
    qvEnums::ShaderEffect m_oldEffect;
};

#endif // IMAGESHADEREFFECT_H
