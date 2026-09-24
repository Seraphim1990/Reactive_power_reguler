#include "ModbusReguler.h"
#include "../APIWorker/ApiWorker.h"

// #define DBG_MOD

ModbusReguler::ModbusReguler(QObject *parent):
    QObject(parent),
    m_settings("setting/settings.ini", QSettings::IniFormat),
    m_remoteControl(ModbusDataUnit::HoldingRegisters, 7, 1),
    m_output(ModbusDataUnit::HoldingRegisters, 8, 1),
    m_setpoint(10),
    m_tcpUnit(),
    m_deviceOutputData(0),
    m_outputData(0),
    m_totalMiddlePower(0),
    m_apiWorker(nullptr),
    m_condensators(),
    KGU_status(0),
    onKGUWork(false)
{
    m_func = std::bind(&ModbusReguler::passFunc, this, std::placeholders::_1);

    m_remoteControl.setSpesificCommandWrite(0x10);
    m_remoteControl.value(1);
    m_output.setSpesificCommandWrite(0x10);

    m_tcpUnit.setIp(m_settings.value("engineIp").toString());
    m_tcpUnit.setPort(m_settings.value("enginePort").toInt());
    m_tcpUnit.setRequesrCount(3);
    m_tcpUnit.setResponseTimeout(300);
    connect(&m_tcpUnit, &MyModbysTCP::stateChanged, this, &ModbusReguler::onTcpStateChanged);
    m_tcpUnit.startConnect();
}


ModbusReguler::~ModbusReguler()
{
    m_tcpUnit.startDisconnect();
}

void ModbusReguler::setApiWorker(ApiWorker *apiWorker)
{
    m_apiWorker = apiWorker;
}

void ModbusReguler::setDataForReguler(std::map<QString, double> data)
{
    for (auto it = data.begin(); it != data.end(); it++) {
        if(it->first == "TotalMidleKVar") {
            m_func(it->second);
        } else if(it->first == "Distance_working_reguler_engine") {
            if(!it->second) {
                auto reply = m_tcpUnit.sendWriteResponse(m_remoteControl, 1);
                if (reply) {
                    connect(reply, &ModbusReply::finished, this, &ModbusReguler::onRemoteRecieved);
                }
            }
        } else if(it->first == "Cos_phi_engine_output") {
            m_deviceOutputData = it->second;
        }
        else if (it->first == "TotalMiddlePower") {
            m_totalMiddlePower = it->second;
        } else if(it->first == "Condensators_reley_output") {
            m_condensators.setReley(it->second);
        }
        else if(it->first == "KGU_generator_work") {
            int16_t KguTmpSt = static_cast<int16_t>(it->second);
            // qDebug() << "input KGU stat" << KGU_status;
            if (KGU_status == KguTmpSt)continue;
            KGU_status = KguTmpSt;

            if (!KGU_status && onKGUWork) { // if (KGU_status < 1000 && onKGUWork) { //
                onKGUWork = false;
                m_outputData = 795;
                sendCosSet(m_outputData);
                qDebug() << "***********";
                qDebug() << "KGU off";
                qDebug() << "***********";
            }
            else if (KGU_status && !onKGUWork){ // else if (KGU_status >= 1000 && !onKGUWork){ //
                onKGUWork = true;
                m_condensators.quitWork();
                qDebug() << "***********";
                qDebug() << "KGU on";
                qDebug() << "***********";
            }
        }
    }
}

void ModbusReguler::setEventForReguler(std::map<ReguгelerEvents, double> events)
{
    for (auto it = events.begin(); it != events.end(); it++) {
        switch (it->first) {
        case WorkStatus:
            qDebug() << "WorkStatus changed to: " << static_cast<bool>(it->second) << QDateTime::currentDateTime();
            m_func = it->second ? std::bind(&ModbusReguler::workingFunc, this, std::placeholders::_1)
                          : std::bind(&ModbusReguler::passFunc, this, std::placeholders::_1);
            m_settings.setValue("workingStatus", it->second);
            break;
        case SetPointChanged:
            qDebug() << "SetPointChanged to: " << it->second << QDateTime::currentDateTime();
            m_setpoint = it->second;
            m_settings.setValue("setpoint", it->second);
            break;
        case OutputSetpointChanged:
            break;
        case ConnectStatus:
            break;
        }
    }
}

