//Qt
#include <QUrl>
#include <QTimer>

//My
#include <TradingCatCommon/appserverprotocol.h>

#include "networkcore.h"
#include "qassert.h"

using namespace TradingCatCommon;
using namespace Common;

static const quint64 SEND_INTERVAL = 5000;
#ifdef QT_NO_DEBUG
Q_GLOBAL_STATIC_WITH_ARGS(const QUrl, SERVER_URL, (QUrl("https://tradingcat.ru")));
#else
Q_GLOBAL_STATIC_WITH_ARGS(const QUrl, SERVER_URL, (QUrl("http://localhost:59923")));
#endif

NetworkCore::NetworkCore(const LocalConfig &cfg)
    : _cfg(cfg)
{
    qRegisterMetaType<TradingCatCommon::UserConfig>("TradingCatCommon::UserConfig");
    qRegisterMetaType<TradingCatCommon::PKLinesIDList>("TradingCatCommon::PKLinesIDList");
    qRegisterMetaType<TradingCatCommon::StockExchangeID>("TradingCatCommon::StockExchangeID");
}

NetworkCore::~NetworkCore()
{
    stop();
}

void NetworkCore::start()
{
    Q_ASSERT(!_isStarted);

    _http = std::make_unique<HTTPSSLQuery>();

    connect(_http.get(), SIGNAL(getAnswer(const QByteArray&, quint64)),
            SLOT(getAnswerHttp(const QByteArray&, quint64)));

    connect(_http.get(), SIGNAL(errorOccurred(QNetworkReply::NetworkError, quint64, const QString&, quint64, const QByteArray&)),
            SLOT(errorOccurredHttp(QNetworkReply::NetworkError, quint64, const QString&, quint64, const QByteArray&)));

    connect(_http.get(), SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&, quint64)),
            SLOT(sendLogMsgHttp(Common::TDBLoger::MSG_CODE, const QString&, quint64)));

    _isStarted = true;

    sendLogin();
}

void NetworkCore::stop()
{
    if (!_isStarted)
    {
        emit finished();

        return;
    }

    _http.reset();

    emit finished();
}

void NetworkCore::updateConfig(const TradingCatCommon::UserConfig &config)
{
    sendConfig(config);
}

void NetworkCore::getAnswerHttp(const QByteArray& answer, quint64 id)
{
    const auto it_sentQuery = _sentQuery.find(id);
    if (it_sentQuery == _sentQuery.end())
    {
        qWarning() << "Get answer from unkow query ID: " << id;

        return;
    }
    auto query = it_sentQuery->second;
    const auto& type = query->type();

    _sentQuery.erase(it_sentQuery);

    bool res = false;
    switch (type)
    {
    case PackageType::LOGIN:
    {
        res = parseLogin(answer);
        if (res)
        {
            sendStockExchanges();
        }
        break;
    }
    case PackageType::STOCKEXCHANGES:
    {
        res = parseStockExchanges(answer);
        if (res)
        {
            sendKLinesIdList();
        }
        break;
    }
    case PackageType::KLINESIDLIST:
    {
        res = parseKLinesIdList(answer);
        if (res)
        {
            if (!_unGetKLinesId.empty())
            {
                sendKLinesIdList();
            }
            else
            {
                sendDetect();
            }
        }
        break;
    }
    case PackageType::CONFIG:
    {
        res = parseConfig(answer);

        break;
    }
    case PackageType::LOGOUT:
    {
        res = parseLogout(answer);
        if (res)
        {
            sendLogin();
        }

        break;
    }
    case PackageType::DETECT:
    {
        res = parseDetect(answer);
        if (res)
        {
            QTimer::singleShot(SEND_INTERVAL, this, [this](){ sendDetect(); });
        }

        break;
    }
    default:
        Q_ASSERT(false);
    }

    if (!res)
    {
        _sessionId = 0;

        emit logout();

        QTimer::singleShot(SEND_INTERVAL, this, [this](){ sendLogin(); });
    }
}

