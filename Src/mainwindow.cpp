//STL
#include <vector>
#include <algorithm>

//Qt
#include <QTimer>
#include <QUrl>
#include <QCandlestickSet>
#include <QBarCategoryAxis>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QRandomGenerator64>
#include <QClipboard>
#include <QRegularExpression>
#include <QMenu>
#include <QDialog>

//EM
#include <emscripten/key_codes.h>

#include "mainwindow.h"
#include "./ui_mainwindow.h"

//My
#include <TradingCatCommon/appserverprotocol.h>

using namespace Common;
using namespace TradingCatCommon;

constexpr static const int INDEX_ROLE = Qt::UserRole;
constexpr static const int KLINE_NAME_ROLE = Qt::UserRole + 1;

constexpr static const int MAX_ITEM_COUNT = 10000;

constexpr static const qint64 LAST_DETECT_TIMEOUT = 1000 * 60 * 5; //5min

Q_GLOBAL_STATIC_WITH_ARGS(const QString, STOCKEXCHANGE_NAME_ALL, ("ALL"));
Q_GLOBAL_STATIC_WITH_ARGS(const QString, SYMBOL_NAME_ALL, ("ALL"));

// static
MainWindow::MainWindow(QWidget *parent /* = nullptr */)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , _localCnf()
{
    qRegisterMetaType<TradingCatCommon::Detector::KLinesDetectedList>("TradingCatCommon::Detector::KLinesDetectedList");
    qRegisterMetaType<TradingCatCommon::StockExchangesIDList>("TradingCatCommon::StockExchangesIDList");
    qRegisterMetaType<Common::TDBLoger::MSG_CODE>("Common::TDBLoger::MSG_CODE");
    qRegisterMetaType<TradingCatCommon::UserConfig>("TradingCatCommon::UserConfig");

    //UI
    ui->setupUi(this);

    if (_localCnf.user().isEmpty() || _localCnf.password().isEmpty())
    {
        _localCnf.setUser(QString::number(QRandomGenerator64::global()->bounded(static_cast<qint64>(1), std::numeric_limits<qint64>().max())));
        _localCnf.setPassword(QString::number(QRandomGenerator64::global()->bounded(static_cast<qint64>(1), std::numeric_limits<qint64>().max())));
    }

    // Network Core
    _networkCore = std::make_unique<NetworcCoreThread>(_localCnf);
    _networkCore->networkCore.moveToThread(&_networkCore->thread);

    connect(&_networkCore->thread, SIGNAL(started()), &_networkCore->networkCore, SLOT(start()), Qt::DirectConnection);
    connect(&_networkCore->networkCore, SIGNAL(finished()), &_networkCore->thread, SLOT(quit()), Qt::DirectConnection);
    connect(this, SIGNAL(stopAll()), &_networkCore->networkCore, SLOT(stop()), Qt::QueuedConnection);

    connect(&_networkCore->networkCore, SIGNAL(login(const TradingCatCommon::UserConfig&)),
            SLOT(loginNetworkCore(const TradingCatCommon::UserConfig&)), Qt::QueuedConnection);
    connect(&_networkCore->networkCore, SIGNAL(logout()),
            SLOT(logoutNetworkCore()), Qt::QueuedConnection);
    connect(&_networkCore->networkCore, SIGNAL(klineDetect(const TradingCatCommon::Detector::KLinesDetectedList&)),
            SLOT(klineDetectNetworkCore(const TradingCatCommon::Detector::KLinesDetectedList&)), Qt::QueuedConnection);
    connect(&_networkCore->networkCore, SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&)),
            SLOT(sendLogMsgNetworkCore(Common::TDBLoger::MSG_CODE, const QString&)), Qt::QueuedConnection);
    connect(&_networkCore->networkCore, SIGNAL(stockExchanges(const TradingCatCommon::StockExchangesIDList&)),
            SLOT(stockExchangesNetworkCore(const TradingCatCommon::StockExchangesIDList&)), Qt::QueuedConnection);
    connect(&_networkCore->networkCore, SIGNAL(klinesIdList(const TradingCatCommon::StockExchangeID&, const TradingCatCommon::PKLinesIDList&)),
            SLOT(klinesIdListNetworkCore(const TradingCatCommon::StockExchangeID&, const TradingCatCommon::PKLinesIDList&)), Qt::QueuedConnection);

    connect(this, SIGNAL(updateConfig(const TradingCatCommon::UserConfig&)),
            &_networkCore->networkCore, SLOT(updateConfig(const TradingCatCommon::UserConfig&)), Qt::QueuedConnection);

    // UI
    connect(ui->detectorSplitter, SIGNAL(splitterMoved(int, int)),
            SLOT(detectorSplitterSplitterMoved(int, int)));
    connect(ui->eventsList, SIGNAL(itemClicked(QListWidgetItem *)),
            SLOT(eventListItemClicked(QListWidgetItem *)));

    connect(ui->autoscrollCB, SIGNAL(checkStateChanged(Qt::CheckState)),
            SLOT(checkStateChangedAutoScrollCB(Qt::CheckState)));

    ui->eventsList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->eventsList, SIGNAL(customContextMenuRequested(const QPoint&)), SLOT(customContextMenuRequestedEventList(const QPoint&)));

    connect(ui->historyMaxPB, SIGNAL(clicked()), SLOT(historyMaxPBClicked()));
    connect(ui->history30minPB, SIGNAL(clicked()), SLOT(history30minPBClicked()));
    connect(ui->history1hourPB, SIGNAL(clicked()), SLOT(history1hourPBClicked()));
    connect(ui->history2hoursPB, SIGNAL(clicked()), SLOT(history2hoursPBClicked()));

    connect(ui->reviewHistory2hoursPB, SIGNAL(clicked()), SLOT(reviewHistory2hoursPBClicked()));
    connect(ui->reviewHistory6hoursPB, SIGNAL(clicked()), SLOT(reviewHistory6hoursPBClicked()));
    connect(ui->reviewHistory12hoursPB, SIGNAL(clicked()), SLOT(reviewHistory12hoursPBClicked()));
    connect(ui->reviewHistoryMaxPB, SIGNAL(clicked()), SLOT(reviewHistoryMaxPBClicked()));

    connect(ui->addPushButton, SIGNAL(clicked()), SLOT(addPushButtonClicked()));
    connect(ui->removePushButton, SIGNAL(clicked()), SLOT(removePushButtonClicked()));
    connect(ui->addBlackListPushButton, SIGNAL(clicked()), SLOT(addBlackListPushButtonClicked()));
    connect(ui->removeBlackListPushButton, SIGNAL(clicked()), SLOT(removeBlackListPushButtonClicked()));

    connect(ui->mainTabWidget, SIGNAL(currentChanged(int)), SLOT(mainTabWidgetCurrentChanged(int)));

    if (!_localCnf.splitterPos().isEmpty())
    {
        ui->detectorSplitter->restoreState(_localCnf.splitterPos());
    }

    ui->mainTabWidget->setCurrentIndex(0);

    makeChart();
    makeReviewChart();

    _eventListMenu = new EventListMenu(this);
    _eventListMenu->setVisible(false);

    connect(_eventListMenu, SIGNAL(clickedItem(EventListMenu::EMenuItemType, quint64)),
            SLOT(clickedItemEventListMenu(EventListMenu::EMenuItemType, quint64)));

    //start
    QTimer::singleShot(10, this,
                       [this]()
                       {
                           _reviewChartView->show();
                           _chartView->show();

                           resizeEvent(nullptr);

                           setHistoryCountButton(_localCnf.historyKLineCount());
                           setReviewHistoryCountButton(_localCnf.reviewHistoryKLineCount());

                           ui->autoscrollCB->setChecked(_localCnf.autoScroll());

                           ui->eventsList->setCurrentRow(0);

                           _networkCore->thread.start();
                       });
}

