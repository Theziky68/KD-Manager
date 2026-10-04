#include "ChangesPanel.h"
#include "DatabaseManager.h"
#include "ThemeManager.h"
#include <QDateTime>
#include <QSettings>
#include <QApplication>
#include <QStyle>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QTextEdit>
#include <QScrollBar>
#include <QToolTip>
#include <QFontDatabase>
#include <QGridLayout>

ChangesPanel* ChangesPanel::s_instance = nullptr;
bool ChangesPanel::s_initialized = false;
QList<ChangesPanel::ChangeItem> ChangesPanel::s_globalNotifications;

ChangesPanel& ChangesPanel::instance()
{
    if (!s_instance) {
        s_instance = new ChangesPanel(nullptr);
        s_instance->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    return *s_instance;
}

void ChangesPanel::initialize(DatabaseManager* db, QWidget* parent)
{
    qDebug() << "🔍 ChangesPanel::initialize - s_instance check";

    if (!s_instance) {
        qDebug() << "🔍 ChangesPanel::initialize - creating new instance";
        qDebug() << "   parent:" << parent;
        qDebug() << "   parent type:" << (parent ? parent->metaObject()->className() : "nullptr");

        s_instance = new ChangesPanel(parent);
        qDebug() << "✅ ChangesPanel instance created";

        s_instance->setAttribute(Qt::WA_DeleteOnClose, false);
        qDebug() << "✅ setAttribute OK";
    } else {
        qDebug() << "⏭️ s_instance already exists, skipping creation";
    }

    if (db) {
        qDebug() << "🔍 Setting database manager...";
        s_instance->setDatabaseManager(db);
        qDebug() << "✅ Database manager set";

        // Загружаем уведомления позже, в безопасный момент
        qDebug() << "⏭️ Skipping loadNotificationsFromDatabase";
    } else {
        qDebug() << "❌ db is nullptr!";
    }

    s_initialized = true;
    qDebug() << "✅ ChangesPanel::initialize - completed";
}

void ChangesPanel::cleanup()
{
    if (s_instance) {
        s_instance->saveNotificationsToDatabase();
        delete s_instance;
        s_instance = nullptr;
        s_initialized = false;
    }
}

ChangesPanel::ChangesPanel(QWidget* parent)
    : QWidget(parent)
    , m_db(nullptr)
    , m_unreadCount(0)
    , m_isUpdating(false)
{
    qDebug() << "🔍 ChangesPanel::ChangesPanel - constructor start, parent:" << parent;

    setWindowTitle("📋 Уведомления");
    qDebug() << "✅ setWindowTitle OK";

    setMinimumWidth(280);
    setMaximumWidth(420);
    qDebug() << "✅ setMinimumWidth/setMaximumWidth OK";

    setupUI();
    qDebug() << "✅ setupUI OK";

    connect(&NotificationManager::instance(), &NotificationManager::newNotification,
            this, &ChangesPanel::onNewNotification);
    qDebug() << "✅ connect to NotificationManager OK";

    QSettings settings("KDManager", "ChangesPanel");
    restoreGeometry(settings.value("geometry").toByteArray());
    qDebug() << "✅ restoreGeometry OK";

    qDebug() << "✅ ChangesPanel::ChangesPanel - constructor complete";
}

ChangesPanel::~ChangesPanel()
{
    QSettings settings("KDManager", "ChangesPanel");
    settings.setValue("geometry", saveGeometry());
    saveNotificationsToDatabase();
}

void ChangesPanel::setupUI()
{
    qDebug() << "🔍 setupUI - начало";

    ThemeManager& tm = ThemeManager::instance();
    qDebug() << "✅ ThemeManager получен";

    // Стиль для самого ChangesPanel (вместо QDockWidget)
    setStyleSheet(QString(
        "QWidget {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "}"
    ).arg(tm.cardBackground(), tm.borderColor()));
    qDebug() << "✅ setStyleSheet OK";

    // Главный layout для ChangesPanel
    QVBoxLayout* panelLayout = new QVBoxLayout(this);
    panelLayout->setSpacing(0);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    qDebug() << "✅ panelLayout создан";

    m_changesList = new QListWidget(this);
    qDebug() << "✅ m_changesList создан";

    m_changesList->setStyleSheet(QString(
        "QListWidget {"
        "   background-color: %1;"
        "   border: none;"
        "   padding: 8px;"
        "}"
        "QScrollBar:vertical {"
        "   background: %2;"
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
    ).arg(tm.cardBackground(),
          tm.currentTheme() == ThemeManager::Dark ? "#1F2937" : "#f5f5f5",
          tm.currentTheme() == ThemeManager::Dark ? "#4B5563" : "#d0d0d0",
          tm.currentTheme() == ThemeManager::Dark ? "#6B7280" : "#b0b0b0"));
    qDebug() << "✅ m_changesList setStyleSheet OK";

    connect(m_changesList, &QListWidget::itemClicked, this, &ChangesPanel::onItemClicked);
    connect(m_changesList, &QListWidget::itemDoubleClicked, this, &ChangesPanel::onItemDoubleClicked);
    qDebug() << "✅ m_changesList connect OK";

    panelLayout->addWidget(m_changesList, 1);
    qDebug() << "✅ m_changesList added to layout";

    QHBoxLayout* footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(8, 8, 8, 8);
    footerLayout->setSpacing(6);
    qDebug() << "✅ footerLayout создан";

    m_markReadButton = new QPushButton("✓ Прочитано", this);
    m_markReadButton->setFixedHeight(32);
    m_markReadButton->setStyleSheet(QString(
        "QPushButton {"
        "   background-color: %1;"
        "   color: #FFFFFF;"
        "   border: none;"
        "   border-radius: 6px;"
        "   font-weight: 500;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QPushButton:hover {"
        "   background-color: %2;"
        "}"
    ).arg(tm.successColor(),
          tm.currentTheme() == ThemeManager::Dark ? "#388E3C" : "#45a049"));
    qDebug() << "✅ m_markReadButton создан";

    connect(m_markReadButton, &QPushButton::clicked, this, &ChangesPanel::markAllAsRead);
    footerLayout->addWidget(m_markReadButton);
    qDebug() << "✅ m_markReadButton добавлен";

    m_clearButton = new QPushButton("✕ Очистить", this);
    m_clearButton->setFixedHeight(32);
    m_clearButton->setStyleSheet(QString(
        "QPushButton {"
        "   background-color: %1;"
        "   color: #FFFFFF;"
        "   border: none;"
        "   border-radius: 6px;"
        "   font-weight: 500;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QPushButton:hover {"
        "   background-color: %2;"
        "}"
    ).arg(tm.errorColor(),
          tm.currentTheme() == ThemeManager::Dark ? "#C62828" : "#da190b"));
    qDebug() << "✅ m_clearButton создан";

    connect(m_clearButton, &QPushButton::clicked, this, &ChangesPanel::clearAll);
    footerLayout->addWidget(m_clearButton);
    qDebug() << "✅ m_clearButton добавлен";

    panelLayout->addLayout(footerLayout);
    qDebug() << "✅ setupUI - конец";
}

