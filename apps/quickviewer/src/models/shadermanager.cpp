#include <QGraphicsPixmapItem>
#include "shadermanager.h"
#include "qvapplication.h"

ShaderManager::ShaderManager(QObject *parent)
    : QObject(parent),
      m_oldEffect(qvEnums::ShaderEffect::Bilinear)
{
}

void ShaderManager::prepare(QGraphicsPixmapItem *item)
{
    if (!item) {
        return;
    }
    switch (qApp->Effect()) {
    default:
        break;
    case qvEnums::ShaderEffect::CpuBicubic:
    case qvEnums::ShaderEffect::CpuSpline16:
    case qvEnums::ShaderEffect::CpuSpline36:
    case qvEnums::ShaderEffect::CpuLanczos3:
    case qvEnums::ShaderEffect::CpuLanczos4:
    case qvEnums::ShaderEffect::Bilinear:
        item->setTransformationMode(Qt::SmoothTransformation);
        break;
    case qvEnums::ShaderEffect::NearestNeighbor:
        if (m_oldEffect != qvEnums::ShaderEffect::NearestNeighbor) {
            item->setTransformationMode(Qt::FastTransformation);
        }
        break;
    }
}

void ShaderManager::prepareFinished()
{
    m_oldEffect = qApp->Effect();
}
