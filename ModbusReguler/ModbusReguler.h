#pragma once
#include <QObject>
#include <QSettings>
#include "../MyModbus/MyModbysTCP.h"
#include "CondensatorsWorker.h"

class ApiWorker;

class ModbusReguler: public QObject
{
    Q_OBJECT
public:
    enum ReguгelerEvents {
        WorkStatus,
        SetPointChanged,
        OutputSetpointChanged,
        ConnectStatus
    };
    ModbusReguler(QObject* parent = nullptr);
    ~ModbusReguler();
    void setApiWorker(ApiWorker* apiWorker);
    void setDataForReguler(std::map<QString, double> data);
    void setEventForReguler(std::map<ReguгelerEvents, double> events);

    void loadWorkingStatus();
    void loadSetPoint();

private:
    int KGU_status;
    bool onKGUWork;
    CondensatorsWorker m_condensators;
    MyModbysTCP m_tcpUnit;
    QSettings m_settings;
    ApiWorker* m_apiWorker;
    ModbusDataUnit m_remoteControl;
    ModbusDataUnit m_output;
    std::function<void(double)> m_func;
    std::optional<int> m_setpoint;
    int m_outputData;
    int m_deviceOutputData;
    int m_totalMiddlePower;

    void passFunc(double data) {return;}
    void workingFunc(double data);
    void sendCosSet(int val);

private slots:
    void onRemoteRecieved();
    void onOutputRecieved();

    void onTcpStateChanged(MyModbysTCP::State);

};