void ChangesPanel::setDatabaseManager(DatabaseManager* db)
{
    m_db = db;
}

void ChangesPanel::loadChanges(const QString& folderPath)
{
    m_currentFolder = folderPath;
}

void ChangesPanel::addNotificationItem(const NotificationItem& item)
{
    ChangeItem changeItem;
    changeItem.icon = item.icon;
    changeItem.title = item.title;
    changeItem.text = item.text;
    changeItem.filePath = item.filePath;
    changeItem.folderPath = item.folderPath;
    changeItem.time = item.time;
    changeItem.isRead = item.isRead;
    changeItem.type = item.type;
    changeItem.sourceUser = item.sourceUser;

    s_globalNotifications.append(changeItem);
    m_items.append(changeItem);

    if (m_changesList) {
        refreshList();
    }
}

void ChangesPanel::clearAllNotifications()
{
    s_globalNotifications.clear();
    m_items.clear();
    if (m_changesList) {
        m_changesList->clear();
    }
    updateNotificationCount();
}

void ChangesPanel::refreshPanel()
{
    refreshList();
}

// ============================================================
// СОЗДАНИЕ ВИДЖЕТА УВЕДОМЛЕНИЯ
// ============================================================

void ChangesPanel::createNotificationWidget(QListWidgetItem* listItem, const ChangeItem& item)
{
    QWidget* widget = new QWidget(m_changesList);

    // Определяем акцентный цвет в зависимости от типа
    QString accentColor;
    QString iconText;

    switch(item.type) {
    case NotificationManager::TYPE_SUCCESS:
        accentColor = "#10B981"; // зелёный
        iconText = "✓";
        break;
    case NotificationManager::TYPE_WARNING:
        accentColor = "#F59E0B"; // оранжевый
        iconText = "⚠";
        break;
    case NotificationManager::TYPE_IMPORTANT:
        accentColor = "#EF4444"; // красный
        iconText = "!";
        break;
    case NotificationManager::TYPE_CHANGE:
        accentColor = "#6366F1"; // индиго
        iconText = "◆";
        break;
    default:
        accentColor = "#6B7280"; // серый
        iconText = "i";
    }

    // Основной стиль карточки
    widget->setStyleSheet(
        QString("QWidget {"
                "   background-color: %1;"
                "   border-radius: 8px;"
                "   padding: 0px;"
                "}")
            .arg(item.isRead ? "#FAFAFA" : "#FFFFFF")
        );

    QHBoxLayout* mainLayout = new QHBoxLayout(widget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== ЛЕВАЯ ПОЛОСА (АКЦЕНТ) =====
    QFrame* accentBar = new QFrame(widget);
    accentBar->setFixedWidth(4);
    accentBar->setStyleSheet(
        QString("QFrame {"
                "   background-color: %1;"
                "   border-radius: 0px;"
                "   border-top-left-radius: 8px;"
                "   border-bottom-left-radius: 8px;"
                "}")
            .arg(accentColor)
        );
    mainLayout->addWidget(accentBar);

    // ===== КОНТЕНТ =====
    QVBoxLayout* contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(12, 10, 12, 10);
    contentLayout->setSpacing(6);

    // Верхняя строка: иконка + заголовок + время
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    // Иконка типа
    QLabel* iconLabel = new QLabel(iconText, widget);
    iconLabel->setFixedSize(20, 20);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet(
        QString("QLabel {"
                "   background-color: %1;"
                "   color: white;"
                "   border-radius: 10px;"
                "   font-weight: bold;"
                "   font-size: 11px;"
                "   font-family: 'Segoe UI', 'Arial', sans-serif;"
                "}")
            .arg(accentColor)
        );
    headerLayout->addWidget(iconLabel);

    // Заголовок
    QLabel* titleLabel = new QLabel(item.title, widget);
    titleLabel->setStyleSheet(
        QString("QLabel {"
                "   font-weight: 600;"
                "   font-size: 13px;"
                "   color: %1;"
                "   background: transparent;"
                "   font-family: 'Segoe UI', 'Arial', sans-serif;"
                "}")
            .arg(item.isRead ? "#6B7280" : "#111827")
        );
    titleLabel->setWordWrap(false);
    headerLayout->addWidget(titleLabel, 1);

    // Индикатор непрочитанного
    if (!item.isRead) {
        QLabel* unreadDot = new QLabel("●", widget);
        unreadDot->setFixedSize(8, 8);
        unreadDot->setStyleSheet(
            QString("QLabel {"
                    "   color: %1;"
                    "   font-size: 8px;"
                    "   background: transparent;"
                    "}")
                .arg(accentColor)
            );
        headerLayout->addWidget(unreadDot);
    }

    contentLayout->addLayout(headerLayout);

    // Текст сообщения
    if (!item.text.isEmpty()) {
        QLabel* textLabel = new QLabel(item.text, widget);
        textLabel->setStyleSheet(
            QString("QLabel {"
                    "   font-size: 12px;"
                    "   color: %1;"
                    "   background: transparent;"
                    "   font-family: 'Segoe UI', 'Arial', sans-serif;"
                    "   line-height: 1.4;"
                    "}")
                .arg(item.isRead ? "#9CA3AF" : "#4B5563")
            );
        textLabel->setWordWrap(true);
        textLabel->setMaximumHeight(40);
        textLabel->setTextFormat(Qt::PlainText);
        contentLayout->addWidget(textLabel);
    }

    // Нижняя строка: пользователь + время
    QHBoxLayout* footerLayout = new QHBoxLayout();
    footerLayout->setSpacing(12);

    if (!item.sourceUser.isEmpty()) {
        QLabel* userLabel = new QLabel(item.sourceUser, widget);
        userLabel->setStyleSheet(
            "QLabel {"
            "   color: #9CA3AF;"
            "   font-size: 10px;"
            "   background: transparent;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "}"
            );
        footerLayout->addWidget(userLabel);
    }

    footerLayout->addStretch();

    // Время
    QString timeText;
    QDateTime now = QDateTime::currentDateTime();
    qint64 secsAgo = item.time.secsTo(now);

    if (secsAgo < 60) {
        timeText = "только что";
    } else if (secsAgo < 3600) {
        timeText = QString("%1 мин назад").arg(secsAgo / 60);
    } else if (secsAgo < 86400) {
        timeText = QString("%1 ч назад").arg(secsAgo / 3600);
    } else if (item.time.date() == now.date().addDays(-1)) {
        timeText = "вчера";
    } else {
        timeText = item.time.toString("dd.MM.yy");
    }

    QLabel* timeLabel = new QLabel(timeText, widget);
    timeLabel->setStyleSheet(
        "QLabel {"
        "   color: #9CA3AF;"
        "   font-size: 10px;"
        "   background: transparent;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        );
    footerLayout->addWidget(timeLabel);

    contentLayout->addLayout(footerLayout);

    mainLayout->addLayout(contentLayout);

    // Устанавливаем размер элемента
    widget->setMinimumHeight(70);
    listItem->setSizeHint(widget->sizeHint());
    m_changesList->setItemWidget(listItem, widget);
}

// ============================================================
// ОБНОВЛЕНИЕ СЧЕТЧИКА
// ============================================================

void ChangesPanel::updateNotificationCount()
{
    int unread = 0;
    for (const ChangeItem& item : m_items) {
        if (!item.isRead) {
            unread++;
        }
    }
    m_unreadCount = unread;

    if (m_notificationCountLabel) {
        m_notificationCountLabel->setText(QString::number(unread));
    }

    if (unread > 0) {
        setWindowTitle("📋 Уведомления (" + QString::number(unread) + ")");
    } else {
        setWindowTitle("📋 Уведомления");
    }
}

// ============================================================
// ОБРАБОТЧИКИ СОБЫТИЙ
// ============================================================

void ChangesPanel::onItemClicked(QListWidgetItem* item)
{
    if (!item) return;

    int index = m_changesList->row(item);
    if (index >= 0 && index < m_items.size()) {
        ChangeItem& changeItem = m_items[index];

        if (!changeItem.isRead) {
            changeItem.isRead = true;
            if (index < s_globalNotifications.size()) {
                s_globalNotifications[index].isRead = true;
            }
            createNotificationWidget(item, changeItem);
            updateNotificationCount();
            saveNotificationsToDatabase();
        }

        QString filePath = changeItem.filePath;
        if (!filePath.isEmpty()) {
            emit fileSelected(filePath);
        }
    }
}

void ChangesPanel::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;

    int index = m_changesList->row(item);
    if (index >= 0 && index < m_items.size()) {
        const ChangeItem& changeItem = m_items[index];
        showNotificationDetails(changeItem);
    }
}

