#include "MOdbusDataUnit.h"
#include <qdebug.h>


ModbusDataUnit::ModbusDataUnit(RegisterStatus regType, int addr, int lenght):
    m_regType(regType),
    m_regAddr(addr),
    m_length(lenght),
    m_dataForWrite(lenght),
    spesificCommandForRead(0),
    spesificCommandForWrite(0)
{
}

bool ModbusDataUnit::isValid()
{
    return m_regAddr > 0 && m_regAddr < 65535 && m_length > 0 && m_length < 65535;
}

int ModbusDataUnit::lenght()
{
    return m_length;
}

int ModbusDataUnit::regAddr()
{
    return m_regAddr;
}

void ModbusDataUnit::setSpesificCommandRead(int newCommand)
{
    spesificCommandForRead = newCommand > 0 && newCommand <= 0x2B ? newCommand : 0;
}

void ModbusDataUnit::setSpesificCommandWrite(int newCommand)
{
    spesificCommandForWrite = newCommand > 0 && newCommand <= 0x2B ? newCommand : 0;
}

void ModbusDataUnit::value(int val)
{
    value(val, 0);
}

void ModbusDataUnit::value(int val, int index)
{
    if(index >= m_dataForWrite.size()) return;
    m_dataForWrite[index] = val;
}

char ModbusDataUnit::getReadCommand()
{
    return spesificCommandForRead > 0 ? spesificCommandForRead : readCommand();
}

char ModbusDataUnit::getWriteCommand()
{
    return spesificCommandForWrite > 0 ? spesificCommandForWrite : writeCommand();
}


ModbusDataUnit::RegisterStatus ModbusDataUnit::regType()
{
    return m_regType;
}

QByteArray ModbusDataUnit::getReadRequest()
{
    QByteArray request;
    char command = spesificCommandForRead > 0 ? spesificCommandForRead : readCommand();

    request.append(command);
    request.append(static_cast<char>(m_regAddr >> 8));
    request.append(static_cast<char>(m_regAddr & 0xFF));
    request.append(static_cast<char>(m_length >> 8));
    request.append(static_cast<char>(m_length & 0xFF));
    return request;
}

QByteArray ModbusDataUnit::getWriteRequest()
{
    QByteArray request;
    char command = spesificCommandForWrite > 0 ? spesificCommandForWrite : writeCommand();

    request.append(command);
    request.append(static_cast<char>(m_regAddr >> 8));
    request.append(static_cast<char>(m_regAddr & 0xFF));

    if (m_regType == HoldingRegisters) {
        request.append(getForHolding());
    }
    else {
        request.append(getForCoils());
    }
    return request;
}
char ModbusDataUnit::readCommand()
{
    char command = 0;

    switch (m_regType) {
    case Coils:
        command = 0x01;
        break;
    case DiscreteInputs:
        command = 0x02;
        break;
    case HoldingRegisters:
        command = 0x03;
        break;
    case InputRegisters:
        command = 0x04;
        break;
    default:
        break;
    };

    return command;
}

char ModbusDataUnit::writeCommand()
{
    char command = 0;
    if (m_length == 1) {
        switch (m_regType) {
        case Coils:
            command = 0x05;
            break;
        case HoldingRegisters:
            command = 0x06;
            break;
        default:
            break;
        };
    }
    else {
        switch (m_regType) {
        case Coils:
            command = 0x0F;
            break;
        case HoldingRegisters:
            command = 0x10;
            break;
        default:
            break;
        };
    }
    return command;
}

QByteArray ModbusDataUnit::getForHolding()
{
    QByteArray request;
    if (m_length == 1 && spesificCommandForWrite != 0x10) {
        request.append(static_cast<char>(m_dataForWrite[0] >> 8));
        request.append(static_cast<char>(m_dataForWrite[0] & 0xFF));
    }
    else {
        request.append(static_cast<char>(m_length >> 8));
        request.append(static_cast<char>(m_length & 0xFF));
        request.append(static_cast<char>(m_dataForWrite.size() * 2));
        for (int i = 0; i < m_dataForWrite.size(); i++) {
            request.append(static_cast<char>(m_dataForWrite[i] >> 8));
            request.append(static_cast<char>(m_dataForWrite[i] & 0xFF));
        }
    }
    return request;
}

QByteArray ModbusDataUnit::getForCoils()
{
    QByteArray request;

    if (m_length == 1) {
        // Запис одного біта: 1 байт, де 0x00 або 0xFF (Modbus стандарт)
        char val = m_dataForWrite[0] ? 0xFF : 0x00;
        request.append(val);
    }
    else {
        // Запис множини біті, пакуємо в байти
        // Спочатку додаємо кількість регістрів (бітів) — 2 байти
        request.append(static_cast<char>(m_length >> 8));
        request.append(static_cast<char>(m_length & 0xFF));

        // Кількість байтів в пакеті даних
        int byteCount = (m_length + 7) / 8;
        request.append(static_cast<char>(byteCount));

        // Запаковуємо біти в байти
        QByteArray dataBytes;
        dataBytes.resize(byteCount);
        dataBytes.fill(0);

        for (int i = 0; i < m_length; ++i) {
            if (m_dataForWrite[i]) {
                dataBytes[i / 8] |= (1 << (i % 8));
            }
        }
        request.append(dataBytes);
    }

    return request;
}

