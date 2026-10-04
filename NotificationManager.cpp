#include "NotificationManager.h"
#include "DatabaseManager.h"
#include <QApplication>
#include <QStyle>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <windows.h>

NotificationManager::NotificationManager()
    : m_showTrayNotifications(true)
    , m_soundEnabled(false)
    , m_batchMode(false)
    , m_maxNotifications(200)
{
    m_checkTimer = new QTimer(this);
    connect(m_checkTimer, &QTimer::timeout, [this]() {
        checkForChanges();
    });
    m_checkTimer->start(60000);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    m_trayIcon->setVisible(true);

    loadNotifications();
}

NotificationManager::~NotificationManager()
{
    // ✅ ИСПРАВЛЕНО: Остановить и удалить таймер
    if (m_checkTimer) {
        m_checkTimer->stop();
        m_checkTimer->deleteLater();
    }

    if (m_batchMode) {
        endBatch();
    }
    saveNotifications();
}

NotificationManager& NotificationManager::instance()
{
    static NotificationManager instance;
    return instance;
}

// ============================================================
// BATCH-РЕЖИМ
// ============================================================

void NotificationManager::beginBatch()
{
    m_batchMode = true;
    m_batchNotifications.clear();
}

void NotificationManager::endBatch()
{
    m_batchMode = false;

    if (!m_batchNotifications.isEmpty()) {
        // Отправляем только ОДНО сводное уведомление
        Notification summary;
        summary.type = TYPE_INFO;
        summary.title = QString("📋 Выполнено %1 операций").arg(m_batchNotifications.size());

        QString message = "Выполнены операции:\n";
        int maxShow = qMin(3, m_batchNotifications.size());
        for (int i = 0; i < maxShow; ++i) {
            message += "• " + m_batchNotifications[i].title + "\n";
        }
        if (m_batchNotifications.size() > maxShow) {
            message += QString("... и еще %1").arg(m_batchNotifications.size() - maxShow);
        }
        summary.message = message;
        summary.timestamp = QDateTime::currentDateTime();
        summary.isRead = false;
        summary.isImportant = true;
        summary.sourceUser = "System";

        addNotificationInternal("", summary);
        m_batchNotifications.clear();
    }
}

bool NotificationManager::isBatchMode() const
{
    return m_batchMode;
}

// ============================================================
// ДОБАВЛЕНИЕ УВЕДОМЛЕНИЙ
// ============================================================

void NotificationManager::addNotification(const QString& user, const Notification& notif)
{
    if (m_batchMode) {
        m_batchNotifications.append(notif);
        return;
    }
    addNotificationInternal(user, notif);
}

void NotificationManager::addNotificationInternal(const QString& user, const Notification& notif)
{
    QList<Notification>& list = m_notifications[user];
    list.append(notif);

    // Ограничиваем количество уведомлений
    trimNotifications(user);

    saveNotifications();
    emit newNotification(user, notif);
    emit notificationsUpdated(user);

    if (m_showTrayNotifications && notif.isImportant) {
        showTrayNotification(notif);
    }
}

void NotificationManager::trimNotifications(const QString& user)
{
    QList<Notification>& list = m_notifications[user];
    if (list.size() > m_maxNotifications) {
        // Удаляем старые прочитанные уведомления
        int toRemove = list.size() - m_maxNotifications;
        int removed = 0;
        QList<Notification> newList;

        for (const Notification& notif : list) {
            if (notif.isRead && removed < toRemove) {
                removed++;
                continue;
            }
            newList.append(notif);
        }

        // Если все еще больше лимита - обрезаем хвост
        while (newList.size() > m_maxNotifications) {
            newList.removeFirst();
        }

        list = newList;
    }
}

