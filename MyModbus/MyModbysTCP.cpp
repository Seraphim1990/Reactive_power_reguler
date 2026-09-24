#include "MyModbysTCP.h"
#include <QRegularExpression>

// Конструктор за замовчуванням: ініціалізує IP як "127.0.0.1"
MyModbysTCP::MyModbysTCP(): MyModbysTCP("127.0.0.1") {}

// Конструктор з IP: використовує порт за замовчуванням 502
MyModbysTCP::MyModbysTCP(QString ip): MyModbysTCP(ip, 502) {}

// Основний конструктор: ініціалізує IP, порт і сокет, налаштовує обробники подій
MyModbysTCP::MyModbysTCP(QString ip, int port):
    m_ip(),
    m_port(502),
    ipValid(false),
    m_socket(new QTcpSocket(this)),
    m_state(UnconnectedState),
    m_responseTimeout(100),
    m_requestCount(1),
    m_deviceId(0),
    m_nowReply(nullptr),
    m_sendForSocket(),
    m_requestTimer(new QTimer)
{
    // Встановлюємо IP і порт
    setIp(ip);
    setPort(port);

    // Обробник відключення: змінює стан на ClosingState
    QObject::connect(m_socket, &QTcpSocket::disconnected, this, [this] () {
        m_state = ClosingState;
        emit stateChanged(m_state);
    });

    // Обробник помилок: змінює стан на UnconnectedState
    QObject::connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError err) {
        m_state = UnconnectedState;
        emit stateChanged(m_state);
    });

    // Обробник успішного підключення: змінює стан на ConnectedState
    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        m_state = ConnectedState;
        emit stateChanged(m_state);
    });

    connect(m_socket, &QTcpSocket::hostFound, this, [this]() {
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &MyModbysTCP::takeRead);

    connect(m_requestTimer, &QTimer::timeout, this, &MyModbysTCP::tryResponseAgain);
    m_requestTimer->setSingleShot(true);
}

// Деструктор: від’єднується від хоста та очищає сокет
MyModbysTCP::~MyModbysTCP()
{
    m_socket->disconnectFromHost();
    m_socket->deleteLater();
    m_requestTimer->deleteLater();
}

// Встановлює нову IP-адресу, якщо вона валідна
bool MyModbysTCP::setIp(const QString &newIp)
{
    bool valid = isValidIp(newIp);
    if (valid) {
        m_ip = newIp;
        ipValid = true;
    }
    return valid;
}

// Встановлює порт, якщо він валідний
bool MyModbysTCP::setPort(int port)
{
    bool valid = isValidPort(port);
    if (valid) m_port = port;
    return valid;
}

// Перевіряє, чи валідні IP і порт
bool MyModbysTCP::isValid() const
{
    return ipValid && isValidPort(m_port);
}

// Ініціює підключення до сервера, якщо параметри валідні та немає активного з'єднання
void MyModbysTCP::startConnect()
{
    if (!isValid() || m_state == ConnectedState) return;
    // Якщо сокет не в стані Unconnected, миттєво розриваємо з'єднання
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    m_state = ConnectingState;
    emit stateChanged(m_state);
    m_socket->connectToHost(m_ip, m_port);
}

// Ініціює відключення від сервера
void MyModbysTCP::startDisconnect()
{
    m_state = UnconnectedState;
    emit stateChanged(m_state);
    m_socket->disconnectFromHost();
}

void MyModbysTCP::setResponseTimeout(int newTime)
{
    m_responseTimeout = newTime > 0 ? newTime : 0;
}

void MyModbysTCP::setRequesrCount(int newCount)
{
    m_requestCount = newCount > 1 ? newCount : 1;
}

