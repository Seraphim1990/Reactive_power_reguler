#include "CondensatorsWorker.h"
#include "QSettings"
#include <QTimer>
#include <QDateTime>

CondensatorsWorker::CondensatorsWorker(QObject* parent):
    QObject(parent),
    m_dataSet(),
    releyStatus(0),
    m_condensators(),
    m_client(),
    stopWork(false),
    m_loopTimer(),
    m_waitTimer(),
    m_output(QModbusDataUnit::HoldingRegisters, 470 ,1)
{
    QSettings settings("setting/settings.ini", QSettings::IniFormat);
    m_client.setConnectionParameter(QModbusDevice::NetworkAddressParameter, settings.value("bataryIp").toString());
    m_client.setConnectionParameter(QModbusDevice::NetworkPortParameter, settings.value("bataryPort").toInt());
    m_client.setNumberOfRetries(1);
    m_client.connectDevice();
    connect(&m_client, &QModbusTcpClient::stateChanged, this, &CondensatorsWorker::onTcpStateChanged);

    QString relBase("rel_%1");
    for (int i = 1; i < 17; i++){
        QString relData = relBase.arg(i);
        int relVal = settings.value(relData).toInt();
        m_condensators.push_back(relVal);
    }
    m_loopTimer.setInterval(2000);
    m_loopTimer.setSingleShot(true);
    connect(&m_loopTimer, &QTimer::timeout, this, &CondensatorsWorker::queueLoop);


    m_waitTimer.setInterval(60000);
    m_waitTimer.setSingleShot(true);
    connect(&m_waitTimer, &QTimer::timeout, this, [this](){
        stopWork = false;
    });
}

void CondensatorsWorker::addData(int16_t newValue)
{
    m_dataSet.push_back(newValue);
    if(stopWork) return;
    int middleVal = getMiddleValue();
    if (!middleVal || std::abs(middleVal) < m_condensators[0]) {
        return;
    }
    int targetOutput = middleVal + nowProdustion();
    qDebug() << "targetOutput"<< targetOutput << "*middleVal" << middleVal << "nowProdustion" << nowProdustion();
    setWorkigQueue(targetOutput);
}

void CondensatorsWorker::setReley(int reley)
{
    releyStatus = reley;
}
int CondensatorsWorker::getMiddleValue()
{
    if(m_dataSet.size() < 30) return 0;
    int count = 0;
    for (int i = m_dataSet.size() - 31; i < m_dataSet.size(); i++) {
        count += m_dataSet[i];
    }
    return count / 30;
}

void CondensatorsWorker::setWorkigQueue(int target)
{
    stopWork = true;
    qDebug() <<"----------------Count output---------------------";
    int counter = target;
    int16_t reley = releyStatus;

    for (int i = m_condensators.size() - 1; i >= 0; i--) {
        if (m_condensators[i] && counter - m_condensators[i] > 0) {
            if (!((reley >> i) & 1)) {
                reley |= (1 << i);                    // вмикаємо біт i
                m_workQueue.enqueue(reley);           // ставимо у чергу новий стан
            }
            qDebug() << m_condensators[i];
            counter -= m_condensators[i];
        }
        else {
            if ((reley >> i) & 1) {                   // якщо був увімкнений — вимикаємо
                reley &= ~(1 << i);
                m_workQueue.enqueue(reley);
            }
        }
    }
    m_loopTimer.start();
    m_dataSet.clear();
    qDebug() <<"----------------End Count output---------------------";
    qDebug() << "counter" << counter;
    releyStatus = reley;
    qDebug() << "final result" << nowProdustion();
    qDebug() << "----------------End result output---------------------";
}


int CondensatorsWorker::nowProdustion()
{
    int totalPowerProd = 0;
    for (int i = 0; i < m_condensators.size(); i++) {
        if((releyStatus >> i) & 1) {
            totalPowerProd += m_condensators[i];
        }
    }
    return totalPowerProd;
}

void CondensatorsWorker::onTcpStateChanged(QModbusDevice::State state)
{
    if (state == QModbusTcpClient::ConnectedState) {
        qDebug() << "*****************************************************************";
        qDebug() << "ConnectedState";
        m_dataSet.clear();
        m_workQueue.clear();
        stopWork = false;
        qDebug() << "Reactive arr clear =" << (m_dataSet.size() == 0);
        qDebug() << "Work queue clear =" << m_workQueue.isEmpty();
        qDebug() << "Acummulation data =" << !stopWork;
        qDebug() << QDateTime::currentDateTime();
        qDebug() << "*****************************************************************";
    }
    else if (state == QModbusTcpClient::UnconnectedState){
        qDebug() << "*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-";
        qDebug() << "UnconnectedState, reconnect after 5 sec.";
        qDebug() << QDateTime::currentDateTime();
        QTimer::singleShot(5000, this, [this]() {
            m_client.connectDevice(); // Спроба перепідключення через 5 сек
        });
        qDebug() << "*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-";
    }
}

void CondensatorsWorker::queueLoop()
{
    if(!m_workQueue.isEmpty()) {
        m_output.setValue(0, m_workQueue.dequeue());
        auto reply = m_client.sendWriteRequest(m_output, 1);
        if(reply) {
            connect(reply, &QModbusReply::finished, this, &CondensatorsWorker::sendReplyFinished);
        }
    }
    else {
        m_waitTimer.start();
    }
}

void CondensatorsWorker::sendReplyFinished()
{
    auto reply = static_cast<QModbusReply*>(sender());

    if(reply->error() == QModbusDevice::NoError){
        m_loopTimer.start();
    }
    else {
        qDebug() << reply->error();
        QTimer::singleShot(1000, this, [this](){
            auto _reply = m_client.sendWriteRequest(m_output, 1);
            if(_reply) {
                connect(_reply, &QModbusReply::finished, this, &CondensatorsWorker::sendReplyFinished);
                qDebug() << "send reply after error" << QDateTime::currentDateTime();
            }
        });
    }

    reply->deleteLater();
}

void CondensatorsWorker::quitWork()
{
    m_dataSet.clear();
    int16_t relays = releyStatus; // поточний стан реле

    // Проходимо по всіх реле
    for (int i = 0; i < m_condensators.size(); ++i) {
        if ((relays >> i) & 1) {             // якщо реле увімкнене
            relays &= ~(1 << i);             // вимикаємо його
            m_workQueue.enqueue(relays);     // додаємо новий проміжний стан у чергу
        }
    }

    stopWork = true; // щоб не додавалося нових команд до черги під час виключення
    m_loopTimer.start(); // запускаємо обробку черги
}