void NetworkCore::errorOccurredHttp(QNetworkReply::NetworkError code, quint64 serverCode, const QString& msg, quint64 id, const QByteArray& answer)
{
    Q_UNUSED(code);
    Q_UNUSED(serverCode);

    emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Error send request to http server: Request ID: %1: %2 Data: %3")
                                                          .arg(id)
                                                          .arg(msg)
                                                          .arg(answer));

    const auto it_sentQuery = _sentQuery.find(id);
    if (it_sentQuery == _sentQuery.end())
    {
        qWarning() << QString("Get answer from unknow query ID: %1 Data: %2").arg(id).arg(answer);

        return;
    }

    auto query = it_sentQuery->second;
    const auto& type = query->type();

    _sentQuery.erase(it_sentQuery);

    switch (type)
    {
    case PackageType::LOGIN:
    {
        QTimer::singleShot(SEND_INTERVAL, this, [this](){ sendLogin(); });

        break;
    }
    case PackageType::LOGOUT:
    case PackageType::STOCKEXCHANGES:
    case PackageType::KLINESIDLIST:
    case PackageType::CONFIG:
    case PackageType::DETECT:
    {
        _sessionId = 0;
        _unGetKLinesId.clear();

        emit logout();

        QTimer::singleShot(SEND_INTERVAL, this, [this](){ sendLogin(); });

        break;
    }
    default:
        Q_ASSERT(false);
    }
}

void NetworkCore::sendLogMsgHttp(Common::TDBLoger::MSG_CODE category, const QString &msg, quint64 id)
{
    emit sendLogMsg(category, QString("Request ID: %1: %2").arg(id).arg(msg));
}

void NetworkCore::sendHTTPRequest(TradingCatCommon::Query* query)
{
    Q_CHECK_PTR(_http);
    Q_CHECK_PTR(query);

    QUrl url(*SERVER_URL);

    url.setQuery(query->query());
    url.setPath(query->path());

    HTTPSSLQuery::Headers headers;

    headers.emplace(QByteArray("Access-Control-Allow-Origin"), QByteArray("*"));
    headers.emplace(QByteArray("Access-Control-Allow-Methods"), QByteArray("*"));
    headers.emplace(QByteArray("Access-Control-Allow-Headers"), QByteArray("*"));

    const auto id = _http->send(url, HTTPSSLQuery::RequestType::GET, headers);

    query->setID(id);

    _sentQuery.emplace(id, query);
}

void NetworkCore::sendLogin()
{
    _sessionId = 0;
    _unGetKLinesId.clear();

    const auto& user = _cfg.user();
    const auto& password = _cfg.password();

    Q_ASSERT(!user.isEmpty());
    Q_ASSERT(!password.isEmpty());

    auto query = new LoginQuery(user, password);

    sendHTTPRequest(query);
}

void NetworkCore::sendStockExchanges()
{
    if (_sessionId == 0) //logout
    {
        return;
    }

    auto query =  new StockExchangesQuery(_sessionId);

    sendHTTPRequest(query);
}

void NetworkCore::sendKLinesIdList()
{
    if (_sessionId == 0 || _unGetKLinesId.empty()) //logout
    {
        return;
    }

    auto query =  new KLinesIDListQuery(_sessionId, *_unGetKLinesId.begin());

    sendHTTPRequest(query);
}

void NetworkCore::sendLogout()
{
    auto query =  new LogoutQuery(_sessionId);

    sendHTTPRequest(query);
}

void NetworkCore::sendConfig(const TradingCatCommon::UserConfig &config)
{
    Q_ASSERT(!config.isError());

    if (_sessionId == 0) //logout
    {
        return;
    }

    auto query = new ConfigQuery(_sessionId, config);

    sendHTTPRequest(query);
}

void NetworkCore::sendDetect()
{
    if (_sessionId == 0) //logout
    {
        return;
    }

    auto query = new DetectQuery(_sessionId);

    sendHTTPRequest(query);
}

bool NetworkCore::parseLogin(const QByteArray &answer)
{
    TradingCatCommon::Package<LoginAnswer> package(answer);

    if (package.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Login: Error parsing package: %1").arg(package.errorString()));

        return false;
    }

    const auto& status= package.status();
    if (status.code() != StatusAnswer::ErrorCode::OK)
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Login: Server answer with error. Code: %1. Error: %2")
                                                              .arg(StatusAnswer::errorCodeToStr(status.code()))
                                                              .arg(status.message()));

        return false;
    }

    const auto& data = package.data();
    if (data.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Login: Error parsing data: %1").arg(data.errorString()));

        return false;
    }

    _sessionId = data.sessionId();

    emit login(data.config());

    emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Login: Successfully. Server message: %1").arg(data.message()));

    return true;
}

bool NetworkCore::parseStockExchanges(const QByteArray &answer)
{
    TradingCatCommon::Package<StockExchangesAnswer> package(answer);

    if (package.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("StockExchanges: Error parsing package: %1").arg(package.errorString()));

        return false;
    }

    const auto& status= package.status();
    if (status.code() != StatusAnswer::ErrorCode::OK)
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("StockExchanges: Server answer with error. Code: %1. Error: %2")
                                                              .arg(StatusAnswer::errorCodeToStr(status.code()))
                                                              .arg(status.message()));

        return false;
    }

    const auto& data = package.data();
    if (data.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("StockExchanges: Error parsing data: %1").arg(data.errorString()));

        return false;
    }

    _unGetKLinesId = data.stockExchangeIdList();

    emit stockExchanges(_unGetKLinesId);

    if (!_unGetKLinesId.empty())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("StockExchanges: Successfully. Server message: %1").arg(data.message()));
    }
    else
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("StockExchanges: Successfully. StockExchange ID list is empty. Server message: %1").arg(data.message()));
    }

    return true;
}

