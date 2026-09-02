#include <QtCore>
#include "fileoperator.h"

FileOperator::FileOperator(QString json, QObject *parent)
    : QObject(parent),
      m_operateAs(OperateAs::AsFile),
      m_mode(OperateMode::CopyMode)
{
    if (json.isEmpty()) {
        return;
    }
    QJsonDocument docc = QJsonDocument::fromVariant(json);
    QJsonObject doc = docc.object();
    QJsonValue as = doc["as"];
    if (as.isString()) {
        m_operateAs = as.toString() == "file" ? OperateAs::AsFile : OperateAs::AsVolume;
    }
    QJsonValue mode = doc["mode"];
    if (mode.isString()) {
        const QString modeName = mode.toString();
        if (modeName == "copy") {
            m_mode = OperateMode::CopyMode;
        } else if (modeName == "move") {
            m_mode = OperateMode::MoveMode;
        } else {
            m_mode = OperateMode::CommandMode;
        }
    }
    QJsonValue command = doc["command"];
    if (command.isString()) {
        m_command = command.toString();
    }
}

QVariant FileOperator::toVariant()
{
    QJsonObject obj;
    obj["as"] = m_operateAs == OperateAs::AsFile ? "file" : "volume";
    QString modeName = "command";
    if (m_mode == OperateMode::CopyMode) {
        modeName = "copy";
    } else if (m_mode == OperateMode::MoveMode) {
        modeName = "move";
    }
    obj["mode"] = modeName;
    obj["command"] = m_command;
    QJsonDocument doc = QJsonDocument(obj);
    return doc.toVariant();
}
