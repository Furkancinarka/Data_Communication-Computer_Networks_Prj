#include "datalinklayer.h"
#include <QFile>
#include <QDataStream>
#include <QRandomGenerator>
#include <QThread>
#include <QBitArray>
#include "crc.h"
#include <QDebug>

// DataLinkWorker Implementation
DataLinkWorker::DataLinkWorker(QObject *parent) : QObject(parent)
{
}

void DataLinkWorker::setData(const QVector<Frame>& newFrames)
{
    mutex.lock();
    frames = newFrames;
    mutex.unlock();
}

QByteArray DataLinkWorker::applyByteStuffing(const QByteArray &data) const
{
    QByteArray stuffed;
    stuffed.append(FRAME_FLAG); // Start flag
    
    for (const char &ch : data) {
        if (ch == FRAME_FLAG || ch == ESCAPE_CHAR) {
            stuffed.append(ESCAPE_CHAR);
            stuffed.append(ch ^ 0x20); // XOR with 0x20 to transform the character
        } else {
            stuffed.append(ch);
        }
    }
    
    stuffed.append(FRAME_FLAG); // End flag
    return stuffed;
}

QByteArray DataLinkWorker::removeByteStuffing(const QByteArray &stuffedData) const
{
    QByteArray destuffed;
    bool escaped = false;
    
    // Skip the start flag
    for (int i = 1; i < stuffedData.size() - 1; i++) {
        char ch = stuffedData[i];
        
        if (escaped) {
            destuffed.append(ch ^ 0x20); // Reverse the XOR transformation
            escaped = false;
        } else if (ch == ESCAPE_CHAR) {
            escaped = true;
        } else {
            destuffed.append(ch);
        }
    }
    
    return destuffed;
}

