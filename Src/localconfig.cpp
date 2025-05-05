#include "localconfig.h"

using namespace emscripten;

//static
LocalConfig::EHistoryKLineCount LocalConfig::stringToEHistoryKLineCount(const QString &count)
{
    if (count == QString::number(static_cast<quint64>(EHistoryKLineCount::MAX)))
    {
        return EHistoryKLineCount::MAX;
    }
    else if (count == QString::number(static_cast<quint64>(EHistoryKLineCount::MIN30)))
    {
        return EHistoryKLineCount::MIN30;
    }
    else if (count == QString::number(static_cast<quint64>(EHistoryKLineCount::H1)))
    {
        return EHistoryKLineCount::H1;
    }
    else if (count == QString::number(static_cast<quint64>(EHistoryKLineCount::H2)))
    {
        return EHistoryKLineCount::H2;
    }

    return EHistoryKLineCount::MAX;
}


LocalConfig::EReviewHistoryKLineCount LocalConfig::stringToEReviewHistoryKLineCount(const QString &count)
{
    if (count == QString::number(static_cast<quint64>(EReviewHistoryKLineCount::MAX)))
    {
        return EReviewHistoryKLineCount::MAX;
    }
    else if (count == QString::number(static_cast<quint64>(EReviewHistoryKLineCount::H2)))
    {
        return EReviewHistoryKLineCount::H2;
    }
    else if (count == QString::number(static_cast<quint64>(EReviewHistoryKLineCount::H6)))
    {
        return EReviewHistoryKLineCount::H6;
    }
    else if (count == QString::number(static_cast<quint64>(EReviewHistoryKLineCount::H12)))
    {
        return EReviewHistoryKLineCount::H12;
    }

    return EReviewHistoryKLineCount::MAX;
}

//class
LocalConfig::LocalConfig()
{
    _localStorage = val::global("window")["localStorage"];

    _user = QByteArray::fromBase64(loadValue("user").toUtf8());
    _password = QByteArray::fromBase64(loadValue("password").toUtf8());
    _splitterPos = QByteArray::fromBase64(loadValue("splitter_pos").toUtf8());
    _autoScroll = loadValue("auto_scroll") == "0" ? false : true;
    _historyKLineCount = stringToEHistoryKLineCount(loadValue("history_kline_count"));
    _reviewHistoryKLineCount = stringToEReviewHistoryKLineCount(loadValue("review_history_kline_count"));
}

const QString &LocalConfig::user() const noexcept
{
    return _user;
}

void LocalConfig::setUser(const QString &user)
{
    _user = user;
    saveValue("user", _user.toUtf8().toBase64());
}

const QString &LocalConfig::password() const noexcept
{
    return _password;
}

void LocalConfig::setPassword(const QString &password)
{
    _password = password;
    saveValue("password", _password.toUtf8().toBase64());
}

QByteArray LocalConfig::splitterPos() const noexcept
{
    return _splitterPos;
}

void LocalConfig::setSplitterPos(const QByteArray& newPos)
{
    _splitterPos= newPos;
    saveValue("splitter_pos", QString(_splitterPos.toBase64()));
}

bool LocalConfig::autoScroll() const noexcept
{
    return _autoScroll;
}

void LocalConfig::setAutoScroll(bool autoscroll)
{
    _autoScroll = autoscroll;
    saveValue("auto_scroll", _autoScroll ? "1" : "0");
}

void LocalConfig::setHistoryKLineCount(EHistoryKLineCount count)
{
    _historyKLineCount = count;
    saveValue("history_kline_count", QString::number(static_cast<quint64>(_historyKLineCount)));
}

LocalConfig::EReviewHistoryKLineCount LocalConfig::reviewHistoryKLineCount() const noexcept
{
    return _reviewHistoryKLineCount;
}

void LocalConfig::setReviewHistoryKLineCount(EReviewHistoryKLineCount count)
{
    _reviewHistoryKLineCount = count;
    saveValue("review_history_kline_count", QString::number(static_cast<quint64>(_reviewHistoryKLineCount)));
}

LocalConfig::EHistoryKLineCount LocalConfig::historyKLineCount() const noexcept
{
    return _historyKLineCount;
}

void LocalConfig::saveValue(const QString &key, const QString &value)
{
    const std::string keyString = key.toStdString();
    const std::string valueString = value.toStdString();
    _localStorage.call<void>("setItem", keyString, valueString);
}

QString LocalConfig::loadValue(const QString &key)
{
    const std::string keyString = key.toStdString();
    const emscripten::val value = _localStorage.call<val>("getItem", keyString);
    if (value.isNull())
    {
        return "";
    }

    return QString::fromStdString(value.as<std::string>());
}
