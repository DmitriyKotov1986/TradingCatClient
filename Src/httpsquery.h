#pragma once

#include <QObject>
#include <QUrl>
#include <QHash>
#include <QByteArray>
#include <QTimer>
#include <QNetworkAccessManager>

#include <emscripten/fetch.h>

class HTTPSQuery : public QObject
{
    Q_OBJECT

public:
    explicit HTTPSQuery(QObject *parent = nullptr);
    ~HTTPSQuery();

    quint64 send(const QUrl& url); //запускает отправку запроса

signals:
    void getAnswer(const QByteArray& answer, int id);
    void errorOccurred(quint32 code, const QString& msg, int id);

private:
    QNetworkAccessManager* _manager;

};

