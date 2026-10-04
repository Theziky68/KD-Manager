#include "NotificationPanel.h"
#include "NotificationManager.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QScreen>
#include <QGuiApplication>
#include <QDateTime>
#include <QListWidgetItem>
#include <QLabel>
#include <QMessageBox>

// ===== DIALOG ДЛЯ ПОДРОБНОЙ ИНФОРМАЦИИ =====
NotificationDetailsDialog::NotificationDetailsDialog(const QString& title, const QString& message,
                                                     const QString& type, const QDateTime& timestamp,
                                                     QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Детали уведомления");
    setFixedSize(500, 300);

    ThemeManager& tm = ThemeManager::instance();

    setStyleSheet(QString(
        "QDialog {"
        "   background-color: %1;"
        "   color: %2;"
        "}"
        "QLabel {"
        "   color: %2;"
        "}"
        "QPushButton {"
        "   background-color: %3;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 6px;"
        "   padding: 8px 16px;"
        "   font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "}"
    ).arg(tm.cardBackground(), tm.textColor(), tm.accentColor(), tm.hoverColor()));

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    // Заголовок
    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(QString("font-size: 16px; font-weight: 700; color: %1;").arg(tm.textColor()));
    layout->addWidget(titleLabel);

    // Время
    QLabel* timeLabel = new QLabel("Время: " + timestamp.toString("dd.MM.yyyy hh:mm:ss"));
    timeLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(tm.secondaryTextColor()));
    layout->addWidget(timeLabel);

    // Тип
    QLabel* typeLabel = new QLabel("Тип: " + type);
    typeLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(tm.secondaryTextColor()));
    layout->addWidget(typeLabel);

    // Сообщение
    QLabel* messageLabel = new QLabel(message);
    messageLabel->setWordWrap(true);
    messageLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    messageLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(tm.textColor()));
    layout->addWidget(messageLabel, 1);

    // Кнопка OK
    QPushButton* okBtn = new QPushButton("OK");
    okBtn->setFixedHeight(36);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(okBtn);
}

// ===== NOTIFICATIONPANEL =====
NotificationPanel::NotificationPanel(QWidget* parent)
    : QWidget(parent)
    , m_unreadCount(0)
    , m_isPanelVisible(false)
    , m_showInTrayOnly(false)
    , m_notificationManager(nullptr)
{
    setupUI();

    m_autoHideTimer = new QTimer(this);
    m_autoHideTimer->setSingleShot(true);
    connect(m_autoHideTimer, &QTimer::timeout, this, &NotificationPanel::hidePanel);
}

NotificationPanel::~NotificationPanel()
{
}

