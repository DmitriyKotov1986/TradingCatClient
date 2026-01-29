#include <emscripten/html5.h>

#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>

QApplication *app = nullptr;
MainWindow *appWindow = nullptr;

static bool keyCallback(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData)
{
    Q_UNUSED(eventType);

    appWindow = (MainWindow*)userData;

    return appWindow->keyPress(keyEvent);
}

int main(int argc, char *argv[])
{
    app = new QApplication(argc, argv);

    QApplication::setApplicationName("TradingCatClient");
    QApplication::setOrganizationName("Cat software development");
    QApplication::setApplicationVersion(QString("Version:0.2 Build: %1 %2").arg(__DATE__).arg(__TIME__));

    qDebug() << "Start" << QApplication::applicationName() << QApplication::applicationVersion();

    // Загрузка файла шрифта
    const auto fontId = QFontDatabase::addApplicationFont(":/font/font/NotoSansCJK-Regular.ttc");
    if (fontId != -1)
    {
        QFont customFont("Noto Sans CJK SC");

        // Применение шрифта
        QApplication::setFont(customFont);
        qDebug() << "Use custom font:" << customFont.family();
    }
    else
    {
        qDebug() << "The font is not load. Use font for default system" << QApplication::font().family();
    }

    appWindow = new MainWindow();

    app->installEventFilter(appWindow);

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, appWindow, true, keyCallback);

    appWindow->show();

    return 0;
}