void ModbusReguler::workingFunc(double data)
{
    if(onKGUWork) {
        static int loothCounter = 0;
        if (m_totalMiddlePower < 0) {
            if (m_deviceOutputData != m_outputData && loothCounter < 3) loothCounter++;
            else {
                if(std::abs(data) > m_setpoint) {
                    if(int step = data / *m_setpoint; std::abs(step) <= 10) {
                        m_outputData = m_deviceOutputData - step;
                        qDebug() << "step: " << step;
                    }
                    else {
                        int abstractStep = 15;
                        qDebug() << "abstractStep: " << abstractStep;
                        abstractStep = data > 0 ? abstractStep : - abstractStep;
                        m_outputData = m_deviceOutputData - abstractStep;
                    }
                }

                if(m_outputData > 1000) m_outputData = 1000;
                if(m_outputData < 0) m_outputData = 0;
                qDebug() << m_outputData << m_deviceOutputData;
                if(m_outputData == m_deviceOutputData) return;
                sendCosSet(m_outputData);
                loothCounter = 0;
            }
        }
    }
    else{
        m_condensators.addData(data);
    }
}

void ModbusReguler::sendCosSet(int val)
{
    m_output.value(val);
    auto reply = m_tcpUnit.sendWriteResponse(m_output, 1);
    if(reply) {
        connect(reply, &ModbusReply::finished, this, &ModbusReguler::onOutputRecieved);
    }
}

void ModbusReguler::loadWorkingStatus()
{
    if (m_settings.value("workingStatus").isValid()) {
            m_func = m_settings.value("workingStatus").toBool() ? std::bind(&ModbusReguler::workingFunc, this, std::placeholders::_1)
                                                                : std::bind(&ModbusReguler::passFunc, this, std::placeholders::_1);
    }
}

void ModbusReguler::loadSetPoint()
{
    if(m_settings.value("setpoint").isValid()) {
        m_setpoint = m_settings.value("setpoint").toInt();
    }
}

void ModbusReguler::onRemoteRecieved()
{
    auto reply = static_cast<ModbusReply*>(sender());
    if(reply->error() != ModbusReply::NoError) {
        qDebug() << "Error with set remote mode";
        auto newReply = m_tcpUnit.sendWriteResponse(m_remoteControl, 1);
        if (reply) {
            connect(newReply, &ModbusReply::finished, this, &ModbusReguler::onRemoteRecieved);
        }
    }
    reply->deleteLater();
}
void ModbusReguler::onOutputRecieved()
{
    auto reply = static_cast<ModbusReply*>(sender());
    if(reply->error() != ModbusReply::NoError) {
        qDebug() << "Error with set output setpoint";
    }
    else {
        qDebug() << "Changed to: " << m_outputData;
    }
    reply->deleteLater();
}

void ModbusReguler::onTcpStateChanged(MyModbysTCP::State state)
{
    switch (state) {
    case MyModbysTCP::ConnectedState:
        m_func = m_settings.value("workingStatus").toBool() ? std::bind(&ModbusReguler::workingFunc, this, std::placeholders::_1)
                                                            : std::bind(&ModbusReguler::passFunc, this, std::placeholders::_1);
        qDebug() << "TCP unit connected";
        break;
    case MyModbysTCP::UnconnectedState:
        m_func = std::bind(&ModbusReguler::passFunc, this, std::placeholders::_1);
        qDebug() << "TCP unit disconnected";
        QTimer::singleShot(5000, this, [this]() {
            m_tcpUnit.startConnect();
        });
        break;
    default:
        break;
    }
}

