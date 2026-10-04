// DatabaseManager.h
#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMap>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QTimer>
#include <QMutex>
#include <QReadWriteLock>
#include <QQueue>
#include <QThread>
#include <QSettings>
#include "FolderData.h"
#include "NotificationData.h"

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static DatabaseManager& instance();
    ~DatabaseManager();
    void debugPrintDocuments();

    // ===== ВЕРСИЯ БД =====
    static const int DATABASE_VERSION = 9;
    bool checkDatabaseVersion();
    bool upgradeDatabase(int fromVersion, int toVersion, QString& errorMessage);
    int getDatabaseVersion();
    QString getDatabaseVersionInfo();

    // ===== ИНИЦИАЛИЗАЦИЯ =====
    bool initDatabase(const QString& path);  // DEPRECATED - используйте initPostgreSQL или initSQLite
    void setDatabasePath(const QString& path);
    QString getDatabasePath() const { return m_dbPath; }
    QSqlDatabase& getDatabase() { return m_db; }

    // ===== ИНИЦИАЛИЗАЦИЯ POSTGRESQL =====
    bool tryConnectPostgreSQL(const QSettings& settings);
    bool initPostgreSQL(const QSettings& settings);
    bool createPostgreSQLTables();

    // ===== ИНИЦИАЛИЗАЦИЯ SQLITE =====
    bool initSQLite(const QSettings& settings);
    bool createSQLiteTables();

    // ===== ДИАЛОГ НАСТРОЙКИ БД =====
    bool showDatabaseSetupDialog(QSettings& settings);

    // ===== ОСНОВНЫЕ МЕТОДЫ =====
    void loadDatabase();
    void saveDatabase();
    void forceSave();
    bool isDirty() const { return m_dirty; }

    // ===== ПРОВЕРКА ЦЕЛОСТНОСТИ =====
    bool validateDataIntegrity();
    void rollbackDatabase();

    // ===== ВОССТАНОВЛЕНИЕ =====
    bool repairDatabase(QString& errorMessage);
    bool restoreFromBackup(const QString& backupPath, QString& errorMessage);
    bool checkDatabaseIntegrity(QStringList& errors);
    bool hasCircularReference(const QString& path, QMap<QString, bool>& visited);
    void setLastBackupPath(const QString& path) { m_lastBackupPath = path; }
    QString getLastBackupPath() const { return m_lastBackupPath; }

    // ===== КОНКУРЕНТНЫЙ ДОСТУП =====
    enum LockMode {
        LOCK_MODE_AUTO,
        LOCK_MODE_NORMAL,
        LOCK_MODE_EXCLUSIVE
    };

    enum FileAccessResult {
        ACCESS_OK,
        ACCESS_LOCKED,
        ACCESS_NOT_EXISTS,
        ACCESS_ERROR
    };

    void setLockMode(LockMode mode) { m_lockMode = mode; }
    LockMode getLockMode() const { return m_lockMode; }
    bool isNetworkDatabase() const;
    bool setLockingMode(LockMode mode);
    bool isDatabaseLocked() const;
    bool waitForDatabaseUnlock(int maxWaitMs = 5000);
    FileAccessResult checkDatabaseAccess();
    bool isDatabaseAccessible();
    bool acquireDatabaseLock(int timeoutMs = 1000);
    void releaseDatabaseLock();
    bool isDatabaseCorrupted();

    // ===== РАБОТА С ДОКУМЕНТАМИ =====
    DocInfo* getDocument(const QString& folderPath, const QString& filePath);
    bool hasDocument(const QString& folderPath, const QString& filePath);

    // Новые методы с folderPath (версия 6+)
    bool updateDocumentStatus(const QString& folderPath, const QString& filePath, const QString& status, const QString& reason = QString(), const QString& changedBy = QString());
    bool updateDocumentResponsible(const QString& folderPath, const QString& filePath, const QString& responsible, const QString& assignedBy = QString());
    bool addStatusChange(const QString& folderPath, const QString& filePath, const QString& oldStatus, const QString& newStatus, const QString& changedBy, const QString& reason = QString());
    bool addAssignment(const QString& folderPath, const QString& filePath, const QString& assignedTo, const QString& assignedBy, const QString& previousAssignee = QString(), const QString& comment = QString(), bool isManual = true);
    QList<StatusChange> getStatusHistory(const QString& folderPath, const QString& filePath);
    QList<Assignment> getAssignmentHistory(const QString& folderPath, const QString& filePath);

    // Старые методы для совместимости (DEPRECATED - используйте версии с folderPath)
    bool updateDocumentStatus_Legacy(const QString& filePath, const QString& status, const QString& reason = QString(), const QString& changedBy = QString());
    bool updateDocumentResponsible_Legacy(const QString& filePath, const QString& responsible, const QString& assignedBy = QString());
    bool addStatusChange_Legacy(const QString& filePath, const QString& oldStatus, const QString& newStatus, const QString& changedBy, const QString& reason = QString());
    bool addAssignment_Legacy(const QString& filePath, const QString& assignedTo, const QString& assignedBy, const QString& previousAssignee = QString(), const QString& comment = QString());

    // ===== РАБОТА С ПАПКАМИ =====
    bool addFolder(const QString& parentPath, const QString& name, FolderType type);
    bool removeFolder(const QString& path);
    bool renameFolder(const QString& oldPath, const QString& newName);

    // Безопасные методы для чтения (возвращают копию, не блокируют писателей)
    QMap<QString, FolderData> getFoldersCopy() const;
    QMap<QString, DocInfo> getDocumentsCopy() const;

    // Прямые ссылки (используются только в saveDatabase и при захвате m_mutex)
    QMap<QString, FolderData>& folders() { return m_folders; }
    QMap<QString, DocInfo>& documents() { return m_docs; }

    // ===== РАБОТА С ФАЙЛАМИ =====
    bool addFile(const QString& folderPath, const QString& filePath,
                 const QString& responsibleUser = "", const QString& status = "Новый");
    bool removeFile(const QString& folderPath, const QString& filePath);
    bool renameFile(const QString& folderPath, const QString& oldName, const QString& newName);
    bool moveFile(const QString& oldFolderPath, const QString& newFolderPath, const QString& filePath);

    // ===== БЛОКИРОВКА ФАЙЛОВ (ДЛЯ МНОГОПОЛЬЗОВАТЕЛЬСКОЙ РАБОТЫ) =====
    bool lockFile(const QString& filePath, const QString& userName);
    bool unlockFile(const QString& filePath, const QString& userName);
    bool isFileLocked(const QString& filePath) const;
    QString getFileLocker(const QString& filePath) const;

    // ===== РАБОТА С ФАЙЛАМИ-СИРОТАМИ =====
    QStringList getOrphanFiles() const;
    bool adoptOrphanFile(const QString& docKey, const QString& newFolderPath);
    int adoptAllOrphanFiles(const QString& newFolderPath);

    // ===== ПРОВЕРКА ФАЙЛОВ =====
    QStringList getMissingFiles() const;
    bool checkFileExists(const QString& filePath) const;
    bool syncFolderWithDisk(const QString& folderPath);

    // ===== УВЕДОМЛЕНИЯ =====
    void saveNotifications(const QList<NotificationItem>& notifications);
    void loadNotifications(QList<NotificationItem>& notifications);
    void clearNotifications();

    // ===== BATCH-РЕЖИМ =====
    void beginBatch();
    void endBatch();
    bool isBatchMode() const { return m_batchMode; }

    // ===== НАСТРОЙКИ =====
    void setAutoSaveInterval(int seconds);
    void enableAutoReload(bool enabled) { Q_UNUSED(enabled); }
    bool isAutoReloadEnabled() const { return false; }
    void checkFileChanged() {}

    // ===== БЭКАП =====
    void backupDatabase();
    void restoreDatabase(const QString& backupPath);
    void createDefaultStructure();

    // ===== ИНДЕКСЫ =====
    void createAllIndexes();

    // ===== СИНХРОНИЗАЦИЯ ПОЛЬЗОВАТЕЛЬСКИХ СЕССИЙ (v0.21) =====
    void registerUserSession(const QString& userFIO);
    QString getLastActiveUser();
    QString getCurrentActiveUserFIO() const;
    void closeUserSession(const QString& userFIO);

