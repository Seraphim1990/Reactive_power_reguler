#pragma once
#include <QObject>
#include <QWebSocket>
#include <QQueue>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include "../ModbusReguler/ModbusReguler.h"

class ApiWorker: public QObject
{
    Q_OBJECT
public:
    ApiWorker(QObject* parent = nullptr);
    ~ApiWorker();
    void setReguler(ModbusReguler* reguler);

private:
    QWebSocket m_liveWebSocket;
    QUrl m_dataUrl;

    QUrl m_eventUrl;
    ModbusReguler* m_reguler;
    QList<QByteArray> pendingEvents;

    QString m_access_token;
    QString m_refresh_token;

    QByteArray m_login;

    QNetworkRequest login_req;
    QNetworkRequest refresh_req;
    QNetworkAccessManager m_networkMenager;

    void login();
    void refresh();

    void connectLiveSocket();


private slots:
    void liveWebSocketConnected();
    void onLiveTextMessageReceived(const QString& msg);

    void onLogin();
    void onRefresh();

};