void NotificationPanel::setupUI()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setGeometry(0, 0, QGuiApplication::primaryScreen()->availableGeometry().width(),
                QGuiApplication::primaryScreen()->availableGeometry().height());

    // Кнопка уведомлений - справа снизу
    m_notificationButton = new QPushButton(this);
    m_notificationButton->setFixedSize(56, 56);
    m_notificationButton->setText("🔔");
    m_notificationButton->setStyleSheet(
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 #0084ff, stop:1 #0073e6);"
        "   color: white;"
        "   border: none;"
        "   border-radius: 28px;"
        "   font-size: 24px;"
        "   font-weight: bold;"
        "   padding: 0px;"
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 #0073e6, stop:1 #0063cc);"
        "}"
        "QPushButton:pressed {"
        "   background: #0063cc;"
        "}"
    );
    connect(m_notificationButton, &QPushButton::clicked, this, [this]() {
        if (m_isPanelVisible) hidePanel();
        else showPanel();
    });

    // Badge
    m_badgeLabel = new QLabel(this);
    m_badgeLabel->setAlignment(Qt::AlignCenter);
    m_badgeLabel->setStyleSheet(
        "QLabel {"
        "   background: #ff3b30;"
        "   color: white;"
        "   border-radius: 11px;"
        "   font-weight: bold;"
        "   font-size: 11px;"
        "   padding: 2px 6px;"
        "   min-width: 20px;"
        "}"
    );
    m_badgeLabel->setFixedHeight(22);
    m_badgeLabel->move(38, -5);
    m_badgeLabel->setVisible(false);

    // Панель уведомлений
    m_panel = new QWidget(this);
    m_panel->setFixedSize(380, 500);
    m_panel->hide();

    // Тень
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(m_panel);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(0, 0, 0, 80));
    shadow->setOffset(0, 8);
    m_panel->setGraphicsEffect(shadow);

    // Заголовок
    QWidget* headerWidget = new QWidget(m_panel);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(16, 14, 16, 14);
    headerLayout->setSpacing(12);

    QLabel* titleLabel = new QLabel("Уведомления", headerWidget);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 17px;"
        "   font-weight: 700;"
        "   font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;"
        "}"
    );
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_deleteAllButton = new QPushButton("🗑️ Удалить", headerWidget);
    m_deleteAllButton->setStyleSheet(
        "QPushButton {"
        "   background: transparent;"
        "   color: #ff3b30;"
        "   border: none;"
        "   font-size: 13px;"
        "   font-weight: 600;"
        "   padding: 4px 8px;"
        "   font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;"
        "}"
        "QPushButton:hover {"
        "   color: #ff5252;"
        "}"
    );
    connect(m_deleteAllButton, &QPushButton::clicked, this, &NotificationPanel::onDeleteAllClicked);
    headerLayout->addWidget(m_deleteAllButton);

    m_clearAllButton = new QPushButton("✕", headerWidget);
    m_clearAllButton->setStyleSheet(
        "QPushButton {"
        "   background: transparent;"
        "   color: #0084ff;"
        "   border: none;"
        "   font-size: 14px;"
        "   font-weight: 600;"
        "   padding: 4px 8px;"
        "   font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;"
        "}"
        "QPushButton:hover {"
        "   color: #0073e6;"
        "}"
    );
    connect(m_clearAllButton, &QPushButton::clicked, this, &NotificationPanel::hidePanel);
    headerLayout->addWidget(m_clearAllButton);

    // Список уведомлений
    m_notificationList = new QListWidget(m_panel);
    m_notificationList->setGeometry(0, 48, 380, 420);
    m_notificationList->setSpacing(2);
    connect(m_notificationList, &QListWidget::itemDoubleClicked,
            this, &NotificationPanel::onNotificationItemDoubleClicked);

    // Empty label
    m_emptyLabel = new QLabel("Нет уведомлений", m_panel);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setGeometry(0, 150, 380, 80);
    m_emptyLabel->setStyleSheet(
        "QLabel {"
        "   color: #999999;"
        "   font-size: 15px;"
        "   font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;"
        "}"
    );
    m_emptyLabel->setVisible(true);

    applyTheme();
}

void NotificationPanel::applyTheme()
{
    ThemeManager& tm = ThemeManager::instance();

    m_panel->setStyleSheet(QString(
        "QWidget {"
        "   background: %1;"
        "   border: none;"
        "   border-radius: 16px;"
        "}"
    ).arg(tm.cardBackground()));

    QWidget* headerWidget = m_panel->findChild<QWidget*>();
    if (headerWidget) {
        headerWidget->setStyleSheet(QString(
            "QWidget {"
            "   background: %1;"
            "   border-bottom: 1px solid %2;"
            "   border-top-left-radius: 16px;"
            "   border-top-right-radius: 16px;"
            "}"
        ).arg(tm.cardBackground(), tm.borderColor()));
    }

    m_notificationList->setStyleSheet(QString(
        "QListWidget {"
        "   background: %1;"
        "   border: none;"
        "   outline: none;"
        "   border-bottom-left-radius: 16px;"
        "   border-bottom-right-radius: 16px;"
        "   color: %2;"
        "}"
        "QListWidget::item {"
        "   padding: 0px;"
        "   margin: 0px;"
        "   border: none;"
        "}"
        "QListWidget::item:selected {"
        "   background: transparent;"
        "}"
        "QScrollBar:vertical {"
        "   background: transparent;"
        "   width: 8px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: %3;"
        "   border-radius: 4px;"
        "   min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background: %4;"
        "}"
    ).arg(tm.cardBackground(), tm.textColor(), tm.borderColor(), tm.hoverColor()));

    m_emptyLabel->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   font-size: 15px;"
        "   background: transparent;"
        "   font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;"
        "}"
    ).arg(tm.secondaryTextColor()));
}

void NotificationPanel::updateTheme()
{
    applyTheme();
}

void NotificationPanel::addNotification(const QString& title, const QString& message, const QString& type)
{
    createNotificationItem(title, message, type, QDateTime::currentDateTime(), "");
    m_notificationsData.append({title, {message, QDateTime::currentDateTime()}});
    m_unreadCount++;
    updateBadge();

    // Если галочка активна - показываем окно автоматически
    if (m_showInTrayOnly) {
        if (!m_isPanelVisible) {
            showPanel();
            m_autoHideTimer->start(5000);
        }
    }
}