signals:
    void databaseChanged();
    void fileRenamed(const QString& folderPath, const QString& oldName, const QString& newName);
    void folderRemoved(const QString& folderPath);
    void folderRenamed(const QString& oldPath, const QString& newName);
    void saveFinished(bool success);
    void databaseReloaded();
    void databaseReloadFailed(const QString& error);
    void reloadStatusChanged(bool enabled);
    void statisticsChanged();
    void notificationsChanged();
    void assignmentChanged(const QString& docKey, const QString& assignedTo);

private:
    DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // ===== ВНУТРЕННИЕ МЕТОДЫ =====
    bool executeQuery(const QString& sql, const QMap<QString, QVariant>& params = QMap<QString, QVariant>());
    QSqlQuery executeQueryWithResult(const QString& sql, const QMap<QString, QVariant>& params = QMap<QString, QVariant>());
    QString getLastError() const;
    void markDirty();
    void loadFoldersFromDb();
    void loadDocumentsFromDb();
    void clearAllData();
    void removeFolderFromDb(const QString& path);
    void removeDocumentFromDb(const QString& path);
    void processSaveQueue();

    // ===== ВЕРСИЯ =====
    bool isVersionTableExists();
    void createVersionTable();

    // ===== ЛОГИРОВАНИЕ =====
    void logOperation(const QString& operation, const QString& details = QString());
    void logError(const QString& operation, const QString& error);

