#pragma once

#include <QObject>
#include "MOdbusDataUnit.h"

class ModbusReply : public QObject
{
    Q_OBJECT
public:
    enum Errors {
        NoError,
        DataUnitInvalid,
        InvalidResponseCommand,
        InvalidResponseLength,
        UnkownWorkType,
        UnknownError,
        DeviceNotResponce

    };
    enum WorkType {
        Unknown,
        Read,
        Write
    };

    explicit ModbusReply(ModbusDataUnit& unit);

    Errors error();
    void read();
    void write();

    bool isValid();
    void valid();
    void inValid();
    void notReaded();
    const std::vector<int>& data();

    void takeData(const QByteArray& response);

signals:
    void finished();
private:
    bool m_valid;
    ModbusDataUnit* m_dataUnit;
    std::vector<int> m_dataList;

    Errors m_error;
    WorkType m_workType;

    void holdingRegisters(const QByteArray &response);

    void vasRead(const QByteArray &response);
    void vasWrite(const QByteArray &response);
    void discreteInputsRead(const QByteArray &response);
    void holdingRegistersWrite(const QByteArray &response);
    void coilsWrite(const QByteArray &response);
    void inputRegistersRead(const QByteArray &response);
    void holdingRegistersRead(const QByteArray &response);
    void coilsRead(const QByteArray &response);
};