// ============================================================
// ДЕТАЛИ УВЕДОМЛЕНИЯ
// ============================================================

void ChangesPanel::showNotificationDetails(const ChangeItem& item)
{
    QDialog dialog(this);
    dialog.setWindowTitle("📋 Детали уведомления");
    dialog.setMinimumSize(500, 350);
    dialog.setStyleSheet(
        "QDialog {"
        "   background-color: #f5f0e8;"
        "}"
        "QLabel {"
        "   color: #3a2a1a;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        );

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel* titleLabel = new QLabel(item.icon + " " + item.title, &dialog);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: 600; color: #3a2a1a;");
    mainLayout->addWidget(titleLabel);

    QFrame* line = new QFrame(&dialog);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("background-color: #ddd4c8; max-height: 1px;");
    mainLayout->addWidget(line);

    QGridLayout* infoLayout = new QGridLayout();
    infoLayout->setSpacing(8);

    int row = 0;

    QString typeText;
    switch(item.type) {
    case NotificationManager::TYPE_CHANGE: typeText = "📝 Изменение"; break;
    case NotificationManager::TYPE_IMPORTANT: typeText = "⭐ Важное"; break;
    case NotificationManager::TYPE_WARNING: typeText = "⚠️ Предупреждение"; break;
    case NotificationManager::TYPE_SUCCESS: typeText = "✅ Успешно"; break;
    default: typeText = "ℹ️ Инфо"; break;
    }
    infoLayout->addWidget(new QLabel("📌 Тип:", &dialog), row, 0);
    infoLayout->addWidget(new QLabel(typeText, &dialog), row, 1);
    row++;

    infoLayout->addWidget(new QLabel("🕐 Время:", &dialog), row, 0);
    infoLayout->addWidget(new QLabel(item.time.toString("dd.MM.yyyy hh:mm:ss"), &dialog), row, 1);
    row++;

    if (!item.sourceUser.isEmpty()) {
        infoLayout->addWidget(new QLabel("👤 Пользователь:", &dialog), row, 0);
        infoLayout->addWidget(new QLabel(item.sourceUser, &dialog), row, 1);
        row++;
    }

    if (!item.filePath.isEmpty()) {
        infoLayout->addWidget(new QLabel("📄 Файл:", &dialog), row, 0);
        QLabel* fileLabel = new QLabel(QFileInfo(item.filePath).fileName(), &dialog);
        fileLabel->setStyleSheet("color: #5a4a3a;");
        infoLayout->addWidget(fileLabel, row, 1);
        row++;
    }

    if (!item.folderPath.isEmpty()) {
        infoLayout->addWidget(new QLabel("📁 Папка:", &dialog), row, 0);
        QLabel* folderLabel = new QLabel(item.folderPath, &dialog);
        folderLabel->setWordWrap(true);
        infoLayout->addWidget(folderLabel, row, 1);
        row++;
    }

    DocumentState state;
    state.status = item.status;
    infoLayout->addWidget(new QLabel("📊 Статус:", &dialog), row, 0);
    QLabel* statusLabel = new QLabel(state.statusToString(), &dialog);
    statusLabel->setStyleSheet(QString("color: %1; font-weight: 500;").arg(state.statusToColor()));
    infoLayout->addWidget(statusLabel, row, 1);
    row++;

    mainLayout->addLayout(infoLayout);

    if (!item.text.isEmpty()) {
        QLabel* msgLabel = new QLabel("📝 Сообщение:", &dialog);
        msgLabel->setStyleSheet("font-weight: 500; margin-top: 8px; color: #3a2a1a;");
        mainLayout->addWidget(msgLabel);

        QTextEdit* textEdit = new QTextEdit(&dialog);
        textEdit->setPlainText(item.text);
        textEdit->setReadOnly(true);
        textEdit->setMaximumHeight(100);
        textEdit->setStyleSheet(
            "QTextEdit {"
            "   background-color: #faf5ee;"
            "   border: 1px solid #ddd4c8;"
            "   border-radius: 8px;"
            "   padding: 10px;"
            "   font-size: 12px;"
            "   color: #3a2a1a;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "}"
            );
        mainLayout->addWidget(textEdit);
    }

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton* closeBtn = new QPushButton("Закрыть", &dialog);
    closeBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: #b8a898;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 6px;"
        "   padding: 8px 24px;"
        "   font-weight: 500;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QPushButton:hover {"
        "   background-color: #a09080;"
        "}"
        );
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addWidget(closeBtn);

    mainLayout->addLayout(buttonLayout);

    dialog.exec();
}