void NotificationPanel::addNotification(const QString& title, const QString& message, const QString& type, const QString& sourceUser)
{
    createNotificationItem(title, message, type, QDateTime::currentDateTime(), sourceUser);
    m_notificationsData.append({title, {message, QDateTime::currentDateTime()}});
    m_unreadCount++;
    updateBadge();

    // Если галочка активна - показываем окно автоматически
    if (m_showInTrayOnly) {
        if (!m_isPanelVisible) {
            showPanel();
            m_autoHideTimer->start(5000);
        }
    }
}

void NotificationPanel::connectToManager(NotificationManager& manager)
{
    m_notificationManager = &manager;
    connect(&manager, static_cast<void(NotificationManager::*)(const QString&, const NotificationManager::Notification&)>(&NotificationManager::newNotification),
            this, &NotificationPanel::onManagerNotificationReceived);
}

void NotificationPanel::onManagerNotificationReceived(const QString& user)
{
    if (!m_notificationManager) return;

    QList<NotificationManager::Notification> notifs = m_notificationManager->getNotifications(user);
    if (notifs.isEmpty()) return;

    const NotificationManager::Notification& lastNotif = notifs.last();
    QString typeStr = notificationTypeToString(lastNotif.type);
    addNotification(lastNotif.title, lastNotif.message, typeStr, lastNotif.sourceUser);
}

QString NotificationPanel::notificationTypeToString(int typeEnum)
{
    switch(typeEnum) {
    case NotificationManager::TYPE_SUCCESS:
        return "success";
    case NotificationManager::TYPE_WARNING:
        return "warning";
    case NotificationManager::TYPE_IMPORTANT:
        return "error";
    case NotificationManager::TYPE_CHANGE:
        return "warning";
    default:
        return "info";
    }
}

