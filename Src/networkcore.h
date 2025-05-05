#pragma once

//STL
#include <memory.h>
#include <unordered_map>

//Qt
#include <QObject>
#include <QThread>

//My
#include <Common/httpsslquery.h>

#include <TradingCatCommon/transmitdata.h>
#include <TradingCatCommon/detector.h>

#include "localconfig.h"

class NetworkCore
    : public QObject
{
    Q_OBJECT

public:
    explicit NetworkCore(const LocalConfig& cfg);
    ~NetworkCore() override;

public slots:
    void start();
    void stop();

    void updateConfig(const TradingCatCommon::UserConfig& config);

signals:
    void login(const TradingCatCommon::UserConfig& userConfig);
    void logout();

    void klineDetect(const TradingCatCommon::Detector::KLinesDetectedList& detectData);
    void stockExchanges(const TradingCatCommon::StockExchangesIDList& stockExchangesIdList);

    /*!
        Дополнительное сообщение логеру
        @param category - категория сообщения
        @param msg - текст сообщения
    */
    void sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString& msg);

    void finished();

private slots:
    /*!
        Получен положительный ответ от сервера
        @param answer данные ответа
        @param id - ИД запроса
    */
    void getAnswerHttp(const QByteArray& answer, quint64 id);

    /*!
        Ошибка обработки запроса
        @param code - код ошибки
        @param serverCode - код ответа сервера (или 0 если ответ не получен)
        @param msg - сообщение об ошибке
        @param id - ИД запроса
    */
    void errorOccurredHttp(QNetworkReply::NetworkError code, quint64 serverCode, const QString& msg, quint64 id);

    /*!
        Дополнительное сообщение логеру
        @param category - категория сообщения
        @param msg - текст сообщения
        @param id -ИД запроса
    */
    void sendLogMsgHttp(Common::TDBLoger::MSG_CODE category, const QString& msg, quint64 id);

private:
    NetworkCore() = delete;
    Q_DISABLE_COPY_MOVE(NetworkCore)

    void sendHTTPRequest(TradingCatCommon::Query* query);

    void sendLogin();
    void sendStockExchanges();
    void sendConfig(const TradingCatCommon::UserConfig& config);
    void sendLogout();
    void sendDetect();

    bool parseLogin(const QByteArray& answer);
    bool parseStockExchanges(const QByteArray& answer);
    bool parseConfig(const QByteArray& answer);
    bool parseLogout(const QByteArray& answer);
    bool parseDetect(const QByteArray& answer);

private:
    const LocalConfig& _cfg;

    std::unique_ptr<Common::HTTPSSLQuery> _http;

    std::unordered_map<quint64, TradingCatCommon::Query*> _sentQuery;

    bool _isStarted = false;

    qint64 _sessionId = 0;
};