void ChangesPanel::showNotificationDetailsDialog()
{
    int currentRow = m_changesList->currentRow();
    if (currentRow < 0 || currentRow >= m_items.size()) return;
    showNotificationDetails(m_items[currentRow]);
}

// ============================================================
// ДЕЙСТВИЯ С УВЕДОМЛЕНИЯМИ
// ============================================================

void ChangesPanel::markAllAsRead()
{
    for (ChangeItem& item : m_items) {
        item.isRead = true;
    }
    for (int i = 0; i < s_globalNotifications.size() && i < m_items.size(); ++i) {
        s_globalNotifications[i].isRead = m_items[i].isRead;
    }

    for (int i = 0; i < m_changesList->count(); ++i) {
        QListWidgetItem* item = m_changesList->item(i);
        if (i < m_items.size()) {
            createNotificationWidget(item, m_items[i]);
        }
    }
    updateNotificationCount();
    saveNotificationsToDatabase();
    QMessageBox::information(this, "Готово", "Все уведомления отмечены как прочитанные");
}

void ChangesPanel::clearAll()
{
    if (QMessageBox::question(this, "Подтверждение",
                              "Очистить все уведомления?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        clearAllNotifications();
    }
}

void ChangesPanel::refreshList()
{
    if (!m_changesList) return;

    m_changesList->clear();

    if (m_items.isEmpty()) {
        QListWidgetItem* emptyItem = new QListWidgetItem(m_changesList);
        emptyItem->setText("📭 Нет уведомлений");
        m_changesList->addItem(emptyItem);
        return;
    }

    for (const ChangeItem& item : m_items) {
        QListWidgetItem* listItem = new QListWidgetItem(m_changesList);
        createNotificationWidget(listItem, item);
        m_changesList->addItem(listItem);
    }

    updateNotificationCount();
}

// ============================================================
// ФИЛЬТРАЦИЯ
// ============================================================

void ChangesPanel::filterChanged()
{
    if (!m_changesList) return;

    m_changesList->clear();
    m_itemWidgets.clear();

    if (m_items.isEmpty()) {
        QListWidgetItem* emptyItem = new QListWidgetItem(m_changesList);
        QWidget* emptyWidget = new QWidget(m_changesList);
        emptyWidget->setStyleSheet(
            "QWidget {"
            "   background-color: transparent;"
            "}"
            );
        QVBoxLayout* layout = new QVBoxLayout(emptyWidget);
        layout->setAlignment(Qt::AlignCenter);
        QLabel* emptyLabel = new QLabel("📭 Нет уведомлений", emptyWidget);
        emptyLabel->setStyleSheet(
            "QLabel {"
            "   color: #9CA3AF;"
            "   font-size: 12px;"
            "   background: transparent;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "}"
            );
        emptyLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(emptyLabel);
        emptyWidget->setLayout(layout);
        emptyItem->setSizeHint(QSize(280, 60));
        m_changesList->setItemWidget(emptyItem, emptyWidget);
        m_changesList->addItem(emptyItem);
        return;
    }

    for (const ChangeItem& item : m_items) {
        QListWidgetItem* listItem = new QListWidgetItem(m_changesList);
        createNotificationWidget(listItem, item);
        m_changesList->addItem(listItem);
    }

    updateNotificationCount();
}

