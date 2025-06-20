#include "frame.h"
#include "crc.h"
#include <QByteArray>
#include <QStringBuilder>

Frame::Frame()
    : frameNumber(-1)
    , valid(true)
    , bitCount(0)
    , lastFrame(false)
    , hasPadding(false)
{
}

Frame::Frame(const QByteArray &data)
    : data(data)
    , frameNumber(-1)
    , valid(true)
    , bitCount(data.size() * 8)
    , lastFrame(false)
    , hasPadding(false)
{
    calculateCRC();
}

Frame::~Frame()
{
}

QByteArray Frame::getData() const
{
    return data;
}

QString Frame::getCRC() const
{
    return crc;
}

int Frame::getFrameNumber() const
{
    return frameNumber;
}

bool Frame::isValid() const
{
    return valid;
}

int Frame::getBitCount() const
{
    return bitCount;
}

void Frame::setData(const QByteArray &newData)
{
    data = newData;
}

void Frame::setCRC(const QString &newCRC)
{
    crc = newCRC;
}

void Frame::setFrameNumber(int number)
{
    frameNumber = number;
}

void Frame::setValid(bool newValid)
{
    valid = newValid;
}

void Frame::setBitCount(int count)
{
    bitCount = count;
}

void Frame::calculateCRC()
{
    crc = CRC::calculateCRC16(data);
}

bool Frame::verifyCRC() const
{
    return CRC::verifyCRC16(data, crc);
}

QString Frame::getHexData() const
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) result += " ";
        result += QString("%1").arg((unsigned char)data[i], 2, 16, QChar('0')).toUpper();
    }
    return result;
}

QString Frame::getBinaryData() const
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) result += " ";
        for (int b = 7; b >= 0; --b) {
            result += (data[i] & (1 << b)) ? "1" : "0";
        }
    }
    return result;
}

QString Frame::getDetailedInfo() const
{
    QString result;
    result += QString("=== Frame %1 Details ===\n\n").arg(frameNumber);
    
    // Basic Information
    result += QString("Frame Number: %1\n").arg(frameNumber);
    result += QString("Bit Count: %1\n").arg(bitCount);
    result += QString("Has Padding: %1\n").arg(hasPadding ? "Yes" : "No");
    
    // Convert CRC to binary
    bool ok;
    uint16_t crcValue = crc.toUShort(&ok, 16);
    QString binaryCRC;
    if (ok) {
        for (int b = 15; b >= 0; --b) {
            binaryCRC += (crcValue & (1 << b)) ? "1" : "0";
        }
    } else {
        binaryCRC = crc; // Fallback to hex if conversion fails
    }
    result += QString("CRC: %1\n").arg(binaryCRC);
    
    result += QString("Status: %1\n").arg(valid ? "Valid" : "Invalid");
    
    // Frame Type Information
    if (lastFrame) {
        result += "Type: Partial Frame\n";
    } else {
        result += "Type: Full Frame\n";
    }
    
    // Data Information
    result += "\nData:\n";
    result += QString("Hex: %1\n").arg(getHexData());
    result += QString("Binary: %1\n").arg(getBinaryData());
    
    // Error Information
    if (!errorInfo.isEmpty()) {
        result += "\nError Information:\n";
        for (const auto &error : errorInfo.toStdMap()) {
            result += QString("%1: %2\n").arg(error.first).arg(error.second);
        }
    }
    
    return result;
}

void Frame::addErrorInfo(const QString &errorType, const QString &description)
{
    errorInfo[errorType] = description;
}

QMap<QString, QString> Frame::getErrorInfo() const
{
    return errorInfo;
}

QString Frame::toString() const
{
    QString result = QString("Frame %1: ").arg(frameNumber);
    result += QString("Data: %1 bits, ").arg(bitCount);
    if (hasPadding) {
        result += "Has Padding, ";
    }
    
    // Convert CRC to binary
    bool ok;
    uint16_t crcValue = crc.toUShort(&ok, 16);
    QString binaryCRC;
    if (ok) {
        for (int b = 15; b >= 0; --b) {
            binaryCRC += (crcValue & (1 << b)) ? "1" : "0";
        }
    } else {
        binaryCRC = crc; // Fallback to hex if conversion fails
    }
    result += QString("CRC: %1, ").arg(binaryCRC);
    
    result += QString("Valid: %1").arg(valid ? "Yes" : "No");
    if (!errorInfo.isEmpty()) {
        result += QString(", Errors: %1").arg(errorInfo.size());
    }
    if (lastFrame) {
        result += ", Partial Frame";
    }
    return result;
}

bool Frame::isLastFrame() const
{
    return lastFrame;
}

void Frame::setLastFrame(bool isLast)
{
    lastFrame = isLast;
}

bool Frame::getHasPadding() const
{
    return hasPadding;
}

void Frame::setHasPadding(bool value)
{
    hasPadding = value;
} 