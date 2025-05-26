#include "eventlistmenu.h"
#include "ui_eventlistmenu.h"

EventListMenu::EMenuItemType EventListMenu::intToEMenuItemType(quint8 type)
{
    switch (type)
    {
    case static_cast<quint8>(EMenuItemType::ADD_BLACK_LIST): return EMenuItemType::ADD_BLACK_LIST;
    case static_cast<quint8>(EMenuItemType::ADD_BLACK_LIST_ALL): return EMenuItemType::ADD_BLACK_LIST_ALL;
    default:
        return EMenuItemType::UNDEFINED;
    }

    return EMenuItemType::UNDEFINED;
}

EventListMenu::EventListMenu(QWidget *parent /* = nullptr */)
    : QWidget(parent, Qt::CustomizeWindowHint | Qt::FramelessWindowHint)
    , ui(new Ui::EventListMenu)
{
    qRegisterMetaType<EventListMenu::EMenuItemType>("EventListMenu::EMenuItemType");

    ui->setupUi(this);

    {
        auto addBlackListItem = new QListWidgetItem("Add to black list");
        addBlackListItem->setIcon(QIcon(":/icon/img/cage_cat_icon.ico"));
        addBlackListItem->setData(Qt::UserRole, static_cast<quint8>(EMenuItemType::ADD_BLACK_LIST));

        ui->menuListWidget->addItem(addBlackListItem);
    }

    {
        auto addBlackListAllItem = new QListWidgetItem("Add to black list for all");
        addBlackListAllItem->setIcon(QIcon(":/icon/img/cage_cat_icon.ico"));
        addBlackListAllItem->setData(Qt::UserRole, static_cast<quint8>(EMenuItemType::ADD_BLACK_LIST_ALL));

        ui->menuListWidget->addItem(addBlackListAllItem);
    }

    connect(ui->menuListWidget, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(itemClickedMenuList(QListWidgetItem*)));
}

EventListMenu::~EventListMenu()
{
    delete ui;
}

void EventListMenu::open(const QPoint &pos, quint64 index)
{
    _currentIndex = index;

    move(pos);
    show();
}

void EventListMenu::itemClickedMenuList(QListWidgetItem *item)
{
    close();

    emit clickedItem(intToEMenuItemType(item->data(Qt::UserRole).toUInt()), _currentIndex);
}