// ============================================================
// НОВЫЕ УВЕДОМЛЕНИЯ
// ============================================================

void ChangesPanel::onNewNotification(const QString& user, const NotificationManager::Notification& notif)
{
    QString currentUser = qgetenv("USERNAME");

    // Если user указан и это не текущий пользователь - пропускаем
    if (!user.isEmpty() && user != currentUser) {
        return;
    }

    // Проверяем дублирование
    for (const ChangeItem& existing : s_globalNotifications) {
        if (existing.filePath == notif.filePath &&
            existing.title == notif.title &&
            existing.time == notif.timestamp) {
            return;
        }
    }

    ChangeItem item;
    item.filePath = notif.filePath;
    item.folderPath = notif.folderPath;
    item.time = notif.timestamp;
    item.isRead = false;
    item.type = notif.type;
    item.sourceUser = notif.sourceUser;

    // Определяем заголовок в зависимости от типа
    switch(notif.type) {
    case NotificationManager::TYPE_CHANGE:
        item.icon = "📝";
        item.title = "📝 Изменение";
        break;
    case NotificationManager::TYPE_IMPORTANT:
        item.icon = "⭐";
        item.title = "⭐ Важное";
        break;
    case NotificationManager::TYPE_WARNING:
        item.icon = "⚠️";
        item.title = "⚠️ Предупреждение";
        break;
    case NotificationManager::TYPE_SUCCESS:
        item.icon = "✅";
        item.title = "✅ Успешно";
        break;
    default:
        item.icon = "ℹ️";
        item.title = "ℹ️ Информация";
        break;
    }

    // Сохраняем полный текст для отображения в деталях
    item.text = notif.message;
    item.status = STATUS_IN_PROGRESS;

    s_globalNotifications.prepend(item);
    m_items.prepend(item);

    while (s_globalNotifications.size() > 500) {
        s_globalNotifications.removeLast();
    }
    while (m_items.size() > 500) {
        m_items.removeLast();
    }

    if (m_changesList) {
        filterChanged();
    }

    emit notificationAdded(item);
}

