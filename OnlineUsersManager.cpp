#include "OnlineUsersManager.h"
#include "DatabaseManager.h"
#include "DatabaseAbstraction.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QHostInfo>
#include <QDebug>
#include <QThread>

OnlineUsersManager::OnlineUsersManager()
    : m_db(&DatabaseManager::instance())
    , m_computerName(getComputerName())
{
    loadOnlineUsersFromDB();

    // Таймер для проверки неактивных пользователей каждую минуту
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &OnlineUsersManager::cleanupInactiveUsers);
    m_cleanupTimer->start(60000); // 1 минута

    // Таймер для периодического обновления списка из БД каждые 5 секунд
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &OnlineUsersManager::loadOnlineUsersFromDB);
    m_updateTimer->start(5000); // 5 секунд
}

OnlineUsersManager::~OnlineUsersManager()
{
    // ✅ ИСПРАВЛЕНО: правильное отключение текущего пользователя при выходе
    if (!m_currentUserFIO.isEmpty()) {
        setUserOffline(m_currentUserFIO);
    }

    // Остановить и удалить таймеры
    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
        m_cleanupTimer->deleteLater();
    }
    if (m_updateTimer) {
        m_updateTimer->stop();
        m_updateTimer->deleteLater();
    }
}

OnlineUsersManager& OnlineUsersManager::instance()
{
    static OnlineUsersManager instance;
    return instance;
}

QString OnlineUsersManager::getComputerName() const
{
    QString computerName = qgetenv("COMPUTERNAME");
    if (computerName.isEmpty()) {
        computerName = QHostInfo::localHostName();
    }
    if (computerName.isEmpty()) {
        computerName = "Unknown";
    }
    return computerName;
}

void OnlineUsersManager::setUserOnline(const QString& fio)
{
    if (fio.isEmpty() || !m_db) return;

    // ✅ ИСПРАВЛЕНО: сохранить текущее ФИО для отключения при выходе
    m_currentUserFIO = fio;

    int retries = 0;
    const int MAX_RETRIES = 5;

    while (retries < MAX_RETRIES) {
        // ===== ИСПРАВЛЕНИЕ: используем DatabaseAbstraction для INSERT OR REPLACE =====
        QMap<QString, QVariant> onlineUserValues;
        onlineUserValues["fio"] = fio;
        onlineUserValues["computer_name"] = m_computerName;
        onlineUserValues["last_seen"] = QDateTime::currentDateTime();
        onlineUserValues["is_active"] = 1;

        QString insertUserSql = DatabaseAbstraction::insertOrReplace("online_users", onlineUserValues, QStringList() << "fio" << "computer_name");

        QSqlQuery query(m_db->getDatabase());
        if (query.prepare(insertUserSql)) {
            for (auto it = onlineUserValues.begin(); it != onlineUserValues.end(); ++it) {
                query.bindValue(":" + it.key(), it.value());
            }

            if (query.exec()) {
                qDebug() << "✅ Пользователь добавлен онлайн:" << fio << "на" << m_computerName;
                emit userOnline(fio);

                {
                    QMutexLocker locker(&m_usersMutex);
                    loadOnlineUsersFromDB();  // Перезагружаем список
                }
                emit onlineUsersChanged();
                return;
            }
        }

        retries++;
        if (retries < MAX_RETRIES) {
            qDebug() << "⏳ Попытка записи" << retries << "из" << MAX_RETRIES << "Ошибка:" << query.lastError().text();
            QThread::msleep(100 * retries);  // Экспоненциальная задержка
        }
    }

    qDebug() << "❌ Не удалось добавить пользователя онлайн после" << MAX_RETRIES << "попыток";
}

