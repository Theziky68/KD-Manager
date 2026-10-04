// BackupManager.cpp - полный файл

#include "BackupManager.h"
#include "DatabaseManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QSettings>

BackupManager& BackupManager::instance()
{
    static BackupManager instance;
    return instance;
}

BackupManager::BackupManager()
    : m_db(nullptr)
    , m_backupTimer(nullptr)
    , m_cleanupTimer(nullptr)
    , m_backupInterval(1)
    , m_retentionDays(3)
    , m_workStartHour(8)
    , m_workEndHour(17)
{
    QString appDir = QCoreApplication::applicationDirPath();
    m_backupDir = appDir + "/backups";

    QDir dir;
    if (!dir.exists(m_backupDir)) {
        dir.mkpath(m_backupDir);
    }

    m_backupTimer = new QTimer(this);
    connect(m_backupTimer, &QTimer::timeout, this, &BackupManager::onBackupTimer);

    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &BackupManager::checkAndCleanup);
    m_cleanupTimer->start(6 * 60 * 60 * 1000);
}

BackupManager::~BackupManager()
{
    stop();
}

void BackupManager::start()
{
    if (!m_db) {
        qDebug() << "❌ BackupManager: DatabaseManager не установлен!";
        return;
    }

    qDebug() << "✅ BackupManager запущен";
    qDebug() << "   📁 Папка бэкапов:" << m_backupDir;
    qDebug() << "   ⏰ Интервал:" << m_backupInterval << "час(ов)";
    qDebug() << "   📅 Хранение:" << m_retentionDays << "дней";
    qDebug() << "   🕐 Рабочее время: с" << m_workStartHour << ":00 до" << m_workEndHour << ":00";

    m_backupTimer->start(m_backupInterval * 60 * 60 * 1000);
    QTimer::singleShot(1000, this, &BackupManager::onBackupTimer);
    QTimer::singleShot(2000, this, &BackupManager::checkAndCleanup);
}

void BackupManager::stop()
{
    if (m_backupTimer) {
        m_backupTimer->stop();
    }
    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
    }
    qDebug() << "⏹️ BackupManager остановлен";
}

void BackupManager::setDatabaseManager(DatabaseManager* db)
{
    m_db = db;
}

void BackupManager::onBackupTimer()
{
    if (!isWorkingTime()) {
        int currentHour = getCurrentHour();
        qDebug() << "⏰ Бэкап пропущен (не рабочее время):" << currentHour << ":00";
        return;
    }

    QString todayDir = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    QString backupPath = m_backupDir + "/" + todayDir;
    QString hourStr = QDateTime::currentDateTime().toString("HH");
    QString backupFile = backupPath + "/kd_db_" + hourStr + ".dat";

    if (QFile::exists(backupFile)) {
        qDebug() << "⏭️ Бэкап за час" << hourStr << ":00 уже существует";
        return;
    }

    createBackup();
}

void BackupManager::createBackup()
{
    if (!m_db) {
        qDebug() << "❌ Ошибка: DatabaseManager не инициализирован!";
        return;
    }

    QString dbPath = m_db->getDatabasePath();
    if (dbPath.isEmpty() || !QFile::exists(dbPath)) {
        qDebug() << "❌ Ошибка: файл БД не найден:" << dbPath;
        return;
    }

    QString todayDir = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    QString backupPath = m_backupDir + "/" + todayDir;
    QString hourStr = QDateTime::currentDateTime().toString("HH");
    QString backupFile = backupPath + "/kd_db_" + hourStr + ".dat";

    QDir dir;
    if (!dir.exists(backupPath)) {
        if (!dir.mkpath(backupPath)) {
            qDebug() << "❌ Не удалось создать папку:" << backupPath;
            return;
        }
    }

    if (QFile::copy(dbPath, backupFile)) {
        m_lastBackupFile = backupFile;
        qDebug() << "✅ Бэкап создан:" << backupFile;
        qDebug() << "   📁 Размер:" << QFileInfo(backupFile).size() / 1024 << "KB";
    } else {
        qDebug() << "❌ Ошибка создания бэкапа:" << backupFile;
    }
}

void BackupManager::checkAndCleanup()
{
    cleanupOldBackups();
}

int BackupManager::getMaxBackupsPerDay() const
{
    int workHours = m_workEndHour - m_workStartHour;
    if (workHours <= 0) {
        workHours = 9;
    }

    int maxBackups = workHours / m_backupInterval;
    if (maxBackups < 1) {
        maxBackups = 1;
    }

    return maxBackups;
}

void BackupManager::cleanupOldBackups()
{
    QDir dir(m_backupDir);
    if (!dir.exists()) {
        return;
    }

    QStringList folders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QDate cutoffDate = QDate::currentDate().addDays(-m_retentionDays);

    for (const QString& folder : folders) {
        QDate folderDate = QDate::fromString(folder, "yyyy-MM-dd");
        if (folderDate.isValid() && folderDate < cutoffDate) {
            QString folderPath = m_backupDir + "/" + folder;
            QDir folderDir(folderPath);
            int fileCount = folderDir.entryList(QDir::Files).size();

            if (folderDir.removeRecursively()) {
                qDebug() << "🗑️ Удалена старая папка бэкапов:" << folder << "(" << fileCount << " файлов)";
            } else {
                qDebug() << "⚠️ Не удалось удалить папку:" << folder;
            }
        }
    }

    int maxPerDay = getMaxBackupsPerDay();

    for (const QString& folder : folders) {
        QDate folderDate = QDate::fromString(folder, "yyyy-MM-dd");
        if (!folderDate.isValid()) continue;

        QString folderPath = m_backupDir + "/" + folder;
        QDir folderDir(folderPath);

        QStringList files = folderDir.entryList(QStringList() << "kd_db_*.dat", QDir::Files);

        if (files.size() > maxPerDay) {
            files.sort();
            int toRemove = files.size() - maxPerDay;
            for (int i = 0; i < toRemove; ++i) {
                QString fileToRemove = folderPath + "/" + files[i];
                if (QFile::remove(fileToRemove)) {
                    qDebug() << "🗑️ Удалён лишний бэкап:" << files[i]
                             << "(осталось:" << (files.size() - i - 1) << ", макс:" << maxPerDay << ")";
                }
            }
        }
    }
}

QString BackupManager::getBackupDir() const
{
    return m_backupDir;
}

bool BackupManager::isWorkingTime() const
{
    int currentHour = getCurrentHour();
    return (currentHour >= m_workStartHour && currentHour < m_workEndHour);
}

int BackupManager::getCurrentHour() const
{
    return QDateTime::currentDateTime().time().hour();
}