bool NetworkCore::parseKLinesIdList(const QByteArray &answer)
{
    Q_ASSERT(!_unGetKLinesId.empty());

    TradingCatCommon::Package<KLinesIDListAnswer> package(answer);

    if (package.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("KLinesIDList: Error parsing package: %1").arg(package.errorString()));

        return false;
    }

    const auto& status= package.status();
    if (status.code() != StatusAnswer::ErrorCode::OK)
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("KLinesIDList: Server answer with error. Code: %1. Error: %2")
                            .arg(StatusAnswer::errorCodeToStr(status.code()))
                            .arg(status.message()));

        return false;
    }

    const auto& data = package.data();
    if (data.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("KLinesIDList: Error parsing data: %1").arg(data.errorString()));

        return false;
    }

    const auto& klinesId = data.klinesIdList();
    const auto& stockExchangeID = data.stockExchangeId();

    const auto it_unGetKLinesId =  _unGetKLinesId.find(stockExchangeID);
    if (it_unGetKLinesId == _unGetKLinesId.end())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("KLinesIDList: Undefine stock exchange: %1").arg(stockExchangeID.toString()));

        return false;
    }
    _unGetKLinesId.erase(it_unGetKLinesId);

    emit klinesIdList(stockExchangeID, klinesId);

    if (!klinesId->empty())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("KLinesIDList: Successfully. Server message: %1").arg(data.message()));
    }
    else
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("KLinesIDList: Successfully. KLines ID list is empty. Server message: %1").arg(data.message()));
    }

    return true;
}

bool NetworkCore::parseConfig(const QByteArray &answer)
{
    TradingCatCommon::Package<ConfigAnswer> package(answer);

    if (package.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Config: Error parsing package: %1").arg(package.errorString()));

        return false;
    }

    const auto& status= package.status();
    if (status.code() != StatusAnswer::ErrorCode::OK)
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Config: Server answer with error. Code: %1. Error: %2")
                                                              .arg(StatusAnswer::errorCodeToStr(status.code()))
                                                              .arg(status.message()));

        return false;
    }

    const auto& data = package.data();
    if (data.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Config: Error parsing data: %1").arg(data.errorString()));

        return false;
    }

    emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Config: Successfully. Server message: %1").arg(data.message()));

    return true;
}

bool NetworkCore::parseLogout(const QByteArray &answer)
{
    TradingCatCommon::Package<LogoutAnswer> package(answer);

    if (package.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Logout: Error parsing package: %1").arg(package.errorString()));

        return false;
    }

    const auto& status= package.status();
    if (status.code() != StatusAnswer::ErrorCode::OK)
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("logout: Server answer with error. Code: %1. Error: %2")
                                                              .arg(StatusAnswer::errorCodeToStr(status.code()))
                                                              .arg(status.message()));

        return false;
    }

    const auto& data = package.data();
    if (data.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Logout: Error parsing data: %1").arg(data.errorString()));

        return false;
    }

    _sessionId = 0;

    emit logout();

    emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("logout: Successfully. Server message: %1").arg(data.message()));

    return true;
}

bool NetworkCore::parseDetect(const QByteArray &answer)
{
    TradingCatCommon::Package<DetectAnswer> package(answer);

    if (package.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Detect: Error parsing package: %1").arg(package.errorString()));

        return false;
    }

    const auto& status= package.status();
    if (status.code() != StatusAnswer::ErrorCode::OK)
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Detect: Server answer with error. Code: %1. Error: %2")
                                                              .arg(StatusAnswer::errorCodeToStr(status.code()))
                                                              .arg(status.message()));

        return false;
    }

    const auto& data = package.data();
    if (data.isError())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Detect: Error parsing data: %1").arg(data.errorString()));

        return false;
    }

    const auto& detectData = data.klinesDetectedList();
    if (detectData.detected.empty())
    {
        emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Detect: Successfully. Detect data list is empty. Skip. Server message: %1").arg(data.message()));
    }
    else
    {
        emit klineDetect(detectData);

        emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Detect: Successfully. Server message: %1").arg(data.message()));
    }

    return true;
}
