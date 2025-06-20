#ifndef CRC_H
#define CRC_H

#include <QByteArray>
#include <QString>

class CRC
{
public:
    static QString calculateCRC16(const QByteArray &data);
    static bool verifyCRC16(const QByteArray &data, const QString &crc);

private:
    // Doğru polinom: x^16 + x^12 + x^5 + 1
static const quint16 POLYNOMIAL = 0x1021;
 // x¹⁶ + x¹² + x⁵ + 1
    static const quint16 INITIAL_VALUE = 0xFFFF;
    static const quint16 FINAL_XOR_VALUE = 0x0000;

    static quint16 calculateCRC16Internal(const QByteArray &data);
};

#endif // CRC_H 