#include "dumpmanager.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QDataStream>

DumpManager::DumpManager(QObject *parent)
    : QObject(parent)
    , m_defaultDirectory("dumps")
{
    QDir().mkpath(m_defaultDirectory);
}

bool DumpManager::saveToJSON(const QVector<uint16_t> &buffer, const int &bIndex, const int &dumpSize, const QString &filePath)
{
    if (dumpSize <= 0 || dumpSize > buffer.size()) {
        qDebug() << "saveToJson: неверный dumpSize" << dumpSize
                 << "buffer.size =" << buffer.size();
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Не удалось открыть файл:" << file.errorString();
        return false;
    }

    QJsonArray values;

    const int N = buffer.size();
    const int start = ((bIndex - dumpSize) % N + N) % N;

    for (int i = 0; i < dumpSize; ++i) {
        const int index = (start + i) % N;
        values.append(static_cast<int>(buffer[index]));
    }

    const QJsonDocument doc(values);
    const QByteArray bytes = doc.toJson(QJsonDocument::Compact);
    file.write(bytes);
    file.close();
    return true;
}

bool DumpManager::saveToCSV(const QVector<uint16_t> &buffer, const int &bIndex, const int &dumpSize, const QString &filePath)
{
    if (dumpSize <= 0 || dumpSize > buffer.size()) {
        qDebug() << "saveToCsv: неверный dumpSize" << dumpSize
                 << "buffer.size =" << buffer.size();
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Не удалось открыть файл:" << file.errorString();
        return false;
    }

    QTextStream out(&file);
    out.setLocale(QLocale::C);

    out << "value\n";

    const int N = buffer.size();
    int start = ((bIndex - dumpSize) % N + N) % N;

    for (int i = 0; i < dumpSize; ++i) {
        int index = (start + i) % N;
        out << buffer[index] << "\n";
    }

    out.flush();
    file.close();
    return true;
}


bool DumpManager::saveToDat(const QVector<uint16_t> &buffer, const int &bIndex, const int &dumpSize, const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Не удалось открыть файл";
        return false;
    }

    uint32_t sizeToWrite = qToBigEndian(static_cast<uint32_t>(dumpSize));
    file.write(reinterpret_cast<const char*>(&sizeToWrite), sizeof(sizeToWrite));

    const int N = buffer.size();
    int start = (bIndex - dumpSize + N) % N;

    for (int i = 0; i < dumpSize; ++i) {
        int index = (start + i) % N;
        uint16_t value = qToBigEndian(buffer[index]);
        file.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    file.close();
    return true;
}