void DataLinkWorker::process()
{
    qDebug() << "Starting transmission process...";
    
    try {
        const int MAX_RETRIES = 3;
        int frameCount = 0;

        mutex.lock();
        if (frames.isEmpty()) {
            mutex.unlock();
            emit errorOccurred("No frames to process");
            return;
        }

        QVector<Frame> localFrames = frames; // Create a local copy
        mutex.unlock();

        qDebug() << "Processing" << localFrames.size() << "frames";

        for (Frame &frame : localFrames) {
            if (QThread::currentThread()->isInterruptionRequested()) {
                qDebug() << "Transmission interrupted by user";
                emit statusUpdate("Transmission stopped by user");
                return;
            }

            frameCount++;
            qDebug() << "Processing frame" << frame.getFrameNumber();
            
            int retryCount = 0;
            bool acked = false;

            while (!acked && retryCount < MAX_RETRIES) {
                try {
                    qDebug() << "Attempting to process frame" << frame.getFrameNumber() << "(Attempt" << retryCount + 1 << "/" << MAX_RETRIES << ")";

                    // Calculate CRC for the frame
                    frame.calculateCRC();
                    qDebug() << "Frame" << frame.getFrameNumber() << "CRC calculated";

                    // Apply byte stuffing to frame data
                    QByteArray stuffedData = applyByteStuffing(frame.getData());
                    frame.setData(stuffedData);
                    qDebug() << "Frame" << frame.getFrameNumber() << "byte stuffing applied";

                    // Simulate transmission errors
                    if (simulateDataLoss()) {
                        qDebug() << "Frame" << frame.getFrameNumber() << "lost (Attempt" << retryCount + 1 << "/" << MAX_RETRIES << ")";
                        frame.setValid(false);
                        frame.addErrorInfo("Lost", QString("Frame lost during transmission (Attempt %1/%2)").arg(retryCount + 1).arg(MAX_RETRIES));
                        emit statusUpdate(QString("Frame %1 lost during transmission (Attempt %2/%3)")
                            .arg(frame.getFrameNumber())
                            .arg(retryCount + 1)
                            .arg(MAX_RETRIES));
                        retryCount++;
                        QThread::msleep(100);
                        continue;
                    }

                    if (simulateDataCorruption()) {
                        qDebug() << "Frame" << frame.getFrameNumber() << "corrupted (Attempt" << retryCount + 1 << "/" << MAX_RETRIES << ")";
                        frame.setValid(false);
                        frame.addErrorInfo("Corrupted", QString("Frame corrupted during transmission (Attempt %1/%2)").arg(retryCount + 1).arg(MAX_RETRIES));
                        emit statusUpdate(QString("Frame %1 corrupted during transmission (Attempt %2/%3)")
                            .arg(frame.getFrameNumber())
                            .arg(retryCount + 1)
                            .arg(MAX_RETRIES));
                        retryCount++;
                        QThread::msleep(100);
                        continue;
                    }

                    // Send frame
                    qDebug() << "Sending frame" << frame.getFrameNumber();
                    emit frameProcessed(frame);

                    // Remove byte stuffing after successful transmission
                    QByteArray destuffedData = removeByteStuffing(frame.getData());
                    frame.setData(destuffedData);
                    qDebug() << "Frame" << frame.getFrameNumber() << "byte stuffing removed";

                    // Simulate ACK loss
                    if (simulateAckLoss()) {
                        qDebug() << "ACK lost for frame" << frame.getFrameNumber() << "(Attempt" << retryCount + 1 << "/" << MAX_RETRIES << ")";
                        frame.setValid(false);
                        frame.addErrorInfo("ACK Lost", QString("ACK lost during transmission (Attempt %1/%2)").arg(retryCount + 1).arg(MAX_RETRIES));
                        emit statusUpdate(QString("ACK lost for frame %1 (Attempt %2/%3)")
                            .arg(frame.getFrameNumber())
                            .arg(retryCount + 1)
                            .arg(MAX_RETRIES));
                        retryCount++;
                        QThread::msleep(100);
                        continue;
                    }

                    // Frame successfully transmitted and acknowledged
                    acked = true;
                    qDebug() << "Frame" << frame.getFrameNumber() << "successfully transmitted and acknowledged";
                    emit statusUpdate(QString("Frame %1 successfully transmitted and acknowledged")
                        .arg(frame.getFrameNumber()));

                    QThread::msleep(200);
                } catch (const std::exception& e) {
                    qDebug() << "Error processing frame" << frame.getFrameNumber() << ":" << e.what();
                    retryCount++;
                    QThread::msleep(100);
                }
            }

            if (!acked) {
                frame.setValid(false);
                frame.addErrorInfo("Transmission Failed", 
                    QString("Maximum retry attempts (%1) reached").arg(MAX_RETRIES));
                qDebug() << "Frame" << frame.getFrameNumber() << "transmission failed after" << MAX_RETRIES << "attempts";
                emit statusUpdate(QString("Frame %1 transmission failed after %2 attempts")
                    .arg(frame.getFrameNumber())
                    .arg(MAX_RETRIES));
            }
        }

        qDebug() << "All frames processed, calculating checksum...";
        calculateChecksum();

        QString checksumFrame = prepareChecksumFrame();
        emit checksumFrameSent(checksumFrame);
        qDebug() << "Checksum frame sent:" << checksumFrame;

        if (simulateChecksumError()) {
            qDebug() << "Checksum error detected";
            emit errorOccurred("Checksum error detected");
        }

        qDebug() << "Transmission complete";
        emit transmissionComplete();

    } catch (const std::exception& e) {
        qDebug() << "Error during transmission:" << e.what();
        emit errorOccurred(QString("Error during transmission: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "Unknown error during transmission";
        emit errorOccurred("Unknown error during transmission");
    }
}

void DataLinkWorker::calculateChecksum()
{
    quint8 accum = 0;
    for (const auto &frame : frames) {
        bool ok;
        quint16 crc = frame.getCRC().toUShort(&ok, 16);
        if (ok) {
            accum += crc; // Addition instead of XOR
            accum %= 0x100; // Modulo 256 (8-bit)
        }
    }
    checksum = QString("%1").arg(accum, 2, 16, QChar('0')).toUpper();
    emit checksumCalculated(checksum);
}

QString DataLinkWorker::prepareChecksumFrame() const
{
    QString frame = CHECKSUM_HEADER;
    frame += checksum;
    return escapeSpecialCharacters(frame);
}

QString DataLinkWorker::escapeSpecialCharacters(const QString &data) const
{
    QString result;
    for (const QChar &c : data) {
        if (c.unicode() == static_cast<ushort>(ESCAPE_CHAR) || c == QChar(CHECKSUM_HEADER[0])) {
            result.append(QChar(ESCAPE_CHAR));
        }
        result.append(c);
    }
    return result;
}

bool DataLinkWorker::simulateDataLoss()
{
    bool result = QRandomGenerator::global()->generateDouble() < 0.10; // 10% chance
    qDebug() << "simulateDataLoss result:" << result;
    return result;
}

bool DataLinkWorker::simulateDataCorruption()
{
    return QRandomGenerator::global()->generateDouble() < 0.20; // 20% chance
}

bool DataLinkWorker::simulateAckLoss()
{
    return QRandomGenerator::global()->generateDouble() < 0.15; // 15% chance
}

bool DataLinkWorker::simulateChecksumError()
{
    return QRandomGenerator::global()->generateDouble() < 0.05; // 5% chance
}

// DataLinkLayer Implementation
DataLinkLayer::DataLinkLayer(QObject *parent)
    : QObject(parent)
    , transmitting(false)
    , worker(new DataLinkWorker)
{
    qDebug() << "Initializing DataLinkLayer...";
    
    // Move worker to thread before starting it
    worker->moveToThread(&workerThread);

    // Connect thread signals
    connect(&workerThread, &QThread::started, this, []() { qDebug() << "Worker thread started"; });
    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    
    // Connect worker signals
    connect(worker, &DataLinkWorker::frameProcessed, this, [this](const Frame &frame) {
        qDebug() << "Frame processed signal received for frame" << frame.getFrameNumber();
        emit frameProcessed(frame);
    });
    connect(worker, &DataLinkWorker::transmissionComplete, this, [this]() {
        qDebug() << "Transmission complete signal received";
        mutex.lock();
        transmitting = false;
        mutex.unlock();
        emit transmissionComplete();
    });
    connect(worker, &DataLinkWorker::errorOccurred, this, [this](const QString &error) {
        qDebug() << "Error occurred:" << error;
        mutex.lock();
        transmitting = false;
        mutex.unlock();
        emit errorOccurred(error);
    });
    connect(worker, &DataLinkWorker::statusUpdate, this, &DataLinkLayer::statusUpdate);
    connect(worker, &DataLinkWorker::checksumCalculated, this, [this](const QString &value) {
        mutex.lock();
        checksum = value;
        mutex.unlock();
        emit checksumCalculated(value);
    });
    connect(worker, &DataLinkWorker::checksumFrameSent, this, [this](const QString &value) {
        mutex.lock();
        checksumFrame = value;
        mutex.unlock();
        emit checksumFrameSent(value);
    });

    // Start the thread
    workerThread.start();
    qDebug() << "Worker thread started successfully";
}

DataLinkLayer::~DataLinkLayer()
{
    stopTransmission();
    workerThread.quit();
    if (!workerThread.wait(1000)) {
        workerThread.terminate();
        workerThread.wait();
    }
}

bool DataLinkLayer::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Failed to open file");
        return false;
    }

    mutex.lock();
    currentFilePath = filePath;
    QByteArray fileData = file.readAll();
    file.close();

    // Clear previous frames
    frames.clear();
    checksum.clear();
    checksumFrame.clear();

    // Convert file data to bits
    QBitArray bits(fileData.size() * 8);
    for(int i = 0; i < fileData.size(); ++i) {
        for(int b = 0; b < 8; ++b) {
            bits.setBit(i * 8 + b, fileData.at(i) & (1 << (7 - b)));
        }
    }

    // Split into 100-bit frames
    const int frameBitSize = 100; // Exactly 100 bits per frame
    int totalFrames = (bits.count() + frameBitSize - 1) / frameBitSize;

    for(int i = 0; i < totalFrames; ++i) {
        // Calculate how many bits remain for this frame
        int remainingBits = qMin(frameBitSize, bits.count() - i * frameBitSize);
        
        // Create a byte array to hold this frame's data
        QByteArray frameData((frameBitSize + 7) / 8, 0); // Always allocate space for 100 bits
        
        // Copy bits to the frame data
        for(int j = 0; j < remainingBits; ++j) {
            if(bits.testBit(i * frameBitSize + j)) {
                int byteIndex = j / 8;
                int bitIndex = 7 - (j % 8);
                frameData[byteIndex] |= (1 << bitIndex);
            }
        }

        // Create frame and set its properties
        Frame frame(frameData);
        frame.setFrameNumber(i);
        
        // Handle padding for the last frame
        if (remainingBits < frameBitSize) {
            frame.setLastFrame(true);
            frame.setHasPadding(true);
            frame.setBitCount(frameBitSize); // Set to 100 bits after padding
            
            // Add padding bits (0s)
            for(int j = remainingBits; j < frameBitSize; ++j) {
                int byteIndex = j / 8;
                int bitIndex = 7 - (j % 8);
                frameData[byteIndex] &= ~(1 << bitIndex); // Set padding bit to 0
            }
            
            emit statusUpdate(QString("Partial frame detected: Frame %1 padded to %2 bits").arg(i).arg(frameBitSize));
        } else {
            frame.setBitCount(frameBitSize);
        }
        
        frames.append(frame);
    }

    worker->setData(frames);
    mutex.unlock();
    
    emit statusUpdate(QString("File loaded: %1 frames created").arg(frames.size()));
    return true;
}