void NotificationManager::addNotification(const QString& user, const QString& title,
                                          const QString& message, NotificationType type)
{
    Notification notif;
    notif.type = type;
    notif.title = title;
    notif.message = message;
    notif.timestamp = QDateTime::currentDateTime();
    notif.isRead = false;
    notif.isImportant = (type == TYPE_IMPORTANT || type == TYPE_CHANGE);
    notif.sourceUser = qgetenv("USERNAME");

    addNotification(user, notif);
}

// ============================================================
// ПОЛУЧЕНИЕ УВЕДОМЛЕНИЙ
// ============================================================

QList<NotificationManager::Notification> NotificationManager::getNotifications(const QString& user) const
{
    return m_notifications.value(user, QList<Notification>());
}

QList<NotificationManager::Notification> NotificationManager::getUnreadNotifications(const QString& user) const
{
    QList<Notification> result;
    for (const Notification& notif : m_notifications.value(user)) {
        if (!notif.isRead) {
            result.append(notif);
        }
    }
    return result;
}

QList<NotificationManager::Notification> NotificationManager::getImportantNotifications(const QString& user) const
{
    QList<Notification> result;
    for (const Notification& notif : m_notifications.value(user)) {
        if (notif.isImportant && !notif.isRead) {
            result.append(notif);
        }
    }
    return result;
}

int NotificationManager::getUnreadCount(const QString& user) const
{
    int count = 0;
    for (const Notification& notif : m_notifications.value(user)) {
        if (!notif.isRead) {
            count++;
        }
    }
    return count;
}

int NotificationManager::getTotalCount(const QString& user) const
{
    return m_notifications.value(user).size();
}

// ============================================================
// УПРАВЛЕНИЕ УВЕДОМЛЕНИЯМИ
// ============================================================

void NotificationManager::markAsRead(const QString& user, int index)
{
    QList<Notification>& list = m_notifications[user];
    if (index >= 0 && index < list.size()) {
        list[index].isRead = true;
        saveNotifications();
        emit notificationsUpdated(user);
    }
}

void NotificationManager::markAllAsRead(const QString& user)
{
    QList<Notification>& list = m_notifications[user];
    for (Notification& notif : list) {
        notif.isRead = true;
    }
    saveNotifications();
    emit notificationsUpdated(user);
}

void NotificationManager::clearNotifications(const QString& user)
{
    m_notifications[user].clear();
    saveNotifications();
    emit notificationsUpdated(user);
}

void NotificationManager::removeNotification(const QString& user, int index)
{
    QList<Notification>& list = m_notifications[user];
    if (index >= 0 && index < list.size()) {
        list.removeAt(index);
        saveNotifications();
        emit notificationsUpdated(user);
    }
}

void NotificationManager::clearAllNotifications()
{
    m_notifications.clear();
    saveNotifications();
    emit notificationsUpdated("");
}

QMap<NotificationManager::NotificationType, int> NotificationManager::getTypeStats(const QString& user) const
{
    QMap<NotificationType, int> stats;
    for (const Notification& notif : m_notifications.value(user)) {
        stats[notif.type]++;
    }
    return stats;
}

// ============================================================
// ПРОВЕРКА ИЗМЕНЕНИЙ
// ============================================================

void NotificationManager::checkForChanges(const QString& currentUser)
{
    QString user = currentUser.isEmpty() ? qgetenv("USERNAME") : currentUser;
    if (user.isEmpty()) return;

    QString dbPath = DatabaseManager::instance().getDatabasePath();
    QFileInfo fi(dbPath);

    if (!fi.exists()) return;

    QDateTime currentModified = fi.lastModified();

    if (m_lastCheckTime.isValid() && m_lastCheckTime != currentModified) {
        m_lastCheckTime = currentModified;

        Notification notif;
        notif.type = TYPE_CHANGE;
        notif.title = "📢 Обнаружены изменения";
        notif.message = "База данных была изменена.";
        notif.timestamp = QDateTime::currentDateTime();
        notif.isImportant = true;
        notif.isRead = false;
        notif.sourceUser = "System";

        addNotification(user, notif);
        emit changeDetected("", ChangeRecord("System", "Изменение", "База данных обновлена"));
    } else if (!m_lastCheckTime.isValid()) {
        m_lastCheckTime = currentModified;
    }
}

