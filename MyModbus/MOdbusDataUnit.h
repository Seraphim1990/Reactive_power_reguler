#pragma once
#include <QObject>

class ModbusDataUnit
{
public:
    enum RegisterStatus {
        DiscreteInputs,
        Coils,
        InputRegisters,
        HoldingRegisters
    };
    ModbusDataUnit(RegisterStatus regType, int addr, int lenght);

    bool isValid();

    int lenght();
    int regAddr();
    RegisterStatus regType();

    void setSpesificCommandRead(int newCommand);
    void setSpesificCommandWrite(int newCommand);
    void value(int val);
    void value(int val, int index);

    char getReadCommand();
    char getWriteCommand();



    QByteArray getReadRequest();
    QByteArray getWriteRequest();

private:
    uint16_t m_regAddr;
    uint16_t m_length;
    uint8_t spesificCommandForRead;
    uint8_t spesificCommandForWrite;
    RegisterStatus m_regType;

    std::vector<int> m_dataForWrite;

    char readCommand();
    char writeCommand();

    QByteArray getForHolding();
    QByteArray getForCoils();
};

