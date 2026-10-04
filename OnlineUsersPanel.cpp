#include "OnlineUsersPanel.h"
#include "OnlineUsersManager.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QDateTime>

OnlineUsersPanel::OnlineUsersPanel(QWidget *parent)
    : QDockWidget("👥 Пользователи онлайн", parent)
    , m_manager(nullptr)
{
    setupUI();
    applyTheme();

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &OnlineUsersPanel::updateUsersList);
    m_updateTimer->start(5000); // Обновляем каждые 5 секунд
}

OnlineUsersPanel::~OnlineUsersPanel()
{
}

void OnlineUsersPanel::setupUI()
{
    QWidget* widget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Заголовок с количеством
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel("Активные пользователи:", widget);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    headerLayout->addWidget(titleLabel);

    m_countLabel = new QLabel("(0)", widget);
    m_countLabel->setStyleSheet("font-weight: bold; color: #0084ff; font-size: 12px;");
    headerLayout->addWidget(m_countLabel);
    headerLayout->addStretch();

    layout->addLayout(headerLayout);

    // Список пользователей
    m_usersList = new QListWidget(widget);
    m_usersList->setStyleSheet(
        "QListWidget {"
        "   border: 1px solid #ddd;"
        "   border-radius: 4px;"
        "   padding: 4px;"
        "}"
        "QListWidget::item {"
        "   padding: 6px;"
        "   border-radius: 3px;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: #f0f0f0;"
        "}"
    );
    layout->addWidget(m_usersList);

    setWidget(widget);
    setFloating(false);
}

void OnlineUsersPanel::applyTheme()
{
    ThemeManager& tm = ThemeManager::instance();
    bool isDark = (tm.currentTheme() == ThemeManager::Dark);

    QString bgColor = isDark ? "#1F2937" : "#F9FAFB";
    QString textColor = isDark ? "#F9FAFB" : "#111827";
    QString borderColor = isDark ? "#374151" : "#E5E7EB";

    QWidget* widget = this->widget();
    if (widget) {
        widget->setStyleSheet(QString(
            "QWidget {"
            "   background-color: %1;"
            "   color: %2;"
            "}"
        ).arg(bgColor, textColor));
    }

    m_usersList->setStyleSheet(QString(
        "QListWidget {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 4px;"
        "   padding: 4px;"
        "}"
        "QListWidget::item {"
        "   padding: 8px;"
        "   border-radius: 3px;"
        "   background-color: %4;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: %5;"
        "}"
    ).arg(bgColor, textColor, borderColor, bgColor, isDark ? "#374151" : "#f0f0f0"));
}

void OnlineUsersPanel::connectToManager(OnlineUsersManager& manager)
{
    m_manager = &manager;
    connect(&manager, &OnlineUsersManager::onlineUsersChanged,
            this, &OnlineUsersPanel::updateUsersList);
    connect(&manager, &OnlineUsersManager::userOnline,
            this, &OnlineUsersPanel::onUserOnline);
    connect(&manager, &OnlineUsersManager::userOffline,
            this, &OnlineUsersPanel::onUserOffline);

    updateUsersList();
}

void OnlineUsersPanel::updateUsersList()
{
    if (!m_manager) return;

    m_usersList->clear();

    QList<OnlineUser> users = m_manager->getOnlineUsers();
    for (const auto& user : users) {
        QListWidgetItem* item = new QListWidgetItem(m_usersList);
        QString timeText = user.lastSeen.toString("hh:mm");
        item->setText(QString("👤 %1\n💻 %2\n🕐 %3")
                         .arg(user.fio, user.computerName, timeText));
        item->setSizeHint(QSize(-1, 60));
        m_usersList->addItem(item);
    }

    // Обновляем счетчик
    int count = users.size();
    m_countLabel->setText(QString("(%1)").arg(count));

    // Если никого нет, показываем сообщение
    if (count == 0) {
        QListWidgetItem* emptyItem = new QListWidgetItem(m_usersList);
        emptyItem->setText("Нет активных пользователей");
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        m_usersList->addItem(emptyItem);
    }
}

void OnlineUsersPanel::onUserOnline(const QString& fio)
{
    // Добавляем уведомление при входе
}

void OnlineUsersPanel::onUserOffline(const QString& fio)
{
    // Добавляем уведомление при выходе
}
