    #ifndef DUMPMANAGER_H
#define DUMPMANAGER_H

#include <QObject>
#include <QString>
#include <QtEndian>

class DumpManager : public QObject {
    Q_OBJECT
public:
    explicit DumpManager(QObject *parent = nullptr);

    bool saveToJSON(const QVector<uint16_t> &buffer, const int &bIndex, const int &dumpSize, const QString &filePath);
    bool saveToCSV(const QVector<uint16_t> &buffer, const int &bIndex, const int &dumpSize, const QString &filePath);
    bool saveToDat(const QVector<uint16_t> &buffer, const int &bIndex, const int &dumpSize, const QString &filePath);

private:
    QString m_defaultDirectory;
};

#endif // DUMPMANAGER_H