MainWindow::~MainWindow()
{
    Q_CHECK_PTR(ui);

    emit stopAll();

    delete _eventListMenu;

    _networkCore->thread.wait();

    _networkCore.reset();

    delete ui;
}

bool MainWindow::keyPress(const EmscriptenKeyboardEvent *keyEvent)
{
    // qDebug() << "User press key:" << keyEvent->key << "Code:" << keyEvent->keyCode;

    if (keyEvent->keyCode == DOM_VK_UP)
    {
        const auto& eventList = ui->eventsList;
        const auto currentRow = eventList->currentRow();
        for (int row = currentRow - 1; row >= 0 ; --row)
        {
            const auto item = eventList->item(row);
            if (!item->data(INDEX_ROLE).isNull())
            {
                eventListItemClicked(item);
                break;
            }
        }

        return true;
    }
    else  if (keyEvent->keyCode == DOM_VK_DOWN)
    {
        const auto& eventList = ui->eventsList;
        const auto currentRow = eventList->currentRow();
        for (int row = currentRow + 1; row < eventList->count() ; ++row)
        {
            const auto item = eventList->item(row);
            if (!item->data(INDEX_ROLE).isNull())
            {
                eventListItemClicked(item);
                break;
            }
        }

        return true;
    }

    return false;
}


void MainWindow::eventListItemClicked(QListWidgetItem *item)
{  
    if (item->data(INDEX_ROLE).isNull())
    {
        return;
    }

    const auto index = item->data(INDEX_ROLE).toULongLong();

    Q_ASSERT(index != 0);

    if (index == _currentKLineIndex)
    {
        return;
    }

    const auto& klineData = _getKLineDetectData.at(index);

    showChart(klineData->history, klineData->stockExchangeId);
    showReviewChart(klineData->reviewHistory, klineData->stockExchangeId);

    ui->eventsList->setCurrentItem(item);

    _currentKLineIndex = index;
}

void MainWindow::checkStateChangedAutoScrollCB(Qt::CheckState state)
{
    _localCnf.setAutoScroll(state == Qt::Checked);
}

void MainWindow::historyMaxPBClicked()
{
    setHistoryCountButton(LocalConfig::EHistoryKLineCount::MAX);
    updateHistoryChart(_currentKLineIndex);
}

void MainWindow::history30minPBClicked()
{
    setHistoryCountButton(LocalConfig::EHistoryKLineCount::MIN30);
    updateHistoryChart(_currentKLineIndex);
}

void MainWindow::history1hourPBClicked()
{
    setHistoryCountButton(LocalConfig::EHistoryKLineCount::H1);
    updateHistoryChart(_currentKLineIndex);
}

void MainWindow::history2hoursPBClicked()
{
    setHistoryCountButton(LocalConfig::EHistoryKLineCount::H2);
    updateHistoryChart(_currentKLineIndex);
}

void MainWindow::reviewHistory2hoursPBClicked()
{
    setReviewHistoryCountButton(LocalConfig::EReviewHistoryKLineCount::H2);
    updateReviewHistoryChart(_currentKLineIndex);
}

void MainWindow::reviewHistory6hoursPBClicked()
{
    setReviewHistoryCountButton(LocalConfig::EReviewHistoryKLineCount::H6);
    updateReviewHistoryChart(_currentKLineIndex);
}

void MainWindow::reviewHistory12hoursPBClicked()
{
    setReviewHistoryCountButton(LocalConfig::EReviewHistoryKLineCount::H12);
    updateReviewHistoryChart(_currentKLineIndex);
}

void MainWindow::reviewHistoryMaxPBClicked()
{
    setReviewHistoryCountButton(LocalConfig::EReviewHistoryKLineCount::MAX);
    updateReviewHistoryChart(_currentKLineIndex);
}

void MainWindow::detectorSplitterSplitterMoved(int pos, int index)
{
    Q_UNUSED(pos);
    Q_UNUSED(index);

    resizeEvent(nullptr);
}

void MainWindow::addPushButtonClicked()
{
    addFilterRow(QString(), 5.0, 1000.0);
}

void MainWindow::removePushButtonClicked()
{
    auto& filterTable = ui->filterTableWidget;
    filterTable->removeRow(filterTable->currentRow());

    showRemovePushButton();
}

void MainWindow::addBlackListPushButtonClicked()
{
    addBlackListRow(QString(), KLineID());
}

void MainWindow::removeBlackListPushButtonClicked()
{
    auto& blackListTable = ui->blackListTableWidget;
    blackListTable->removeRow(blackListTable->currentRow());

    showRemoveBlackListPushButton();
}

