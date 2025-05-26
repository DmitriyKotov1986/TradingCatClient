#pragma once

#include <QWidget>
#include <QListWidgetItem>

namespace Ui {
class EventListMenu;
}

class EventListMenu
    : public QWidget
{
    Q_OBJECT

public:
    enum class EMenuItemType: quint8
    {
        UNDEFINED = 0,
        ADD_BLACK_LIST =1,
        ADD_BLACK_LIST_ALL = 2
    };

    static EMenuItemType intToEMenuItemType(quint8 type);

public:
    explicit EventListMenu(QWidget *parent = nullptr);
    ~EventListMenu();

    void open(const QPoint& pos, quint64 index);

signals:
    void clickedItem(EMenuItemType type, quint64 index);

private slots:
    void itemClickedMenuList(QListWidgetItem *item);

private:
    Ui::EventListMenu *ui;

    quint64 _currentIndex = 0;

};

Q_DECLARE_METATYPE(EventListMenu::EMenuItemType);