void NotificationPanel::createNotificationItem(const QString& title, const QString& message,
                                               const QString& type, const QDateTime& timestamp,
                                               const QString& sourceUser)
{
    if (m_notificationList->count() == 0) {
        m_emptyLabel->setVisible(false);
    }

    ThemeManager& tm = ThemeManager::instance();
    bool isDark = (tm.currentTheme() == ThemeManager::Dark);

    // Используем цвета из ThemeManager для консистентности
    QString bgColor = tm.cardBackground();
    QString textColor = tm.textColor();
    QString timeColor = tm.secondaryTextColor();
    QString borderColor = tm.borderColor();

    QWidget* itemWidget = new QWidget();
    itemWidget->setStyleSheet(QString(
        "QWidget {"
        "   background: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 8px;"
        "   padding: 12px 14px;"
        "}"
    ).arg(bgColor, borderColor));

    QVBoxLayout* layout = new QVBoxLayout(itemWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // Заголовок + время + ФИО в один ряд
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    QLabel* titleLabel = new QLabel(title, itemWidget);
    titleLabel->setStyleSheet(QString(
        "QLabel {"
        "   font-size: 13px;"
        "   font-weight: 600;"
        "   color: %1;"
        "   background: transparent;"
        "   border: none;"
        "}"
    ).arg(textColor));
    titleLabel->setWordWrap(false);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    // ФИО если есть
    if (!sourceUser.isEmpty()) {
        QLabel* userLabel = new QLabel(QString("От: %1").arg(sourceUser), itemWidget);
        userLabel->setStyleSheet(QString(
            "QLabel {"
            "   font-size: 10px;"
            "   color: %1;"
            "   background: transparent;"
            "   border: none;"
            "   font-style: italic;"
            "}"
        ).arg(timeColor));
        headerLayout->addWidget(userLabel);
    }

    QLabel* timeLabel = new QLabel(timestamp.toString("hh:mm"), itemWidget);
    timeLabel->setStyleSheet(QString(
        "QLabel {"
        "   font-size: 11px;"
        "   color: %1;"
        "   background: transparent;"
        "   border: none;"
        "}"
    ).arg(timeColor));
    headerLayout->addWidget(timeLabel);

    layout->addLayout(headerLayout);

    // Сообщение
    QLabel* messageLabel = new QLabel(message, itemWidget);
    messageLabel->setStyleSheet(QString(
        "QLabel {"
        "   font-size: 12px;"
        "   color: %1;"
        "   background: transparent;"
        "   border: none;"
        "}"
    ).arg(textColor));
    messageLabel->setWordWrap(true);
    messageLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(messageLabel);

    // Не устанавливаем фиксированный размер - карточка подстраивается под содержимое
    QListWidgetItem* item = new QListWidgetItem(m_notificationList);

    // Вычисляем размер содержимого
    QSize contentSize = itemWidget->sizeHint();
    // Минимальная высота 70px, но может быть больше в зависимости от текста
    int height = qMax(70, contentSize.height() + 12);

    item->setSizeHint(QSize(360, height));
    m_notificationList->addItem(item);
    m_notificationList->setItemWidget(item, itemWidget);
}

void NotificationPanel::onNotificationItemDoubleClicked(QListWidgetItem* item)
{
    int index = m_notificationList->row(item);
    if (index >= 0 && index < m_notificationsData.size()) {
        const auto& data = m_notificationsData[index];
        NotificationDetailsDialog dialog(data.first, data.second.first, "info", data.second.second, this);
        dialog.exec();
    }
}

void NotificationPanel::showPanel()
{
    if (m_isPanelVisible) return;

    m_isPanelVisible = true;
    show();
    m_panel->show();
    m_notificationButton->hide();  // Скрываем кнопку когда окно открыто

    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();

    int x = screenGeometry.right() - m_panel->width() - 16;
    int y = screenGeometry.top() + 100;  // Поднимаем окно выше

    m_panel->move(x + 380, y - 50);

    m_slideAnimation = new QPropertyAnimation(m_panel, "geometry");
    m_slideAnimation->setDuration(300);
    m_slideAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_slideAnimation->setStartValue(QRect(x + 380, y - 50, 380, 500));
    m_slideAnimation->setEndValue(QRect(x, y, 380, 500));
    m_slideAnimation->start();

    m_unreadCount = 0;
    updateBadge();
}

void NotificationPanel::hidePanel()
{
    if (!m_isPanelVisible) return;

    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int x = screenGeometry.right() - m_panel->width() - 16;
    int y = screenGeometry.top() + 100;

    m_slideAnimation = new QPropertyAnimation(m_panel, "geometry");
    m_slideAnimation->setDuration(300);
    m_slideAnimation->setEasingCurve(QEasingCurve::InCubic);
    m_slideAnimation->setStartValue(m_panel->geometry());
    m_slideAnimation->setEndValue(QRect(x + 380, y - 50, 380, 500));

    connect(m_slideAnimation, &QPropertyAnimation::finished, [this]() {
        m_panel->hide();
        hide();
        m_isPanelVisible = false;
        m_notificationButton->show();  // Показываем кнопку когда окно закрыто
    });

    m_slideAnimation->start();
}

void NotificationPanel::onClearAllClicked()
{
    m_notificationList->clear();
    m_notificationsData.clear();
    m_emptyLabel->setVisible(true);
    m_unreadCount = 0;
    updateBadge();
}

void NotificationPanel::onDeleteAllClicked()
{
    if (m_notificationList->count() == 0) {
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Удалить все уведомления?",
        "Вы уверены, что хотите удалить все уведомления? Это действие нельзя отменить.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        onClearAllClicked();
        if (m_notificationManager) {
            m_notificationManager->clearAllNotifications();
        }
    }
}

void NotificationPanel::updateBadge()
{
    if (m_unreadCount > 0) {
        m_badgeLabel->setText(QString::number(m_unreadCount));
        m_badgeLabel->setVisible(true);
    } else {
        m_badgeLabel->setVisible(false);
    }
}

QString NotificationPanel::getTypeColor(const QString& type)
{
    if (type == "success") return "#4caf50";
    if (type == "error") return "#f44336";
    if (type == "warning") return "#ff9800";
    return "#2196f3";
}

QString NotificationPanel::getTypeIcon(const QString& type)
{
    if (type == "success") return "✅";
    if (type == "error") return "❌";
    if (type == "warning") return "⚠️";
    return "ℹ️";
}

void NotificationPanel::clearAll()
{
    onClearAllClicked();
}

void NotificationPanel::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::transparent);
    QWidget::paintEvent(event);
}

void NotificationPanel::onNotificationItemClicked(QListWidgetItem* item)
{
}

void NotificationPanel::autoHideNotification()
{
    if (m_isPanelVisible && m_unreadCount == 0) {
        hidePanel();
    }
}
