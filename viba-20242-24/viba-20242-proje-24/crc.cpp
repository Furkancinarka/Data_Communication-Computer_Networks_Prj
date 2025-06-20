#include "crc.h"
#include <QByteArray>

QString CRC::calculateCRC16(const QByteArray &data)
{
    quint16 crc = calculateCRC16Internal(data);
    return QString("%1").arg(crc, 4, 16, QChar('0')).toUpper();
}

bool CRC::verifyCRC16(const QByteArray &data, const QString &crc)
{
    quint16 calculatedCRC = calculateCRC16Internal(data);
    bool ok;
    quint16 expectedCRC = crc.toUShort(&ok, 16);
    return ok && (calculatedCRC == expectedCRC);
}

quint16 CRC::calculateCRC16Internal(const QByteArray &data)
{
    quint32 crc = INITIAL_VALUE;

    for (const char &ch : data) {
        quint8 byte = static_cast<quint8>(ch);
        crc ^= (static_cast<quint32>(byte) << 8);
        
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ POLYNOMIAL;
            } else {
                crc <<= 1;
            }
        }
    }

    return static_cast<quint16>(crc ^ FINAL_XOR_VALUE);
}