private slots:
    void onAutoSave();
    void onDebounceSave();
    void onSyncTimeout();

private:
    void synchronizeBothDatabases();

private:
    QSqlDatabase m_db;
    QSqlDatabase m_dbPostgreSQL;    // ДОБАВЛЕНО: Отдельное подключение к PostgreSQL
    QSqlDatabase m_dbSQLite;        // ДОБАВЛЕНО: Отдельное подключение к SQLite
    QString m_dbPath;
    QString m_backupPath;

    QMap<QString, FolderData> m_folders;
    QMap<QString, DocInfo> m_docs;  // Ключ: folderPath + "|" + filePath

    bool m_batchMode;
    bool m_batchHasChanges;
    bool m_dirty;
    int m_changeCounter;
    QDateTime m_lastSaveTime;

    QTimer* m_autoSaveTimer;
    QTimer* m_debounceTimer;
    QTimer* m_queueTimer;
    QTimer* m_syncTimer;            // ДОБАВЛЕНО: Таймер для периодической синхронизации
    int m_autoSaveInterval;
    int m_debounceDelay;

    QMutex m_mutex;
    mutable QReadWriteLock m_dataLock;      // ДОБАВЛЕНО: Для безопасного доступа к папкам и документам (mutable для const методов)
    QMutex m_syncMutex;             // ДОБАВЛЕНО: Для синхронизации между БД
    QQueue<QMap<QString, QString>> m_saveQueue;
    QMutex m_queueMutex;
    bool m_isSaving;

    bool m_autoBackupEnabled;
    int m_autoBackupThreshold;

    // ===== ВОССТАНОВЛЕНИЕ =====
    QString m_lastBackupPath;

    // ===== КОНКУРЕНТНЫЙ ДОСТУП =====
    LockMode m_lockMode;
    bool m_isNetworkDb;
    bool m_hasLock;
    QMutex m_lockMutex;

    // ДОБАВЛЕНО: Для двусторонней синхронизации
    bool m_isPostgresqlAvailable;
    bool m_isSqliteAvailable;

    // ДОБАВЛЕНО (v0.21): Текущий пользователь сессии
    QString m_currentUserFIO;
    QDateTime m_lastSyncTime;

    static const int MAX_WAL_ENTRIES = 1000;
    static const int SYNC_INTERVAL = 60000;  // ДОБАВЛЕНО: 60 сек синхронизация
};

#endif // DATABASEMANAGER_H