// ============================================================
// КОНТЕКСТНОЕ МЕНЮ
// ============================================================

void ChangesPanel::showContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_changesList->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "   background-color: #faf5ee;"
        "   border: 1px solid #ddd4c8;"
        "   border-radius: 8px;"
        "   padding: 4px;"
        "}"
        "QMenu::item {"
        "   padding: 6px 24px;"
        "   border-radius: 4px;"
        "   color: #3a2a1a;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QMenu::item:selected {"
        "   background-color: #ede8de;"
        "}"
        );

    QAction* actionMarkRead = new QAction("✅ Отметить прочитанным", this);
    connect(actionMarkRead, &QAction::triggered, this, &ChangesPanel::markSelectedAsRead);
    menu.addAction(actionMarkRead);

    QAction* actionDelete = new QAction("🗑️ Удалить", this);
    connect(actionDelete, &QAction::triggered, this, &ChangesPanel::deleteSelectedNotification);
    menu.addAction(actionDelete);

    menu.addSeparator();

    QAction* actionDetails = new QAction("ℹ️ Подробнее", this);
    connect(actionDetails, &QAction::triggered, this, &ChangesPanel::showNotificationDetailsDialog);
    menu.addAction(actionDetails);

    menu.exec(m_changesList->mapToGlobal(pos));
}