void OnlineUsersManager::setUserOffline(const QString& fio)
{
    if (!m_db) return;

    QString targetFio = fio;
    if (targetFio.isEmpty()) {
        // Если не передан ФИО, ищем текущего пользователя по имени компьютера
        // Это для выхода при закрытии приложения
        // На самом деле нужно передать текущее ФИО из main.cpp
        return;
    }

    QSqlQuery query(m_db->getDatabase());
    query.prepare(
        "UPDATE online_users SET is_active = 0, last_seen = :last_seen "
        "WHERE fio = :fio AND computer_name = :computer_name"
    );
    query.bindValue(":fio", targetFio);
    query.bindValue(":computer_name", m_computerName);
    query.bindValue(":last_seen", QDateTime::currentDateTime());

    if (!query.exec()) {
        qDebug() << "❌ Ошибка отключения пользователя:" << query.lastError().text();
        return;
    }

    qDebug() << "✅ Пользователь отключен:" << targetFio;
    emit userOffline(targetFio);
    emit onlineUsersChanged();
}

QList<OnlineUser> OnlineUsersManager::getOnlineUsers() const
{
    return m_onlineUsers;
}

int OnlineUsersManager::getOnlineCount() const
{
    int count = 0;
    for (const auto& user : m_onlineUsers) {
        if (user.isActive) count++;
    }
    return count;
}

bool OnlineUsersManager::isUserOnline(const QString& fio) const
{
    for (const auto& user : m_onlineUsers) {
        if (user.fio == fio && user.isActive) {
            return true;
        }
    }
    return false;
}

QDateTime OnlineUsersManager::getLastSeenTime(const QString& fio) const
{
    for (const auto& user : m_onlineUsers) {
        if (user.fio == fio) {
            return user.lastSeen;
        }
    }
    return QDateTime();
}

void OnlineUsersManager::cleanupInactiveUsers()
{
    if (!m_db) return;

    QDateTime now = QDateTime::currentDateTime();
    QDateTime fiveMinutesAgo = now.addSecs(-300); // 5 минут

    QSqlQuery query(m_db->getDatabase());

    // ✅ ИСПРАВЛЕНО: использование parameterized queries вместо string interpolation
    query.prepare(
        "DELETE FROM online_users "
        "WHERE is_active = 0 AND last_seen < :cutoff_time"
    );
    query.bindValue(":cutoff_time", fiveMinutesAgo);

    if (!query.exec()) {
        qDebug() << "❌ Ошибка очистки неактивных пользователей:" << query.lastError().text();
        return;
    }

    int rowsDeleted = query.numRowsAffected();
    if (rowsDeleted > 0) {
        qDebug() << "🧹 Удалено неактивных пользователей:" << rowsDeleted;
        loadOnlineUsersFromDB();
        emit onlineUsersChanged();
    }
}

void OnlineUsersManager::loadOnlineUsersFromDB()
{
    if (!m_db) return;

    m_onlineUsers.clear();

    QSqlQuery query(m_db->getDatabase());

    // Сначала проверяем, существует ли таблица
    QSqlQuery checkTable(m_db->getDatabase());
    if (!checkTable.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='online_users'")) {
        qDebug() << "❌ Ошибка проверки таблицы online_users:" << checkTable.lastError().text();
        return;
    }

    if (!checkTable.next()) {
        qDebug() << "⚠️ Таблица online_users не существует, пропускаем загрузку";
        return;
    }

    // Таблица существует, загружаем данные
    if (!query.exec("SELECT fio, computer_name, last_seen, is_active FROM online_users WHERE is_active = 1 ORDER BY last_seen DESC")) {
        qDebug() << "❌ Ошибка загрузки онлайн пользователей:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        QString fio = query.value("fio").toString();
        QString computerName = query.value("computer_name").toString();
        QDateTime lastSeen = query.value("last_seen").toDateTime();
        bool isActive = query.value("is_active").toInt() == 1;

        OnlineUser user(fio, computerName, lastSeen);
        user.isActive = isActive;
        m_onlineUsers.append(user);
    }

    qDebug() << "✅ Загружено онлайн пользователей:" << m_onlineUsers.size();
    emit onlineUsersChanged();
}

