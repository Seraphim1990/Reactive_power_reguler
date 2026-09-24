#include "ApiWorker.h"
#include <QSettings>
#include <QUrl>
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>
#include<QNetworkReply>
#include <QUrlQuery>
#include <limits>

ApiWorker::ApiWorker(QObject *parent):
    QObject(parent),
    m_liveWebSocket(),
    m_dataUrl(),
    m_access_token(),
    m_refresh_token(),
    m_networkMenager()
{
    QJsonObject l;

    l["username"] = "Reactive_leguler";
    l["password"] = "JKJHHJKLMASDU89234234lklkxc_1"; // а чого б і ні? всеодно копімастить буду :)
    m_login = QJsonDocument(l).toJson(QJsonDocument::Compact);

    QString urlAuthBase ("http://%1:%2/auth/%3");
    QSettings setting("setting/settings.ini", QSettings::IniFormat);

    login_req = QNetworkRequest(QUrl(urlAuthBase.arg(setting.value("hostAddr").toString()).arg(setting.value("hostPort").toInt()).arg("login")));
    refresh_req = QNetworkRequest(QUrl(urlAuthBase.arg(setting.value("hostAddr").toString()).arg(setting.value("hostPort").toInt()).arg("refresh_token")));


    QString urlStringBase("ws://%1:%2/live_data");
    m_dataUrl = urlStringBase.arg(setting.value("hostAddr").toString()).arg(setting.value("hostPort").toInt());


    connect(&m_liveWebSocket, &QWebSocket::disconnected,
            this, [this]() {
                qDebug() << "Data WebSocket closed";
                QTimer::singleShot(1000, this, [this]() {
                    refresh();
                });
            });
    connect(&m_liveWebSocket, &QWebSocket::connected, this, &ApiWorker::liveWebSocketConnected);
    connect(&m_liveWebSocket, &QWebSocket::textMessageReceived,this, &ApiWorker::onLiveTextMessageReceived);
    login();
}

ApiWorker::~ApiWorker()
{
    m_liveWebSocket.close();
}

void ApiWorker::setReguler(ModbusReguler *reguler)
{
    m_reguler = reguler;
    m_reguler->setApiWorker(this);
}

void ApiWorker::login()
{
    login_req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    QNetworkReply *reply = m_networkMenager.post(login_req, m_login);

    connect(reply, &QNetworkReply::finished, this, &ApiWorker::onLogin);
}

void ApiWorker::refresh()
{
    QJsonObject obj;
    obj["refresh_token"] = m_refresh_token;

    QByteArray body =
        QJsonDocument(obj).toJson(QJsonDocument::Compact);

    refresh_req.setHeader(
        QNetworkRequest::ContentTypeHeader,
        "application/json");

    QNetworkReply *reply =
        m_networkMenager.post(refresh_req, body);

    connect(reply,
            &QNetworkReply::finished,
            this,
            &ApiWorker::onRefresh);
}

void ApiWorker:: connectLiveSocket()
{;
    QUrl url = m_dataUrl;
    QUrlQuery query;

    query.addQueryItem("token", m_access_token);

    url.setQuery(query);
    if (m_liveWebSocket.state() != QAbstractSocket::UnconnectedState)
        m_liveWebSocket.abort();

    m_liveWebSocket.open(url);
}

void ApiWorker::liveWebSocketConnected()
{
    qDebug() << "liveWebSocketConnected";
    QJsonObject request;

    request["values"] = QJsonArray{
        "TotalMidleKVar",
        "Distance_working_reguler_engine",
        "Cos_phi_engine_output",
        "TotalMiddlePower",
        "Condensators_reley_output",
        "KGU_generator_work"
    };

    request["nodes"] = QJsonArray();
    request["devices"] = QJsonArray();

    m_liveWebSocket.sendTextMessage(
        QString::fromUtf8(
            QJsonDocument(request).toJson(QJsonDocument::Compact)
            )
        );
}

void ApiWorker::onLiveTextMessageReceived(const QString &msg)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError)
        return;

    if (!doc.isObject())
        return;

    QJsonObject root = doc.object();
    if (!root.contains("values"))
        return;

    QJsonArray values = root["values"].toArray();

    std::map<QString, double> data;

    for (const auto &v : values)
    {
        auto obj = v.toObject();

        double val = obj["value"].toDouble();
        if (val < -1.79769e+300) {
            continue;
        }

        data.emplace(
            obj["tag"].toString(),
            val
            );
    }

    m_reguler->setDataForReguler(data);
    /*
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        // Це був не JSON, а просто текст
        return;
    }

    std::map<QString, double> data;
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const QJsonValue &val : arr) {
            if (val.isObject()) {
                QJsonObject obj = val.toObject();
                QString tag = obj.value("tag").toString();
                double value = obj.value("value").toDouble();
                data.insert({tag, value});
            }
        }
    } else if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QString tag = obj.value("tag").toString();
        double value = obj.value("value").toDouble();
        data.insert({tag, value});
    }
    m_reguler->setDataForReguler(data);
*/
}

void ApiWorker::onLogin()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {return;}

    if(reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());

        if (!doc.isObject()) return;

        QJsonObject obj = doc.object();

        m_access_token = obj["access_token"].toString();
        m_refresh_token = obj["refresh_token"].toString();
        connectLiveSocket();
    }
    else {
        QTimer::singleShot(5000, this, &ApiWorker::login);
    }

    reply->deleteLater();
}

void ApiWorker::onRefresh()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    if (!reply) {return;}

    int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if(reply->error() == QNetworkReply::NoError)
    {
        QJsonDocument doc =
            QJsonDocument::fromJson(reply->readAll());

        m_access_token =doc.object()["access_token"].toString();

        connectLiveSocket();
    }
    else
    {
        switch (httpStatus) {
        case 401:
            // refresh протух -> логінимось
            login();
            break;
        default:
            QTimer::singleShot(1000, this, &ApiWorker::refresh);
            break;
        }
    }
    reply->deleteLater();
}