QVector<Frame> DataLinkLayer::getFrames() const
{
    mutex.lock();
    QVector<Frame> result = frames;
    mutex.unlock();
    return result;
}

QString DataLinkLayer::getChecksum() const
{
    mutex.lock();
    QString result = checksum;
    mutex.unlock();
    return result;
}

QString DataLinkLayer::getChecksumFrame() const
{
    mutex.lock();
    QString result = checksumFrame;
    mutex.unlock();
    return result;
}

void DataLinkLayer::startTransmission()
{
    qDebug() << "Starting transmission...";
    
    if (transmitting) {
        qDebug() << "Transmission already in progress";
        emit errorOccurred("Transmission already in progress");
        return;
    }

    mutex.lock();
    if (frames.isEmpty()) {
        mutex.unlock();
        qDebug() << "No frames to transmit";
        emit errorOccurred("No frames to transmit");
        return;
    }
    transmitting = true;
    mutex.unlock();

    qDebug() << "Invoking process method in worker thread";
    QMetaObject::invokeMethod(worker, "process", Qt::QueuedConnection);
}

void DataLinkLayer::stopTransmission()
{
    mutex.lock();
    if (transmitting) {
        transmitting = false;
        workerThread.requestInterruption();
    }
    mutex.unlock();
}

bool DataLinkLayer::isTransmitting() const
{
    mutex.lock();
    bool result = transmitting;
    mutex.unlock();
    return result;
} 