void ChangesPanel::deleteSelectedNotification()
{
    int currentRow = m_changesList->currentRow();
    if (currentRow < 0 || currentRow >= m_items.size()) return;

    if (QMessageBox::question(this, "Подтверждение",
                              "Удалить это уведомление?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_items.removeAt(currentRow);
        if (currentRow < s_globalNotifications.size()) {
            s_globalNotifications.removeAt(currentRow);
        }
        filterChanged();
        updateNotificationCount();
        saveNotificationsToDatabase();
    }
}

void ChangesPanel::markSelectedAsRead()
{
    int currentRow = m_changesList->currentRow();
    if (currentRow < 0 || currentRow >= m_items.size()) return;

    if (!m_items[currentRow].isRead) {
        m_items[currentRow].isRead = true;
        if (currentRow < s_globalNotifications.size()) {
            s_globalNotifications[currentRow].isRead = true;
        }
        filterChanged();
        updateNotificationCount();
        saveNotificationsToDatabase();
    }
}

// ============================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// ============================================================

QString ChangesPanel::getStatusIcon(DocumentStatus status)
{
    switch(status) {
    case STATUS_APPROVED: return "✅";
    case STATUS_IN_PROGRESS: return "🔄";
    case STATUS_REVIEW: return "👀";
    case STATUS_NEEDS_CHANGE: return "🔧";
    case STATUS_OBSOLETE: return "⚠️";
    case STATUS_ARCHIVED: return "📦";
    default: return "❓";
    }
}

QString ChangesPanel::getStatusColor(DocumentStatus status)
{
    switch(status) {
    case STATUS_APPROVED: return "#28a745";
    case STATUS_IN_PROGRESS: return "#ffc107";
    case STATUS_REVIEW: return "#17a2b8";
    case STATUS_NEEDS_CHANGE: return "#dc3545";
    case STATUS_OBSOLETE: return "#6c757d";
    case STATUS_ARCHIVED: return "#6c757d";
    default: return "#000000";
    }
}

void ChangesPanel::saveNotificationsToDatabase()
{
    if (m_db) {
        QList<NotificationItem> itemsToSave;
        for (const ChangeItem& item : s_globalNotifications) {
            NotificationItem notifItem;
            notifItem.icon = item.icon;
            notifItem.title = item.title;
            notifItem.text = item.text;
            notifItem.filePath = item.filePath;
            notifItem.folderPath = item.folderPath;
            notifItem.time = item.time;
            notifItem.isRead = item.isRead;
            notifItem.type = item.type;
            notifItem.status = item.status;
            notifItem.sourceUser = item.sourceUser;
            itemsToSave.append(notifItem);
        }
        m_db->saveNotifications(itemsToSave);
    }
}

void ChangesPanel::loadNotificationsFromDatabase()
{
    if (m_db) {
        QList<NotificationItem> loadedItems;
        m_db->loadNotifications(loadedItems);

        if (s_globalNotifications.isEmpty()) {
            for (const NotificationItem& item : loadedItems) {
                ChangeItem changeItem;
                changeItem.icon = item.icon;
                changeItem.title = item.title;
                changeItem.text = item.text;
                changeItem.filePath = item.filePath;
                changeItem.folderPath = item.folderPath;
                changeItem.time = item.time;
                changeItem.isRead = item.isRead;
                changeItem.type = item.type;
                changeItem.status = item.status;
                changeItem.sourceUser = item.sourceUser;
                s_globalNotifications.append(changeItem);
            }
            s_initialized = true;
        }
    }
}