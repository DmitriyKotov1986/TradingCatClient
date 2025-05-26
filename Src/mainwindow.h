#pragma once

//STL
#include <unordered_map>

//Qt
#include <QKeyEvent>
#include <QMainWindow>
#include <QChart>
#include <QChartView>
#include <QCandlestickSeries>
#include <QListWidgetItem>
#include <QSet>
#include <QMap>
#include <QHash>
#include <QComboBox>
#include <QThread>

//EM
#include <emscripten/html5.h>

//My
#include <TradingCatCommon/kline.h>
#include <TradingCatCommon/stockexchange.h>
#include <TradingCatCommon/transmitdata.h>
#include <TradingCatCommon/detector.h>

#include "networkcore.h"
#include "localconfig.h"
#include "eventlistmenu.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow
    : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool keyPress(const EmscriptenKeyboardEvent *keyEvent);

signals:
    void stopAll();

    void updateConfig(const TradingCatCommon::UserConfig& config);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    // NetworkCore
    void loginNetworkCore(const TradingCatCommon::UserConfig& userConfig);
    void logoutNetworkCore();
    void klineDetectNetworkCore(const TradingCatCommon::Detector::KLinesDetectedList& detectData);
    void stockExchangesNetworkCore(const TradingCatCommon::StockExchangesIDList& stockExchangesIdList);
    void klinesIdListNetworkCore(const TradingCatCommon::StockExchangeID& stockExchangesId, const TradingCatCommon::PKLinesIDList& klinesIdList);

    /*!
        Дополнительное сообщение логеру
        @param category - категория сообщения
        @param msg - текст сообщения
    */
    void sendLogMsgNetworkCore(Common::TDBLoger::MSG_CODE category, const QString& msg);

    //UI
    void mainTabWidgetCurrentChanged(int index);

    //Main tab
    void detectorSplitterSplitterMoved(int pos, int index);
    void eventListItemClicked(QListWidgetItem *item);

    void checkStateChangedAutoScrollCB(Qt::CheckState state);

    void historyMaxPBClicked();
    void history30minPBClicked();
    void history1hourPBClicked();
    void history2hoursPBClicked();

    void reviewHistory2hoursPBClicked();
    void reviewHistory6hoursPBClicked();
    void reviewHistory12hoursPBClicked();
    void reviewHistoryMaxPBClicked();

    //Filter tab
    void addPushButtonClicked();
    void removePushButtonClicked();


    void addBlackListPushButtonClicked();
    void removeBlackListPushButtonClicked();

    void customContextMenuRequestedEventList(const QPoint &pos);
    void clickedItemEventListMenu(EventListMenu::EMenuItemType type, quint64 index);

private:
    void makeChart();
    void makeReviewChart();

    void makeFilterTab();
    void clearFilterTab();

    void makeBlackListTab();
    void clearBlackListTab();

    void sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString& msg);

    void showChart(const TradingCatCommon::PKLinesList& klinesData, const TradingCatCommon::StockExchangeID& stockExchangeID);
    void showReviewChart(const TradingCatCommon::PKLinesList& klinesData, const TradingCatCommon::StockExchangeID& stockExchangeID);

    QListWidgetItem* addDetectToEventList(const TradingCatCommon::Detector::PKLineDetectData& detectData);

    void setHistoryCountButton(LocalConfig::EHistoryKLineCount count);
    void setReviewHistoryCountButton(LocalConfig::EReviewHistoryKLineCount count);
    void updateHistoryChart(quint64 index);
    void updateReviewHistoryChart(quint64 index);

    QComboBox* makeStockExchangeComboBox(const QString& stockExchange) const;
    QComboBox* makeSymbolComboBox(const TradingCatCommon::KLineID &klineId) const;

    void addFilterRow(const QString& stockExchange, double delta, double volume);
    void showRemovePushButton();

    void addBlackListRow(const QString& stockExchange, const TradingCatCommon::KLineID &klineId);
    void showRemoveBlackListPushButton();

    QColor stockExchangeColor(const TradingCatCommon::StockExchangeID& stockExchangeId);

    bool isLastDetected(const TradingCatCommon::StockExchangeID& stockExchangeId, const TradingCatCommon::KLineID& klineId, qint64 time);

private:
    Ui::MainWindow *ui;

    EventListMenu* _eventListMenu = nullptr;     ///< контекстное меню списка событий

    LocalConfig _localCnf;

    struct NetworcCoreThread
    {
        QThread thread;
        NetworkCore networkCore;

        explicit NetworcCoreThread(const LocalConfig& cfg)
            : thread()
            , networkCore(cfg)
        {}

        NetworcCoreThread() = delete;
        Q_DISABLE_COPY_MOVE(NetworcCoreThread);
    };

    std::unique_ptr<NetworcCoreThread> _networkCore;

    std::unordered_map<TradingCatCommon::StockExchangeID, TradingCatCommon::PKLinesIDList> _stockExchengeData;

    bool _login = false;
    TradingCatCommon::UserConfig _userConfig; //текущие настройки пользователя

    QCandlestickSeries *_series = nullptr;
    QCandlestickSeries *_seriesVolume = nullptr;
    QChartView *_chartView = nullptr;
    LocalConfig::EHistoryKLineCount _viewCount = LocalConfig::EHistoryKLineCount::MAX;

    QCandlestickSeries *_reviewSeries = nullptr;
    QCandlestickSeries *_reviewSeriesVolume = nullptr;
    QChartView *_reviewChartView = nullptr;
    LocalConfig::EReviewHistoryKLineCount _reviewCount = LocalConfig::EReviewHistoryKLineCount::MAX;

    quint64 _currentKLineIndex = 0;
    std::unordered_map<quint64, TradingCatCommon::Detector::PKLineDetectData> _getKLineDetectData;//список отфильтрованных свечей поступивших от сервера

    std::unordered_map<qint64, qint64> _lastDetected; ///< Список последних фотфильтрованных свечей. Ключ - хэш ИД свечи, значене - время детектирования
};