void MainWindow::customContextMenuRequestedEventList(const QPoint &pos)
{
    Q_CHECK_PTR(_eventListMenu);

    const auto& eventList = ui->eventsList;
    const auto item = eventList->currentItem();
    const auto data = item->data(INDEX_ROLE);
    if (!data.isNull())
    {
        _eventListMenu->open(ui->eventsList->mapToGlobal(pos), data.toULongLong());
    }
}

void MainWindow::clickedItemEventListMenu(EventListMenu::EMenuItemType type, quint64 index)
{
    //qDebug() << "Event list menu clicked" << static_cast<quint8>(type) << "Index" << index;

    const auto it_getKLineDetectData = _getKLineDetectData.find(index);
    if (it_getKLineDetectData == _getKLineDetectData.end())
    {
        return;
    }

    const auto& klineData = it_getKLineDetectData->second;
    const auto& klineId = klineData->history->front()->id;
    const auto& stockExchangeId = klineData->stockExchangeId;

    switch (type)
    {
    case EventListMenu::EMenuItemType::ADD_BLACK_LIST:
        addBlackListRow(stockExchangeId.name, klineId);
        mainTabWidgetCurrentChanged(0);
        break;
    case EventListMenu::EMenuItemType::ADD_BLACK_LIST_ALL:
        addBlackListRow(QString(), klineId);
        mainTabWidgetCurrentChanged(0);
        break;
    default:
        break;
    }
}

void MainWindow::mainTabWidgetCurrentChanged(int index)
{
    if (index != 0 || !_login)
    {
        return;
    }

    _userConfig.clearFilter();
    const auto& filterTableWidget = ui->filterTableWidget;
    for (int row = 0; row < filterTableWidget->rowCount(); ++row)
    {
        KLineFilterData tmp;

        const auto stockExchangeName = static_cast<QComboBox*>(filterTableWidget->cellWidget(row, 0))->currentText();
        if (stockExchangeName != *STOCKEXCHANGE_NAME_ALL)
        {
            tmp.setStockExchangeID(StockExchangeID(stockExchangeName));
        }

        tmp.setDelta(static_cast<QDoubleSpinBox*>(filterTableWidget->cellWidget(row, 1))->value());
        tmp.setVolume(static_cast<QDoubleSpinBox*>(filterTableWidget->cellWidget(row, 2))->value());

        _userConfig.addFilterData(std::move(tmp));
    }

    const auto& blackListTabletWidget = ui->blackListTableWidget;
    for (int row = 0; row < blackListTabletWidget->rowCount(); ++row)
    {
        BlackListFilterData tmp;

        const auto stockExchangeName = static_cast<QComboBox*>(blackListTabletWidget->cellWidget(row, 0))->currentText();
        if (stockExchangeName != *STOCKEXCHANGE_NAME_ALL)
        {
            tmp.setStockExchangeID(StockExchangeID(stockExchangeName));
        }

        const auto symbol = static_cast<QComboBox*>(blackListTabletWidget->cellWidget(row, 1))->currentData().toString();
        if (symbol != *SYMBOL_NAME_ALL)
        {
            tmp.setKLineID(KLineID(symbol, KLineType::MIN1));
        }

        _userConfig.addBlackListData(std::move(tmp));
    }

    emit updateConfig(_userConfig);
}

void MainWindow::makeChart()
{
    //Chart
    _series = new QCandlestickSeries;
    _series->setIncreasingColor(QColor(Qt::green));
    _series->setDecreasingColor(QColor(Qt::red));
    _series->setBodyOutlineVisible(true);
    _series->setBodyWidth(0.7);
    _series->setCapsVisible(true);
    _series->setCapsWidth(0.5);
    _series->setMinimumColumnWidth(-1.0);
    _series->setMaximumColumnWidth(50.0);
    _series->setUseOpenGL(true);
    auto pen = _series->pen();
    pen.setColor(Qt::white);
    _series->setPen(pen);

    _seriesVolume = new QCandlestickSeries;
    _seriesVolume->setIncreasingColor(QColor(61, 56, 70));
    _seriesVolume->setDecreasingColor(QColor(61, 56, 70));
    _seriesVolume->setBodyOutlineVisible(true);
    _seriesVolume->setCapsVisible(true);
    _seriesVolume->setMinimumColumnWidth(-1.0);
    _seriesVolume->setMaximumColumnWidth(50.0);
    _seriesVolume->setCapsWidth(0.3);
    _seriesVolume->setBodyWidth(0.5);
    _seriesVolume->setUseOpenGL(true);
    _seriesVolume->setPen(pen);

    auto chart = new QChart;
    chart->addSeries(_seriesVolume);
    chart->addSeries(_series);
    chart->setTitle("No data");
//    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setBackgroundBrush(QBrush(QColor(36, 31, 49)));
    auto titleFont = chart->titleFont();
    titleFont.setPointSize(20);
    chart->setTitleFont(titleFont);
    chart->legend()->setVisible(false);
    chart->setTitle("No data");
    chart->setMargins({0,0,0,0});

    auto axisX = new QDateTimeAxis;
    axisX->setTickCount(3);
    axisX->setFormat("hh:mm");
    axisX->setGridLineVisible(false);
    axisX->setLabelsColor(Qt::white);
    auto axisXLabelsFont = axisX->labelsFont();
    axisXLabelsFont.setPixelSize(12);
    axisX->setLabelsFont(axisXLabelsFont);

    chart->addAxis(axisX, Qt::AlignBottom);

    _series->attachAxis(axisX);
    _seriesVolume->attachAxis(axisX);

    auto axisY = new QValueAxis();
    axisY->setGridLineVisible(true);
    axisY->setGridLineColor(QColor(94, 92, 100));
    auto axisYGridLinePen = axisY->gridLinePen();
    axisYGridLinePen.setStyle(Qt::DotLine);
    axisY->setGridLinePen(axisYGridLinePen);
    axisY->setLabelsColor(Qt::white);
    auto axisYLabelsFont = axisY->labelsFont();
    axisYLabelsFont.setPixelSize(12);
    axisY->setLabelsFont(axisYLabelsFont);

    chart->addAxis(axisY, Qt::AlignRight);

    _series->attachAxis(axisY);

    auto axisY2 = new QValueAxis();
    axisY2->setGridLineVisible(false);
    axisY2->setLabelsVisible(false);

    chart->addAxis(axisY2, Qt::AlignLeft);

    _seriesVolume->attachAxis(axisY2);

    _chartView = new QChartView(chart, ui->chartFrame);
    _chartView->resize(ui->chartFrame->size());
    _chartView->show();
}

