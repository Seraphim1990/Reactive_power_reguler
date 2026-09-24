#pragma once
#include <QObject>
#include <QQueue>
#include <QModbusTcpClient>
#include "../MyModbus/MyModbysTCP.h"

class CondensatorsWorker: public QObject
{
    Q_OBJECT
public:
    CondensatorsWorker(QObject* parent = nullptr);

    void addData(int16_t newValue);
    void setReley(int reley);
    void quitWork();


private:
    QModbusTcpClient m_client;
    QModbusDataUnit m_output;
    QTimer m_waitTimer;
    QTimer m_loopTimer;
    std::vector<int16_t> m_dataSet;
    std::vector<int> m_condensators;
    int16_t releyStatus;
    QQueue<int16_t> m_workQueue;
    bool stopWork;

    int getMiddleValue();
    void setWorkigQueue(int target);
    int nowProdustion();

private slots:
    void onTcpStateChanged(QModbusDevice::State state);
    void queueLoop();
    void sendReplyFinished();
};