ModbusReply *MyModbysTCP::sendReadResponse(ModbusDataUnit &unit, int devAddr)
{
    if (!unit.isValid() || devAddr < 0) return nullptr;
    ModbusReply* tmpReply = new ModbusReply(unit);
    if (!tmpReply->isValid()) {
        delete tmpReply;
        return nullptr;
    }
    m_deviceId = devAddr;
    tmpReply->read();
    QObject::connect(tmpReply, &ModbusReply::finished, this, & MyModbysTCP::replyFinished);
    m_nowReply = tmpReply;
    createReadArr(unit);

    return m_nowReply;
}

ModbusReply *MyModbysTCP::sendWriteResponse(ModbusDataUnit &unit, int devAddr)
{
    if (!unit.isValid() || devAddr < 0) return nullptr;
    ModbusReply* tmpReply = new ModbusReply(unit);
    if (!tmpReply->isValid()) {
        delete tmpReply;
        return nullptr;
    }
    m_deviceId = devAddr;
    tmpReply->write();  // <-- Ти це реалізуєш у підкласі ModbusReply
    QObject::connect(tmpReply, &ModbusReply::finished, this, &MyModbysTCP::replyFinished);
    m_nowReply = tmpReply;
    createWriteArr(unit);
    return m_nowReply;
}

void MyModbysTCP::replyFinished()
{
    m_nowReply = nullptr;
}

// Перевіряє валідність IP-адреси за допомогою регулярного виразу
bool MyModbysTCP::isValidIp(const QString &ip) const
{
    QHostAddress addr;
    return addr.setAddress(ip) && addr.protocol() == QAbstractSocket::IPv4Protocol;
}

// Перевіряє, чи порт в межах допустимого діапазону (1-65535)
bool MyModbysTCP::isValidPort(int port) const
{
    return port > 0 && port <= 65535;
}

void MyModbysTCP::createReadArr(ModbusDataUnit &unit)
{
    m_sendForSocket.clear();
    m_requestStep = 0;

    QByteArray pdu = unit.getReadRequest();
    quint16 length = pdu.size() + 1;  // +1 за Unit ID

    QByteArray mbap;
    mbap.append(static_cast<char>(m_requestStep >> 8));
    mbap.append(static_cast<char>(m_requestStep & 0xFF));
    mbap.append(char(0x00)); // Protocol ID hi
    mbap.append(char(0x00)); // Protocol ID lo
    mbap.append(static_cast<char>(length >> 8));
    mbap.append(static_cast<char>(length & 0xFF));
    mbap.append(static_cast<char>(m_deviceId));  // Unit ID

    m_sendForSocket = mbap + pdu;

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(m_sendForSocket);
        m_requestTimer->start(m_responseTimeout);
    }

}

void MyModbysTCP::createWriteArr(ModbusDataUnit &unit)
{
    m_sendForSocket.clear();
    m_requestStep = 0;

    QByteArray pdu = unit.getWriteRequest();
    quint16 length = pdu.size() + 1;  // +1 за Unit ID

    QByteArray mbap;
    mbap.append(static_cast<char>(m_requestStep >> 8));
    mbap.append(static_cast<char>(m_requestStep & 0xFF));
    mbap.append(char(0x00)); // Protocol ID hi
    mbap.append(char(0x00)); // Protocol ID lo
    mbap.append(static_cast<char>(length >> 8));
    mbap.append(static_cast<char>(length & 0xFF));
    mbap.append(static_cast<char>(m_deviceId));  // Unit ID

    m_sendForSocket = mbap + pdu;

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(m_sendForSocket);
        m_requestTimer->start(m_responseTimeout);
    }
}

void MyModbysTCP::takeRead()
{
    m_requestTimer->stop();
    QByteArray response = m_socket->readAll();

    response.remove(0, 7);
    if (m_nowReply) {
        m_nowReply->takeData(response);
    }
}

void MyModbysTCP::tryResponseAgain()
{
    m_requestStep ++;
    if(m_requestStep > m_requestCount) {
        m_nowReply->notReaded();
        return;
    }
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(m_sendForSocket);
        m_requestTimer->start(m_responseTimeout);
    }
}
