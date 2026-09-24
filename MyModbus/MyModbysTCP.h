#pragma once

#include <QObject>
#include <QTimer>
#include <QTcpSocket>
#include "MOdbusDataUnit.h"
#include "ModbusReply.h"

// Клас MyModbysTCP для управління TCP-з'єднанням з Modbus-сервером
class MyModbysTCP: public QObject
{
    Q_OBJECT
public:
    // Перерахування можливих станів з'єднання
    enum State {
        UnconnectedState, // Немає з'єднання
        ConnectingState,  // В процесі підключення
        ConnectedState,   // З'єднання встановлено
        ClosingState      // З'єднання закривається
    };

    // Конструктор за замовчуванням (використовує IP "127.0.0.1" і порт 502)
    MyModbysTCP();
    // Конструктор з вказаним IP (порт за замовчуванням 502)
    MyModbysTCP(QString ip);
    // Конструктор з вказаним IP і портом
    MyModbysTCP(QString ip, int port);

    // Деструктор для очищення ресурсів
    ~MyModbysTCP();

    // Встановлює нову IP-адресу та перевіряє її валідність
    bool setIp(const QString& newIp);
    // Встановлює порт та перевіряє його валідність
    bool setPort(int port);
    // Перевіряє, чи валідні IP і порт
    bool isValid() const;

    // Ініціює підключення до сервера
    void startConnect();
    // Ініціює відключення від сервера
    void startDisconnect();

    void setResponseTimeout(int newTime);
    void setRequesrCount(int newCount);

    State state() {return m_state;}

    ModbusReply* sendReadResponse(ModbusDataUnit& unit, int devAddr);
    ModbusReply* sendWriteResponse(ModbusDataUnit& unit, int devAddr);

signals:
    // Сигнал, що сповіщає про зміну стану з'єднання
    void stateChanged(State);

private slots:
    void replyFinished();

private:
    QString m_ip;          // IP-адреса сервера
    bool ipValid;          // Прапорець валідності IP
    uint16_t m_port;       // Порт для з'єднання
    QTcpSocket* m_socket;  // Сокет для TCP-з'єднання
    State m_state;         // Поточний стан з'єднання

    // Перевіряє валідність IP-адреси за допомогою регулярного виразу
    bool isValidIp(const QString& ip) const;
    // Перевіряє, чи порт знаходиться в допустимому діапазоні (1-65535)
    bool isValidPort(int port) const;

    int m_responseTimeout;
    int m_requestCount;
    int m_requestStep;

    int m_deviceId;
    ModbusReply* m_nowReply;

    QByteArray m_sendForSocket;
    QTimer* m_requestTimer;

    void createReadArr(ModbusDataUnit &unit);
    void createWriteArr(ModbusDataUnit &unit);

    void sendToSocket();
    void takeRead();
    void tryResponseAgain();
};
