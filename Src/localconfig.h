#pragma once

//Qt
#include <QString>
#include <QByteArray>

//EM
#include <emscripten.h>
#include <emscripten/val.h>

//My
#include <TradingCatCommon/detector.h>

class LocalConfig
{
public:
    enum class EHistoryKLineCount: quint64
    {
        MAX = TradingCatCommon::KLINES_COUNT_HISTORY,
        MIN30 = 30,
        H1 = 60,
        H2 = 120
    };

    static EHistoryKLineCount stringToEHistoryKLineCount(const QString& count);

    enum class EReviewHistoryKLineCount: quint64
    {
        MAX = TradingCatCommon::KLINES_COUNT_HISTORY,
        H2 = 24,
        H6 = 72,
        H12 = 144
    };

    static EReviewHistoryKLineCount stringToEReviewHistoryKLineCount(const QString& count);

public:
    LocalConfig();

    const QString& user() const noexcept;
    void setUser(const QString& user);

    const QString& password() const noexcept;
    void setPassword(const QString& password);

    QByteArray splitterPos() const noexcept;
    void setSplitterPos(const QByteArray& newPos);

    bool autoScroll() const noexcept;
    void setAutoScroll(bool autoscroll);

    EHistoryKLineCount historyKLineCount() const noexcept;
    void setHistoryKLineCount(EHistoryKLineCount count);

    EReviewHistoryKLineCount reviewHistoryKLineCount() const noexcept;
    void setReviewHistoryKLineCount(EReviewHistoryKLineCount count);

private:
    Q_DISABLE_COPY_MOVE(LocalConfig);

    void saveValue(const QString& key, const QString& value);
    QString loadValue(const QString& key);

private:
    emscripten::val _localStorage;

    QString _user;
    QString _password;
    QByteArray _splitterPos;
    bool _autoScroll = true;
    EHistoryKLineCount _historyKLineCount = EHistoryKLineCount::MAX;
    EReviewHistoryKLineCount _reviewHistoryKLineCount = EReviewHistoryKLineCount::MAX;

};
