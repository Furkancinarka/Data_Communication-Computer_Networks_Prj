#ifndef DATALINKLAYER_H
#define DATALINKLAYER_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QThread>
#include <QMutex>
#include "frame.h"

// Frame flags and escape character definitions
#define FRAME_FLAG 0x7E
#define ESCAPE_CHAR 0x7D

// Special header for checksum frame
#define CHECKSUM_HEADER "CHK"

class DataLinkWorker : public QObject
{
    Q_OBJECT

public:
    explicit DataLinkWorker(QObject *parent = nullptr);
    void setData(const QVector<Frame>& newFrames);

public slots:
    void process();

signals:
    void frameProcessed(const Frame &frame);
    void transmissionComplete();
    void errorOccurred(const QString &error);
    void statusUpdate(const QString &status);
    void checksumCalculated(const QString &checksum);
    void checksumFrameSent(const QString &checksumFrame);

private:
    QVector<Frame> frames;
    QString checksum;
    QString checksumFrame;
    QMutex mutex;

    void calculateChecksum();
    QString prepareChecksumFrame() const;
    QString escapeSpecialCharacters(const QString &data) const;
    QByteArray applyByteStuffing(const QByteArray &data) const;
    QByteArray removeByteStuffing(const QByteArray &stuffedData) const;
    bool simulateDataLoss();
    bool simulateDataCorruption();
    bool simulateAckLoss();
    bool simulateChecksumError(); // Only keep the bool version
};

class DataLinkLayer : public QObject
{
    Q_OBJECT

public:
    explicit DataLinkLayer(QObject *parent = nullptr);
    ~DataLinkLayer();

    // File operations
    bool loadFile(const QString &filePath);
    QVector<Frame> getFrames() const;
    QString getChecksum() const;
    QString getChecksumFrame() const;

    // Transmission operations
    void startTransmission();
    void stopTransmission();
    bool isTransmitting() const;

signals:
    void frameProcessed(const Frame &frame);
    void transmissionComplete();
    void errorOccurred(const QString &error);
    void statusUpdate(const QString &status);
    void checksumCalculated(const QString &checksum);
    void checksumFrameSent(const QString &checksumFrame);

private:
    QVector<Frame> frames;
    QString checksum;
    QString checksumFrame;
    bool transmitting;
    QString currentFilePath;
    
    QThread workerThread;
    DataLinkWorker* worker;
    mutable QMutex mutex;
};

#endif // DATALINKLAYER_H 