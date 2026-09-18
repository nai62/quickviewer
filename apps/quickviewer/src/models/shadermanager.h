#ifndef IMAGESHADEREFFECT_H
#define IMAGESHADEREFFECT_H

#include <QtWidgets>
#include "qvenums.h"

struct ImageContent;

/**
 * @brief The ShaderManager class
 * customizing the shader effec oft QGraphicsPixmapItem,
 * which can use a fragment shader
 */
class ShaderManager : public QObject
{
    Q_OBJECT
public:
    ShaderManager(QObject *parent = nullptr);
    /**
     * @brief prepare shader for each page
     * @param ic
     */
    void prepare(QGraphicsPixmapItem *item, const ImageContent &ic, QSize size);
    /**
     * @brief prepareFinished must be called once after all prepare()
     */
    void prepareFinished();
    void prepareInitialize()
    {
        m_oldEffect = qvEnums::UnPrepared;
        pageCnt = 0;
    }

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
        return ok ? static_cast<qvEnums::ShaderEffect>(value) : qvEnums::Bilinear;
    }

private:
    void loadShader(QByteArray &target, QString path);

    qvEnums::ShaderEffect m_oldEffect;
    int pageCnt;
    QByteArray m_bicubic;
    QByteArray m_lanczos;
};

#endif // IMAGESHADEREFFECT_H