// ============================================================
// ТРЕЙ
// ============================================================

void NotificationManager::showTrayNotification(const Notification& notif)
{
    if (!m_trayIcon) return;

    QIcon icon;
    switch(notif.type) {
    case TYPE_INFO:
        icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);
        break;
    case TYPE_WARNING:
        icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
        break;
    case TYPE_SUCCESS:
        icon = QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton);
        break;
    case TYPE_IMPORTANT:
    case TYPE_CHANGE:
        icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical);
        break;
    default:
        icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);
    }

    m_trayIcon->setIcon(icon);
    m_trayIcon->showMessage(notif.title, notif.message, QSystemTrayIcon::Information, 3000);

    // Воспроизводим встроенный звук если включен
    if (m_soundEnabled) {
        MessageBeep(MB_ICONINFORMATION);
        qDebug() << "🔊 Звук уведомления воспроизведен";
    }
}

// ============================================================
// СОХРАНЕНИЕ / ЗАГРУЗКА
// ============================================================

void NotificationManager::saveNotifications()
{
    QSettings settings("KDManager", "Notifications");

    for (auto it = m_notifications.begin(); it != m_notifications.end(); ++it) {
        QString user = it.key();
        const QList<Notification>& list = it.value();

        settings.beginGroup(user);
        settings.setValue("count", list.size());

        for (int i = 0; i < list.size(); ++i) {
            const Notification& notif = list[i];
            settings.setValue(QString("notif_%1_type").arg(i), notif.type);
            settings.setValue(QString("notif_%1_title").arg(i), notif.title);
            settings.setValue(QString("notif_%1_message").arg(i), notif.message);
            settings.setValue(QString("notif_%1_file").arg(i), notif.filePath);
            settings.setValue(QString("notif_%1_folder").arg(i), notif.folderPath);
            settings.setValue(QString("notif_%1_timestamp").arg(i), notif.timestamp);
            settings.setValue(QString("notif_%1_read").arg(i), notif.isRead);
            settings.setValue(QString("notif_%1_important").arg(i), notif.isImportant);
            settings.setValue(QString("notif_%1_source").arg(i), notif.sourceUser);
        }

        settings.endGroup();
    }
}

void NotificationManager::loadNotifications()
{
    QSettings settings("KDManager", "Notifications");

    QStringList users = settings.childGroups();
    for (const QString& user : users) {
        settings.beginGroup(user);

        int count = settings.value("count", 0).toInt();
        QList<Notification> list;

        for (int i = 0; i < count; ++i) {
            Notification notif;
            notif.type = (NotificationType)settings.value(QString("notif_%1_type").arg(i), TYPE_INFO).toInt();
            notif.title = settings.value(QString("notif_%1_title").arg(i)).toString();
            notif.message = settings.value(QString("notif_%1_message").arg(i)).toString();
            notif.filePath = settings.value(QString("notif_%1_file").arg(i)).toString();
            notif.folderPath = settings.value(QString("notif_%1_folder").arg(i)).toString();
            notif.timestamp = settings.value(QString("notif_%1_timestamp").arg(i)).toDateTime();
            notif.isRead = settings.value(QString("notif_%1_read").arg(i), false).toBool();
            notif.isImportant = settings.value(QString("notif_%1_important").arg(i), false).toBool();
            notif.sourceUser = settings.value(QString("notif_%1_source").arg(i)).toString();
            list.append(notif);
        }

        m_notifications[user] = list;
        settings.endGroup();
    }
}

void NotificationManager::setCheckInterval(int seconds)
{
    if (m_checkTimer) {
        m_checkTimer->stop();
        m_checkTimer->start(seconds * 1000);
        qDebug() << "✅ Интервал проверки уведомлений установлен на" << seconds << "сек";
    }
}