#ifndef FRAME_H
#define FRAME_H

#include <QByteArray>
#include <QString>
#include <QMap>

class Frame
{
public:
    Frame();
    Frame(const QByteArray &data);
    ~Frame();

    // Getters
    QByteArray getData() const;
    QString getCRC() const;
    int getFrameNumber() const;
    bool isValid() const;
    int getBitCount() const;
    bool isLastFrame() const;
    bool getHasPadding() const;
    QMap<QString, QString> getErrorInfo() const;

    // Setters
    void setData(const QByteArray &newData);
    void setCRC(const QString &newCRC);
    void setFrameNumber(int number);
    void setValid(bool newValid);
    void setBitCount(int count);
    void setLastFrame(bool isLast);
    void setHasPadding(bool value);

    // Operations
    void calculateCRC();
    bool verifyCRC() const;
    void addErrorInfo(const QString &errorType, const QString &description);

    // Display
    QString getHexData() const;
    QString getBinaryData() const;
    QString getDetailedInfo() const;
    QString toString() const;

private:
    QByteArray data;
    QString crc;
    int frameNumber;
    bool valid;
    int bitCount;
    bool lastFrame;
    bool hasPadding;
    QMap<QString, QString> errorInfo;
};

#endif // FRAME_H 