void MainWindow::makeReviewChart()
{
    //ReviewChart
    _reviewSeries = new QCandlestickSeries;
    _reviewSeries->setIncreasingColor(QColor(Qt::green));
    _reviewSeries->setDecreasingColor(QColor(Qt::red));
    _reviewSeries->setBodyOutlineVisible(true);
    _reviewSeries->setBodyWidth(0.5);
    _reviewSeries->setMinimumColumnWidth(-1.0);
    _reviewSeries->setMaximumColumnWidth(50.0);
    _reviewSeries->setCapsVisible(true);
    _reviewSeries->setCapsWidth(0.3);
    _reviewSeries->setUseOpenGL(true);
    auto pen = _series->pen();
    pen.setWidth(1);
    pen.setColor(Qt::white);
    _reviewSeries->setPen(pen);

    _reviewSeriesVolume = new QCandlestickSeries;
    _reviewSeriesVolume->setIncreasingColor(QColor(61, 56, 70));
    _reviewSeriesVolume->setDecreasingColor(QColor(61, 56, 70));
    _reviewSeriesVolume->setBodyOutlineVisible(true);
    _reviewSeriesVolume->setCapsVisible(true);
    _reviewSeriesVolume->setMinimumColumnWidth(-1.0);
    _reviewSeriesVolume->setMaximumColumnWidth(50.0);
    _reviewSeriesVolume->setCapsWidth(0.3);
    _reviewSeriesVolume->setBodyWidth(0.5);
    _reviewSeriesVolume->setUseOpenGL(true);
    _reviewSeriesVolume->setPen(pen);

    auto chart = new QChart;
    chart->addSeries(_reviewSeriesVolume);
    chart->addSeries(_reviewSeries);
//    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setBackgroundBrush(QBrush(QColor(36, 31, 49)));
    auto titleFont = chart->titleFont();
    titleFont.setPointSize(20);
    chart->setTitleFont(titleFont);
    chart->legend()->setVisible(false);
    chart->setTitle("No data");
    chart->setMargins({0,0,0,0});

    auto axisX = new QDateTimeAxis;
    axisX->setTickCount(3);
    axisX->setFormat("hh:mm");
    axisX->setGridLineVisible(false);
    axisX->setLabelsColor(Qt::white);
    auto axisXLabelsFont = axisX->labelsFont();
    axisXLabelsFont.setPixelSize(12);
    axisX->setLabelsFont(axisXLabelsFont);

    chart->addAxis(axisX, Qt::AlignBottom);

    _reviewSeries->attachAxis(axisX);
    _reviewSeriesVolume->attachAxis(axisX);

    auto axisY = new QValueAxis();
    axisY->setGridLineVisible(true);
    axisY->setGridLineColor(QColor(94, 92, 100));
    auto axisYGridLinePen = axisY->gridLinePen();
    axisYGridLinePen.setStyle(Qt::DotLine);
    axisY->setGridLinePen(axisYGridLinePen);
    axisY->setLabelsColor(Qt::white);
    auto axisYLabelsFont = axisY->labelsFont();
    axisYLabelsFont.setPixelSize(12);
    axisY->setLabelsFont(axisYLabelsFont);

    chart->addAxis(axisY, Qt::AlignRight);

    _reviewSeries->attachAxis(axisY);

    auto axisY2 = new QValueAxis();
    axisY2->setGridLineVisible(false);
    axisY2->setLabelsVisible(false);

    chart->addAxis(axisY2, Qt::AlignLeft);

    _reviewSeriesVolume->attachAxis(axisY2);

    _reviewChartView = new QChartView(chart, ui->reviewChartFrame);
    _reviewChartView->resize(ui->reviewChartFrame->size());
    _reviewChartView->show();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    if (_chartView != nullptr)
    {
        _chartView->resize(ui->chartFrame->size());
        _chartView->update();
    }

    if (_reviewChartView != nullptr)
    {
        _reviewChartView->resize(ui->reviewChartFrame->size());
        _reviewChartView->update();
    }

    _localCnf.setSplitterPos(ui->detectorSplitter->saveState());
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_CHECK_PTR(_eventListMenu);

    if (event->type() == QEvent::MouseButtonDblClick)
    {
        const auto& eventList = ui->eventsList;
        const auto item = eventList->currentItem();
        const auto data = item->data(INDEX_ROLE);
        if (!data.isNull())
        {
            const auto& detect = _getKLineDetectData.at(data.toULongLong());
            const auto& klineId = detect->history->front()->id;

            auto clipboard = QApplication::clipboard();
            clipboard->setText(klineId.baseName());

            qDebug() << "Symbol name copy to clipboard:" << clipboard->text();

            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        if (!_eventListMenu->isHidden())
        {
            QTimer::singleShot(10, this, [this](){ _eventListMenu->close(); });
        }
    }

    // pass the event on to the parent class
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::loginNetworkCore(const TradingCatCommon::UserConfig &userConfig)
{
    _userConfig = userConfig;
}

void MainWindow::logoutNetworkCore()
{
    auto item = new QListWidgetItem(QString("Logout. Try to reconnection to server"));
    item->setIcon(QIcon(":/icon/img/error.ico"));
    item->setFlags(Qt::ItemFlag::ItemIsEnabled);
    ui->eventsList->addItem(item);

    clearFilterTab();
    clearBlackListTab();

    _stockExchengeData.clear();

    _login = false;  
}

void MainWindow::klineDetectNetworkCore(const TradingCatCommon::Detector::KLinesDetectedList &detectData)
{
    Q_ASSERT(!detectData.detected.empty());

    QListWidgetItem* item = nullptr;
    for (const auto& detect: detectData.detected)
    {
        item = addDetectToEventList(detect);
    }

    Q_CHECK_PTR(item);

    if (detectData.isFull)
    {
        auto item = new QListWidgetItem(QString("There are too many detections. Some events were skipped"));
        item->setFlags(Qt::ItemFlag::ItemIsEnabled);
        ui->eventsList->addItem(item);
    }

    if (ui->autoscrollCB->isChecked())
    {
        eventListItemClicked(item);

        ui->eventsList->scrollToBottom();
    }
}

void MainWindow::stockExchangesNetworkCore(const TradingCatCommon::StockExchangesIDList& stockExchangesIdList)
{
    _stockExchengeData.clear();
    for (const auto& stockExchnageId: stockExchangesIdList)
    {
        _stockExchengeData.emplace(stockExchnageId, std::make_shared<TradingCatCommon::KLinesIDList>());
    }

    //далее ждем данных о доступных свечах
}

void MainWindow::klinesIdListNetworkCore(const TradingCatCommon::StockExchangeID &stockExchangesId, const TradingCatCommon::PKLinesIDList &klinesIdList)
{
    Q_ASSERT(_stockExchengeData.contains(stockExchangesId));
    Q_CHECK_PTR(klinesIdList);

    _stockExchengeData.at(stockExchangesId) = klinesIdList;

    qDebug() << "Add stockExhange" << stockExchangesId.toString() << klinesIdList->size();
    // выполняем логин, как только данные по всем биржам получены
    if (!_login)
    {
        const auto& isAll = std::all_of(_stockExchengeData.begin(), _stockExchengeData.end(),
                                        [](const auto& item)
                                        {
                                            return !item.second->empty();
                                        });

        if (isAll)
        {
            auto item = new QListWidgetItem(QString("Login successfully as: %1").arg(_localCnf.user()));
            item->setIcon(QIcon(":/image/img/ok.png"));
            item->setFlags(Qt::ItemFlag::ItemIsEnabled);
            ui->eventsList->addItem(item);

            _login = true;

            makeFilterTab();
            makeBlackListTab();
        }
    }
}

void MainWindow::sendLogMsgNetworkCore(Common::TDBLoger::MSG_CODE category, const QString &msg)
{
    sendLogMsg(category, QString("Network core: %1").arg(msg));
}

void MainWindow::sendLogMsg(TDBLoger::MSG_CODE category, const QString &msg)
{
    switch (category)
    {
    case TDBLoger::MSG_CODE::DEBUG_CODE:
#ifdef QT_DEBUG
        qDebug() << msg;
        break;
#else
        return;
#endif
    case TDBLoger::MSG_CODE::INFORMATION_CODE:
        qInfo() << msg;
        break;
    case TDBLoger::MSG_CODE::WARNING_CODE:
        qWarning() << msg;
        break;
    case TDBLoger::MSG_CODE::CRITICAL_CODE:
        qCritical() << msg;
        break;
    case TDBLoger::MSG_CODE::FATAL_CODE:
        qFatal() << msg;
        break;
    default:
        Q_ASSERT(false);
    }
}

void MainWindow::showChart(const TradingCatCommon::PKLinesList &klinesData, const TradingCatCommon::StockExchangeID &stockExchangeID)
{
    Q_CHECK_PTR(_series);
    Q_CHECK_PTR(_chartView);

    Q_ASSERT(!klinesData->empty());
    Q_ASSERT(!stockExchangeID.isEmpty());

    const auto& firstKLine = klinesData->front();
    auto& lastKLine = klinesData->back();
    const auto& klineId = firstKLine->id;

    _chartView->chart()->setTitle(QString("%1: %2 %3")
                                      .arg(stockExchangeID.name)
                                      .arg(klineId.symbol)
                                      .arg(KLineTypeToString(klineId.type)));

    float max = std::numeric_limits<float>::min();
    float min = std::numeric_limits<float>::max();
    float maxVolume = std::numeric_limits<float>::min();

    quint64 index = 0;
    const auto count = static_cast<quint64>(_viewCount);
    QList<QCandlestickSet*> candlestickList;
    candlestickList.reserve(count);
    QList<QCandlestickSet*> candlestickVolumeList;
    candlestickVolumeList.reserve(count);
    for (const auto& kline: *klinesData)
    {
        if (index >= count)
        {
            break;
        }

        ++index;

        {
            auto candlestick = new QCandlestickSet(kline->closeTime);
            candlestick->setHigh(kline->high);
            candlestick->setLow(kline->low);
            candlestick->setOpen(kline->open);
            candlestick->setClose(kline->open != kline->close ? kline->close : kline->close + 0.001 * kline->close);
            auto pen = _series->pen();
            pen.setColor(kline->open <= kline->close ? QColor(Qt::green) : QColor(Qt::red));
            candlestick->setPen(pen);

            candlestickList.push_back(candlestick);
        }

        {
            auto candlestickVolume = new QCandlestickSet(kline->closeTime);
            candlestickVolume->setOpen(kline->volume);
            candlestickVolume->setHigh(kline->volume);
            candlestickVolume->setLow(0);
            candlestickVolume->setClose(0);
            auto penVolume = _seriesVolume->pen();
            penVolume.setColor(QColor(61, 56, 70));
            candlestickVolume->setPen(penVolume);

            candlestickVolumeList.push_back(candlestickVolume);
        }

        max = std::max(max, kline->high);
        min = std::min(min, kline->low);
        maxVolume = std::max(maxVolume, kline->volume);

        lastKLine = kline;
    }

    _chartView->hide();

    auto axisX = qobject_cast<QDateTimeAxis*>(_chartView->chart()->axes(Qt::Horizontal).at(0));
    axisX->setMax(QDateTime::fromMSecsSinceEpoch(firstKLine->closeTime + static_cast<qint64>(klineId.type) * 5));
    axisX->setMin(QDateTime::fromMSecsSinceEpoch(lastKLine->closeTime - static_cast<qint64>(klineId.type)));
    axisX->setTickCount(5);

    auto axisY = qobject_cast<QValueAxis*>(_chartView->chart()->axes(Qt::Vertical).at(0));
    const auto fivePercent = (max - min) / 3.0;
    axisY->setMax(max  +  fivePercent);
    axisY->setMin(min -  fivePercent);

    auto axisY2 = qobject_cast<QValueAxis*>(_chartView->chart()->axes(Qt::Vertical).at(1));
    axisY2->setMax(maxVolume * 2);
    axisY2->setMin(-0.01f);

    _series->clear();
    _seriesVolume->clear();

    _series->append(candlestickList);
    _seriesVolume->append(candlestickVolumeList);

    _chartView->show();
}

void MainWindow::showReviewChart(const TradingCatCommon::PKLinesList &klinesData, const TradingCatCommon::StockExchangeID &stockExchangeID)
{
    Q_CHECK_PTR(_reviewSeries);
    Q_CHECK_PTR(_reviewChartView);

    Q_ASSERT(!klinesData->empty());
    Q_ASSERT(!stockExchangeID.isEmpty());

    const auto& firstKLine = klinesData->front();
    auto& lastKLine = klinesData->back();
    const auto& klineId = firstKLine->id;

    _reviewChartView->chart()->setTitle(QString("%2 %3")
                                            .arg(klineId.symbol)
                                            .arg(KLineTypeToString(klineId.type)));

    float max = std::numeric_limits<float>::min();
    float min = std::numeric_limits<float>::max();
    float maxVolume = std::numeric_limits<float>::min();

    quint64 index = 0;
    const auto count = static_cast<quint64>(_reviewCount);
    QList<QCandlestickSet*> candlestickList;
    candlestickList.reserve(count);
    QList<QCandlestickSet*> candlestickVolumeList;
    candlestickVolumeList.reserve(count);
    for (const auto& kline: *klinesData)
    {
        if (index >= count)
        {
            break;
        }

        ++index;

        {
            auto candlestick = new QCandlestickSet(kline->closeTime);

            candlestick->setHigh(kline->high);
            candlestick->setLow(kline->low);
            candlestick->setOpen(kline->open);
            candlestick->setClose(kline->open != kline->close ? kline->close : kline->close + 0.001 * kline->close);
            auto pen = _series->pen();
            pen.setColor(kline->open <= kline->close ? QColor(Qt::green) : QColor(Qt::red));
            candlestick->setPen(pen);

            candlestickList.push_back(candlestick);
        }

        {
            auto candlestickVolume = new QCandlestickSet(kline->closeTime);
            candlestickVolume->setOpen(kline->volume);
            candlestickVolume->setHigh(kline->volume);
            candlestickVolume->setLow(0);
            candlestickVolume->setClose(0);
            auto penVolume = _seriesVolume->pen();
            penVolume.setColor(QColor(61, 56, 70));
            candlestickVolume->setPen(penVolume);

            candlestickVolumeList.push_back(candlestickVolume);
        }

        max = std::max(max, kline->high);
        min = std::min(min, kline->low);
        maxVolume = std::max(maxVolume, kline->volume);

        lastKLine= kline;
    }

    _reviewChartView->hide();

    auto axisX = qobject_cast<QDateTimeAxis*>(_reviewChartView->chart()->axes(Qt::Horizontal).at(0));
    axisX->setMax(QDateTime::fromMSecsSinceEpoch(firstKLine->closeTime + static_cast<qint64>(klineId.type) * 5));
    axisX->setMin(QDateTime::fromMSecsSinceEpoch(lastKLine->closeTime- static_cast<qint64>(klineId.type)));
    axisX->setTickCount(5);

    auto axisY = qobject_cast<QValueAxis*>(_reviewChartView->chart()->axes(Qt::Vertical).at(0));
    const auto fivePercent = (max - min) / 3.0;
    axisY->setMax(max  +  fivePercent);
    axisY->setMin(min -  fivePercent);

    auto axisY2 = qobject_cast<QValueAxis*>(_reviewChartView->chart()->axes(Qt::Vertical).at(1));
    axisY2->setMax(maxVolume * 2);
    axisY2->setMin(-0.01f);

    _reviewSeries->clear();
    _reviewSeriesVolume->clear();

    _reviewSeries->append(candlestickList);
    _reviewSeriesVolume->append(candlestickVolumeList);

    _reviewChartView->show();
}

QListWidgetItem* MainWindow::addDetectToEventList(const TradingCatCommon::Detector::PKLineDetectData &detectData)
{
    Q_ASSERT(!detectData->history->empty());
    Q_ASSERT(!detectData->reviewHistory->empty());
    Q_ASSERT(!detectData->stockExchangeId.isEmpty());

    const auto& kline = detectData->history->front();
    const auto stockExchangeName = detectData->stockExchangeId.toString();

    const QString text = QString("%1->%2 Delta=%3 Volume=%4")
                             .arg(detectData->stockExchangeId.toString())
                             .arg(kline->id.symbol)
                             .arg(detectData->delta)
                             .arg(detectData->volume);

    qInfo() << "Detect:" << detectData->msg;

    auto item = new QListWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    static quint64 lastIDKLine = 0;

    item->setData(INDEX_ROLE, ++lastIDKLine);
    item->setData(KLINE_NAME_ROLE, kline->id.symbol);
    _getKLineDetectData.emplace(lastIDKLine, detectData);

    item->setForeground(stockExchangeColor(detectData->stockExchangeId));

    const auto isDetected = isLastDetected(detectData->stockExchangeId, kline->id, kline->closeTime);
    if (kline->open <= kline->close)
    {
        item->setIcon(isDetected ? QIcon(":/image/img/increase_star.png") : QIcon(":/image/img/increase.png"));
    }
    else
    {
        item->setIcon(isDetected ? QIcon(":/image/img/decrease_star.png") : QIcon(":/image/img/decrease.png"));
    }

    while (ui->eventsList->count() >= MAX_ITEM_COUNT)
    {
        auto item = ui->eventsList->takeItem(0);
        const auto index = item->data(INDEX_ROLE);
        if (!index.isNull())
        {
            _getKLineDetectData.erase(index.toULongLong());
        }

        delete item;
    }

    ui->eventsList->addItem(item);

    return item;
}

void MainWindow::setHistoryCountButton(LocalConfig::EHistoryKLineCount count)
{
    _viewCount = count;

    ui->historyMaxPB->setChecked(false);
    ui->history30minPB->setChecked(false);
    ui->history1hourPB->setChecked(false);
    ui->history2hoursPB->setChecked(false);

    switch (_viewCount)
    {
    case LocalConfig::EHistoryKLineCount::MAX:
        ui->historyMaxPB->setChecked(true);
        break;
    case LocalConfig::EHistoryKLineCount::MIN30:
        ui->history30minPB->setChecked(true);
        break;
    case LocalConfig::EHistoryKLineCount::H1:
        ui->history1hourPB->setChecked(true);
        break;
    case LocalConfig::EHistoryKLineCount::H2:
        ui->history2hoursPB->setChecked(true);
        break;
    default:
        Q_ASSERT(false);
    }

    _localCnf.setHistoryKLineCount(count);
}

void MainWindow::setReviewHistoryCountButton(LocalConfig::EReviewHistoryKLineCount count)
{
    _reviewCount = count;

    ui->reviewHistory2hoursPB->setChecked(false);
    ui->reviewHistory6hoursPB->setChecked(false);
    ui->reviewHistory12hoursPB->setChecked(false);
    ui->reviewHistoryMaxPB->setChecked(false);

    switch (_reviewCount)
    {
    case LocalConfig::EReviewHistoryKLineCount::MAX:
        ui->reviewHistoryMaxPB->setChecked(true);
        break;
    case LocalConfig::EReviewHistoryKLineCount::H2:
        ui->reviewHistory2hoursPB->setChecked(true);
        break;
    case LocalConfig::EReviewHistoryKLineCount::H6:
        ui->reviewHistory6hoursPB->setChecked(true);
        break;
    case LocalConfig::EReviewHistoryKLineCount::H12:
        ui->reviewHistory12hoursPB->setChecked(true);
        break;
    default:
        Q_ASSERT(false);
    }

    _localCnf.setReviewHistoryKLineCount(count);
}

void MainWindow::updateHistoryChart(quint64 index)
{
    if (index != 0)
    {
        const auto klineData = _getKLineDetectData.at(_currentKLineIndex);

        showChart(klineData->history, klineData->stockExchangeId);
    }
}

void MainWindow::updateReviewHistoryChart(quint64 index)
{
    if (index != 0)
    {
        const auto klineData = _getKLineDetectData.at(_currentKLineIndex);

        showReviewChart(klineData->reviewHistory, klineData->stockExchangeId);
    }
}

QComboBox* MainWindow::makeStockExchangeComboBox(const QString &stockExchange) const
{
    auto stockExchangeComboBox = new QComboBox();
    stockExchangeComboBox->setEditable(false);
    stockExchangeComboBox->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    stockExchangeComboBox->addItem(*STOCKEXCHANGE_NAME_ALL);

    QStringList stockExchangeNames;
    for (const auto& stockExchangeData: _stockExchengeData)
    {
        stockExchangeNames.push_back(stockExchangeData.first.name);
    }
    stockExchangeNames.sort();

    for (const auto& stockExchangeName: stockExchangeNames)
    {
        stockExchangeComboBox->addItem(QIcon(QString(":/image/img/%1.png").arg(stockExchangeName)), stockExchangeName);
    }

    stockExchangeComboBox->setCurrentText(stockExchange);

    return stockExchangeComboBox;
}

QComboBox *MainWindow::makeSymbolComboBox(const TradingCatCommon::KLineID &klineId) const
{
    auto symbolComboBox = new QComboBox();
    symbolComboBox->setEditable(false);

    symbolComboBox->addItem(*SYMBOL_NAME_ALL, QVariant(*SYMBOL_NAME_ALL));

    std::vector<KLineID> klinesIdList;
    if (!klineId.isEmpty())
    {
        klinesIdList.push_back(klineId);
    }

    for(const auto& stockExchengeData: _stockExchengeData)
    {
        for (const auto& klineIdInStock: *stockExchengeData.second)
        {
            if (klineIdInStock.type == TradingCatCommon::KLineType::MIN1)
            {
                klinesIdList.push_back(klineIdInStock);
            }
        }
    }

    std::sort(klinesIdList.begin(), klinesIdList.end(),
              [](const auto& klineIdLeft, const auto& klineIdRight)
              {
                  return klineIdLeft.symbol < klineIdRight.symbol;
              });

    klinesIdList.erase(std::unique(klinesIdList.begin(), klinesIdList.end()), klinesIdList.end());

    for (const auto& klineIdInStock: klinesIdList)
    {
        symbolComboBox->addItem(klineIdInStock.baseName(), QVariant(klineIdInStock.symbol));
    }

    if (!klineId.isEmpty())
    {
        symbolComboBox->setCurrentText(klineId.baseName());
    }
    else
    {
        symbolComboBox->setCurrentText(*SYMBOL_NAME_ALL);
    }

    return symbolComboBox;
}

void MainWindow::makeFilterTab()
{
    //Filter
    {
        auto& filterTableWidget = ui->filterTableWidget;
        filterTableWidget->clear();
        filterTableWidget->setRowCount(0);

        filterTableWidget->setColumnCount(4);
        QStringList headersLabel;
        headersLabel << "  Stock exchange  " << "  Delta  " << "  Volume  " << "";
        filterTableWidget->setHorizontalHeaderLabels(headersLabel);
        filterTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        filterTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        filterTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        filterTableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

        for (const auto& filterData: _userConfig.filter().klineFilter())
        {
            const auto o_currentStockExcangeName = filterData.stockExchangeID();
            const auto currentStockExcangeName = o_currentStockExcangeName.has_value() && !o_currentStockExcangeName.value().isEmpty() ? o_currentStockExcangeName.value().name : *STOCKEXCHANGE_NAME_ALL;

            const auto o_currentDelta = filterData.delta();
            const auto o_currentVolume = filterData.volume();

            addFilterRow(currentStockExcangeName, o_currentDelta.value_or(KLineFilterData::MinDelta), o_currentVolume.value_or(KLineFilterData::MinVolume));
        }

        ui->addPushButton->setEnabled(true);
    }
}

void MainWindow::clearFilterTab()
{
    ui->filterTableWidget->clear();
    ui->filterTableWidget->setRowCount(0);

    ui->addPushButton->setEnabled(false);
    ui->removePushButton->setEnabled(false);
}

void MainWindow::makeBlackListTab()
{
    //black list
    {
        auto& blackListTableWidget = ui->blackListTableWidget;
        blackListTableWidget->clear();
        blackListTableWidget->setRowCount(0);

        blackListTableWidget->setColumnCount(3);
        QStringList headersLabel;
        headersLabel << "  Stock exchange  " << "  Symbol  " << "";
        blackListTableWidget->setHorizontalHeaderLabels(headersLabel);
        blackListTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        blackListTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        blackListTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

        for (const auto& blackListData: _userConfig.filter().blackList())
        {
            const auto o_currentStockExcangeName = blackListData.stockExchangeID();
            const auto currentStockExcangeName = o_currentStockExcangeName.has_value() && !o_currentStockExcangeName.value().isEmpty() ? o_currentStockExcangeName.value().name : *STOCKEXCHANGE_NAME_ALL;

            const auto o_currentKLineID = blackListData.klineID();
            const auto currentKLineID = o_currentKLineID.has_value() && !o_currentKLineID.value().isEmpty() ? o_currentKLineID.value() : KLineID();

            addBlackListRow(currentStockExcangeName, currentKLineID);
        }

        ui->addBlackListPushButton->setEnabled(true);
    }
}

void MainWindow::clearBlackListTab()
{
    ui->blackListTableWidget->clear();
    ui->blackListTableWidget->setRowCount(0);

    ui->addBlackListPushButton->setEnabled(false);
    ui->removeBlackListPushButton->setEnabled(false);
}

void MainWindow::addFilterRow(const QString &stockExchange, double delta, double volume)
{
    auto deltaSpinBox = new QDoubleSpinBox();
    deltaSpinBox->setMinimum(TradingCatCommon::KLineFilterData::MinDelta);
    deltaSpinBox->setMaximum(TradingCatCommon::KLineFilterData::MaxDelta);
    deltaSpinBox->setSingleStep(1.0);
    deltaSpinBox->setValue(delta);
    deltaSpinBox->setSpecialValueText("MINIMUM");

    auto volumeSpinBox = new QDoubleSpinBox();
    volumeSpinBox->setMinimum(TradingCatCommon::KLineFilterData::MinVolume);
    volumeSpinBox->setMaximum(TradingCatCommon::KLineFilterData::MaxVolume);
    volumeSpinBox->setSingleStep(100.0);
    volumeSpinBox->setValue(volume);
    volumeSpinBox->setSpecialValueText("MINIMUM");

    auto filterTableWidget = ui->filterTableWidget;

    auto removeButton = new QPushButton(QIcon(":/image/img/remove2.png"), "", filterTableWidget);
    removeButton->setMaximumSize(QSize(34, 34));

    connect(removeButton, SIGNAL(clicked()), SLOT(removePushButtonClicked()));

    filterTableWidget->insertRow(filterTableWidget->rowCount());
    const auto currentRow = filterTableWidget->rowCount() - 1;
    filterTableWidget->setCellWidget(currentRow, 0, makeStockExchangeComboBox(stockExchange));
    filterTableWidget->setCellWidget(currentRow, 1, deltaSpinBox);
    filterTableWidget->setCellWidget(currentRow, 2, volumeSpinBox);
    filterTableWidget->setCellWidget(currentRow, 3, removeButton);

    showRemovePushButton();
}

void MainWindow::showRemovePushButton()
{
    ui->removePushButton->setEnabled(ui->filterTableWidget->rowCount() != 0);
}

void MainWindow::addBlackListRow(const QString& stockExchange, const TradingCatCommon::KLineID &klineId)
{
    Q_ASSERT(ui->blackListTableWidget->columnCount() == 3);

    auto& blackListTableWidget = ui->blackListTableWidget;

    auto removeButton = new QPushButton(QIcon(":/image/img/remove2.png"), "", blackListTableWidget);
    removeButton->setMaximumSize(QSize(34, 34));

    connect(removeButton, SIGNAL(clicked()), SLOT(removeBlackListPushButtonClicked()));

    blackListTableWidget->insertRow(blackListTableWidget->rowCount());
    const auto currentRow = blackListTableWidget->rowCount() - 1;
    blackListTableWidget->setCellWidget(currentRow, 0, makeStockExchangeComboBox(stockExchange));
    blackListTableWidget->setCellWidget(currentRow, 1, makeSymbolComboBox(klineId));
    blackListTableWidget->setCellWidget(currentRow, 2, removeButton);

    showRemoveBlackListPushButton();
}

void MainWindow::showRemoveBlackListPushButton()
{
    ui->removeBlackListPushButton->setEnabled(ui->blackListTableWidget->rowCount() != 0);
}

QColor MainWindow::stockExchangeColor(const TradingCatCommon::StockExchangeID &stockExchangeId)
{
    const auto& stockExchangeName = stockExchangeId.name;

    //Spot
    if (stockExchangeName == "MEXC")
    {
        return Qt::yellow; //желтый
    }
    else if (stockExchangeName == "KUCOIN")
    {
        return QColor(246, 97, 81);  //красный
    }
    else if (stockExchangeName == "GATE")
    {
        return QColor(153, 193, 241);  //голубой
    }
    else if (stockExchangeName == "BYBIT")
    {
        return QColor(97, 53, 131); //фиолетовый
    }
    else if (stockExchangeName == "BINANCE")
    {
        return QColor(46, 194, 126); // светло зеленый
    }
    else if (stockExchangeName == "BITGET")
    {
        return QColor(200, 200, 200); //серый
    }
    else if (stockExchangeName == "BINGX")
    {
        return QColor(220, 138, 221); // розовый
    }
    else if (stockExchangeName == "OKX")
    {
        return QColor(255, 163, 72); // оранжевый
    }
    else if (stockExchangeName == "HTX")
    {
        return QColor(5, 55, 155); // синий
    }

    //Futures
    else if (stockExchangeName == "KUCOIN_FUTURES")
    {
        return QColor(246, 127, 110);  //красный
    }
    else if (stockExchangeName == "BITGET_FUTURES")
    {
        return QColor(200, 230, 230); //серый
    }
    else if (stockExchangeName == "GATE_FUTURES")
    {
        return QColor(153, 223, 255); //серый
    }
    else if (stockExchangeName == "BYBIT_FUTURES")
    {
        return QColor(97, 83, 161); //фиолетовый
    }
    if (stockExchangeName == "MEXC_FUTURES")
    {
        return Qt::darkYellow; //желтый
    }

    else
    {
        return Qt::gray;
    }
}

bool MainWindow::isLastDetected(const TradingCatCommon::StockExchangeID &stockExchangeId, const TradingCatCommon::KLineID &klineId, qint64 time)
{
    Q_UNUSED(stockExchangeId);

    const auto hash = std::hash<QString>()(klineId.baseName());  //+ 49831 * std::hash<TradingCatCommon::StockExchangeID>()(stockExchangeId);
    const auto it_lastDetected = _lastDetected.find(hash);
    if (it_lastDetected == _lastDetected.end())
    {
        _lastDetected.emplace(hash, time);

        return false;
    }

    auto& klineTime = it_lastDetected->second;
    const auto result = (time - klineTime) < LAST_DETECT_TIMEOUT;
    klineTime= time;

    return result;
}

