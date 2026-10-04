// DatabaseManager.cpp
#include "DatabaseManager.h"
#include "DatabaseSetupDialog.h"
#include "DatabaseAbstraction.h"
#include "NotificationManager.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QSqlDriver>
#include <QVariant>
#include <QThread>
#include <QElapsedTimer>
#include <QSettings>
#include <QMessageBox>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/statvfs.h>
#endif

// ===== СИНГЛТОН =====
DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager s_instance;
    return s_instance;
}

// ===== ДЕСТРУКТОР =====
DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
    }
    if (m_debounceTimer) {
        m_debounceTimer->stop();
    }
    if (m_queueTimer) {
        m_queueTimer->stop();
    }
    qDebug() << "✅ DatabaseManager деструктор завершен";
}

DatabaseManager::DatabaseManager()
    : m_batchMode(false)
    , m_batchHasChanges(false)
    , m_dirty(false)
    , m_changeCounter(0)
    , m_autoSaveInterval(5)  // ===== УРОВЕНЬ 1: Уменьшено с 60 до 5 сек для защиты от потери данных
    , m_debounceDelay(5000)
    , m_isSaving(false)
    , m_autoBackupEnabled(true)
    , m_autoBackupThreshold(10)
    , m_lockMode(LOCK_MODE_AUTO)
    , m_isNetworkDb(false)
    , m_hasLock(false)
    , m_isPostgresqlAvailable(false)
    , m_isSqliteAvailable(false)
{
    // ===== ОПРЕДЕЛЯЕМ ТИП И ПАРАМЕТРЫ БД =====
    QSettings settings("KDManager", "Settings");

    // Проверяем, есть ли сохраненные параметры PostgreSQL
    QString dbType = settings.value("database/type", "auto").toString();  // auto/sqlite/postgresql

    qDebug() << "=== ИНИЦИАЛИЗАЦИЯ БД (ДВУСТОРОННЯЯ СИНХРОНИЗАЦИЯ) ===";
    qDebug() << "Тип БД:" << dbType;

    bool dbInitialized = false;

    // ===== НОВАЯ СТРАТЕГИЯ: ИНИЦИАЛИЗИРУЕМ ОБЕ БД =====
    if (dbType == "postgresql" || dbType == "auto") {
        // Пытаемся подключиться к PostgreSQL
        qDebug() << "📡 Попытка подключения к PostgreSQL...";
        if (tryConnectPostgreSQL(settings)) {
            if (initPostgreSQL(settings)) {
                m_isPostgresqlAvailable = true;
                m_isNetworkDb = true;
                dbInitialized = true;
                qDebug() << "✅ PostgreSQL инициализирован как основная БД";
            }
        }
    }

    // ===== ИНИЦИАЛИЗИРУЕМ SQLite КАК РЕЗЕРВНУЮ БД =====
    qDebug() << "💻 Инициализация SQLite как резервной БД...";
    if (initSQLite(settings)) {
        m_isSqliteAvailable = true;
        qDebug() << "✅ SQLite инициализирован как резервная БД";

        if (!dbInitialized) {
            dbInitialized = true;
            qDebug() << "✅ SQLite установлен как основная БД (PostgreSQL недоступен)";
        }
    }

    // ===== ДВУСТОРОННЯЯ СИНХРОНИЗАЦИЯ =====
    if (m_isPostgresqlAvailable && m_isSqliteAvailable) {
        qDebug() << "🔄 Синхронизируем PostgreSQL ↔ SQLite...";
        synchronizeBothDatabases();
        qDebug() << "✅ Синхронизация завершена";
    }

    // Если БД не инициализирована - показываем диалог
    if (!dbInitialized) {
        qDebug() << "❌ Не удалось инициализировать БД автоматически, показываем диалог";
        if (showDatabaseSetupDialog(settings)) {
            // Пользователь заполнил параметры
            dbType = settings.value("database/type", "auto").toString();
            if (dbType == "postgresql") {
                if (!initPostgreSQL(settings)) {
                    QMessageBox::critical(nullptr, "❌ Ошибка",
                        "Не удалось подключиться к PostgreSQL с указанными параметрами.\n"
                        "Проверьте параметры и попробуйте еще раз.");
                    return;
                }
            } else {
                if (!initSQLite(settings)) {
                    QMessageBox::critical(nullptr, "❌ Ошибка",
                        "Не удалось инициализировать SQLite БД.");
                    return;
                }
            }
        } else {
            // Пользователь отменил - выход
            qDebug() << "⚠️ Пользователь отменил настройку БД";
            return;
        }
    }

    // ===== ИНИЦИАЛИЗАЦИЯ СЛОЯ АБСТРАКЦИИ =====
    DatabaseAbstraction::detectDatabaseType(m_db);

    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &DatabaseManager::onAutoSave);
    m_autoSaveTimer->start(m_autoSaveInterval * 1000);

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this, &DatabaseManager::onDebounceSave);

    m_queueTimer = new QTimer(this);
    m_queueTimer->setSingleShot(true);
    connect(m_queueTimer, &QTimer::timeout, this, [this]() { processSaveQueue(); });

    // ДОБАВЛЕНО: Таймер для периодической синхронизации
    m_syncTimer = new QTimer(this);
    connect(m_syncTimer, &QTimer::timeout, this, &DatabaseManager::onSyncTimeout);
    if (m_isPostgresqlAvailable && m_isSqliteAvailable) {
        m_syncTimer->start(SYNC_INTERVAL);  // Синхронизация каждые 60 сек
        qDebug() << "✅ Таймер синхронизации запущен (60 сек)";
    }
}

// ===== ДИАЛОГ НАСТРОЙКИ БД =====
bool DatabaseManager::showDatabaseSetupDialog(QSettings& settings)
{
    DatabaseSetupDialog dialog;

    // Загружаем сохраненные параметры
    dialog.setHostname(settings.value("database/postgres_host", "localhost").toString());
    dialog.setPort(settings.value("database/postgres_port", "5432").toInt());
    dialog.setUsername(settings.value("database/postgres_user", "kd_user").toString());
    dialog.setPassword(settings.value("database/postgres_password", "").toString());
    dialog.setDatabaseName(settings.value("database/postgres_database", "kd_documents").toString());
    dialog.setSqlitePath(settings.value("database/sqlite_path", "N:/kd.db").toString());

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    // Сохраняем выбор пользователя
    QString dbType = dialog.getDatabaseType();
    settings.setValue("database/type", dbType);

    if (dbType == "postgresql") {
        settings.setValue("database/postgres_host", dialog.getHostname());
        settings.setValue("database/postgres_port", dialog.getPort());
        settings.setValue("database/postgres_user", dialog.getUsername());
        settings.setValue("database/postgres_password", dialog.getPassword());
        settings.setValue("database/postgres_database", dialog.getDatabaseName());
    } else {
        settings.setValue("database/sqlite_path", dialog.getSqlitePath());
    }

    settings.sync();
    return true;
}

// ============================================================
// ИНИЦИАЛИЗАЦИЯ POSTGRESQL
// ============================================================

bool DatabaseManager::tryConnectPostgreSQL(const QSettings& settings)
{
    QString host = settings.value("database/postgres_host", "localhost").toString();
    QString port = settings.value("database/postgres_port", "5432").toString();
    QString user = settings.value("database/postgres_user", "kd_user").toString();
    QString password = settings.value("database/postgres_password", "").toString();
    QString database = settings.value("database/postgres_database", "kd_documents").toString();

    QSqlDatabase testDb = QSqlDatabase::addDatabase("QPSQL", "test_connection");
    testDb.setHostName(host);
    testDb.setPort(port.toInt());
    testDb.setUserName(user);
    testDb.setPassword(password);
    testDb.setDatabaseName(database);

    // ===== ДОБАВЛЯЕМ ТАЙМАУТ ПОДКЛЮЧЕНИЯ (5 СЕКУНД) =====
    testDb.setConnectOptions("connect_timeout=5");

    bool connected = testDb.open();

    if (connected) {
        qDebug() << "✅ PostgreSQL доступен:" << host << ":" << port;
    } else {
        qDebug() << "⚠️ PostgreSQL недоступен:" << testDb.lastError().text();
    }

    QSqlDatabase::removeDatabase("test_connection");
    return connected;
}

bool DatabaseManager::initPostgreSQL(const QSettings& settings)
{
    if (m_db.isOpen()) {
        m_db.close();
    }

    QSqlDatabase::removeDatabase("qt_sql_default_connection");

    // Читаем параметры подключения
    QString host = settings.value("database/postgres_host", "localhost").toString();
    QString port = settings.value("database/postgres_port", "5432").toString();
    QString user = settings.value("database/postgres_user", "kd_user").toString();
    QString password = settings.value("database/postgres_password", "").toString();
    QString database = settings.value("database/postgres_database", "kd_documents").toString();

    m_db = QSqlDatabase::addDatabase("QPSQL");
    m_db.setHostName(host);
    m_db.setPort(port.toInt());
    m_db.setUserName(user);
    m_db.setPassword(password);
    m_db.setDatabaseName(database);

    // ===== ДОБАВЛЯЕМ ТАЙМАУТ ПОДКЛЮЧЕНИЯ (5 СЕКУНД) =====
    m_db.setConnectOptions("connect_timeout=5");

    if (!m_db.open()) {
        qDebug() << "❌ Не удалось подключиться к PostgreSQL:" << m_db.lastError().text();
        return false;
    }

    qDebug() << "✅ Подключено к PostgreSQL:" << host << ":" << port;
    m_isNetworkDb = true;

    // Создаем таблицы если их нет
    if (!createPostgreSQLTables()) {
        qDebug() << "❌ Ошибка создания таблиц PostgreSQL";
        return false;
    }

    // Загружаем данные
    qDebug() << "📂 Загружаем данные из PostgreSQL...";
    loadDatabase();
    qDebug() << "✅ Данные загружены. Папок:" << m_folders.size() << "Файлов:" << m_docs.size();

    return true;
}

bool DatabaseManager::createPostgreSQLTables()
{
    QStringList createQueries = {
        // Таблица папок
        "CREATE TABLE IF NOT EXISTS folders ("
        "    id SERIAL PRIMARY KEY,"
        "    path TEXT UNIQUE NOT NULL,"
        "    name TEXT NOT NULL,"
        "    type INTEGER DEFAULT 0,"
        "    parent_path TEXT,"
        "    created_by TEXT DEFAULT '',"
        "    modified_by TEXT DEFAULT '',"
        "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");",

        // Таблица документов
        "CREATE TABLE IF NOT EXISTS documents ("
        "    id SERIAL PRIMARY KEY,"
        "    path TEXT NOT NULL,"
        "    name TEXT NOT NULL,"
        "    folder_path TEXT NOT NULL,"
        "    designation TEXT DEFAULT '',"
        "    material TEXT DEFAULT '',"
        "    mass TEXT DEFAULT '',"
        "    scale TEXT DEFAULT '',"
        "    format TEXT DEFAULT '',"
        "    status TEXT DEFAULT 'В работе',"
        "    comment TEXT DEFAULT '',"
        "    responsible_user TEXT DEFAULT '',"
        "    size BIGINT DEFAULT 0,"
        "    modified TIMESTAMP,"
        "    created_by TEXT DEFAULT '',"
        "    modified_by TEXT DEFAULT '',"
        "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "    locked_by TEXT DEFAULT NULL,"
        "    locked_at TIMESTAMP DEFAULT NULL"
        ");",

        // Таблица уведомлений
        "CREATE TABLE IF NOT EXISTS notifications ("
        "    id SERIAL PRIMARY KEY,"
        "    icon TEXT DEFAULT 'ℹ️',"
        "    title TEXT NOT NULL,"
        "    text TEXT DEFAULT '',"
        "    file_path TEXT DEFAULT '',"
        "    folder_path TEXT DEFAULT '',"
        "    time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "    is_read INTEGER DEFAULT 0,"
        "    type INTEGER DEFAULT 0,"
        "    status INTEGER DEFAULT 1,"
        "    source_user TEXT DEFAULT '',"
        "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");",

        // Таблица комментариев
        "CREATE TABLE IF NOT EXISTS comments ("
        "    id SERIAL PRIMARY KEY,"
        "    file_path TEXT NOT NULL,"
        "    folder_path TEXT NOT NULL,"
        "    user TEXT NOT NULL,"
        "    text TEXT NOT NULL,"
        "    time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "    is_reply INTEGER DEFAULT 0,"
        "    reply_to INTEGER DEFAULT -1,"
        "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");",

        // Таблица онлайн пользователей
        "CREATE TABLE IF NOT EXISTS online_users ("
        "    id SERIAL PRIMARY KEY,"
        "    fio TEXT NOT NULL,"
        "    computer_name TEXT DEFAULT '',"
        "    last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "    is_active INTEGER DEFAULT 1,"
        "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "    UNIQUE(fio, computer_name)"
        ");",

        // Таблица версии БД
        "CREATE TABLE IF NOT EXISTS db_version ("
        "    version INTEGER PRIMARY KEY,"
        "    last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");"
    };

    // Создаем индексы для лучшей производительности
    QStringList indexQueries = {
        "CREATE INDEX IF NOT EXISTS idx_documents_folder ON documents(folder_path);",
        "CREATE INDEX IF NOT EXISTS idx_documents_status ON documents(status);",
        "CREATE INDEX IF NOT EXISTS idx_notifications_user ON notifications(source_user);",
        "CREATE INDEX IF NOT EXISTS idx_online_users_fio ON online_users(fio);"
    };

    for (const QString& sql : createQueries) {
        if (!executeQuery(sql)) {
            qDebug() << "❌ Ошибка создания таблицы:" << getLastError();
            return false;
        }
    }

    for (const QString& sql : indexQueries) {
        executeQuery(sql);  // Индексы - некритично если не создадутся
    }

    qDebug() << "✅ Таблицы PostgreSQL созданы/проверены";
    return true;
}

// ============================================================
// ИНИЦИАЛИЗАЦИЯ SQLITE
// ============================================================

bool DatabaseManager::initSQLite(const QSettings& settings)
{
    if (m_db.isOpen()) {
        m_db.close();
    }

    QSqlDatabase::removeDatabase("qt_sql_default_connection");

    // Определяем путь к БД
    QString dbPath = settings.value("database/sqlite_path", "").toString();

    if (dbPath.isEmpty() || !QFile::exists(dbPath)) {
        // Проверяем N:/
        if (QFile::exists("N:/")) {
            dbPath = "N:/kd.db";
            qDebug() << "✅ Используем N:/ для БД";
        } else {
            // Локальная папка
            QString appDir = QCoreApplication::applicationDirPath();
            dbPath = appDir + "/kd.db";
            qDebug() << "⚠️ Используем локальную папку для БД";
        }

        // Сохраняем для следующего запуска
        ((QSettings&)settings).setValue("database/sqlite_path", dbPath);
        ((QSettings&)settings).sync();
    }

    m_dbPath = dbPath;
    m_backupPath = dbPath;
    m_backupPath = m_backupPath.replace(".db", "_backup.db");

    qDebug() << "Путь к БД:" << m_dbPath;

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        qDebug() << "❌ Не удалось открыть SQLite БД:" << m_db.lastError().text();
        return false;
    }

    qDebug() << "✅ SQLite БД открыта:" << m_dbPath;

    // Оптимизация SQLite для работы по сети (PRAGMA команды работают только в SQLite!)
    if (!DatabaseAbstraction::pragma("foreign_keys = ON").isEmpty())
        executeQuery(DatabaseAbstraction::pragma("foreign_keys = ON"));
    if (!DatabaseAbstraction::pragma("journal_mode = WAL").isEmpty())
        executeQuery(DatabaseAbstraction::pragma("journal_mode = WAL"));
    if (!DatabaseAbstraction::pragma("synchronous = NORMAL").isEmpty())
        executeQuery(DatabaseAbstraction::pragma("synchronous = NORMAL"));
    if (!DatabaseAbstraction::pragma("cache_size = 10000").isEmpty())
        executeQuery(DatabaseAbstraction::pragma("cache_size = 10000"));
    if (!DatabaseAbstraction::pragma("temp_store = MEMORY").isEmpty())
        executeQuery(DatabaseAbstraction::pragma("temp_store = MEMORY"));
    if (!DatabaseAbstraction::pragma("busy_timeout = 5000").isEmpty())
        executeQuery(DatabaseAbstraction::pragma("busy_timeout = 5000"));
    if (!DatabaseAbstraction::pragma("wal_autocheckpoint = 1000").isEmpty())
        executeQuery(DatabaseAbstraction::pragma("wal_autocheckpoint = 1000"));

    m_isNetworkDb = QFile::exists("N:/");

    if (m_isNetworkDb) {
        qDebug() << "📡 Обнаружена сетевая БД, используется EXCLUSIVE блокировка";
    } else {
        qDebug() << "💻 Локальная БД, используется NORMAL блокировка";
    }

    // Проверяем наличие таблиц и создаем если нужно
    if (!createSQLiteTables()) {
        qDebug() << "❌ Ошибка создания таблиц SQLite";
        return false;
    }

    // Загружаем данные
    qDebug() << "📂 Загружаем данные из SQLite...";
    loadDatabase();
    qDebug() << "✅ Данные загружены. Папок:" << m_folders.size() << "Файлов:" << m_docs.size();

    return true;
}

bool DatabaseManager::createSQLiteTables()
{
    // ===== УРОВЕНЬ 2: Включаем WAL режим для защиты от потери данных =====
    QSqlQuery pragmaQuery(m_db);

    // WAL (Write-Ahead Logging) - все изменения сначала пишутся в журнал, потом в БД
    if (!pragmaQuery.exec("PRAGMA journal_mode=WAL;")) {
        qDebug() << "⚠️ Не удалось включить WAL режим:" << pragmaQuery.lastError().text();
    } else {
        qDebug() << "✅ WAL режим включен (защита от потери данных при крашах)";
    }

    // NORMAL режим - баланс между скоростью и надежностью
    if (!pragmaQuery.exec("PRAGMA synchronous=NORMAL;")) {
        qDebug() << "⚠️ Не удалось установить synchronous=NORMAL:" << pragmaQuery.lastError().text();
    } else {
        qDebug() << "✅ Установлен режим синхронизации NORMAL";
    }

    // Увеличиваем размер кэша для быстрой работы
    if (!pragmaQuery.exec("PRAGMA cache_size=5000;")) {
        qDebug() << "⚠️ Не удалось установить cache_size:" << pragmaQuery.lastError().text();
    } else {
        qDebug() << "✅ Размер кэша установлен на 5000 страниц";
    }

    QStringList createQueries = {
        "CREATE TABLE IF NOT EXISTS folders ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    path TEXT UNIQUE NOT NULL,"
        "    name TEXT NOT NULL,"
        "    type INTEGER DEFAULT 0,"
        "    parent_path TEXT,"
        "    created_by TEXT DEFAULT '',"
        "    modified_by TEXT DEFAULT '',"
        "    status TEXT DEFAULT 'В работе',"
        "    responsible_user TEXT DEFAULT '',"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",

        "CREATE TABLE IF NOT EXISTS documents ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    path TEXT NOT NULL,"
        "    name TEXT NOT NULL,"
        "    folder_path TEXT NOT NULL,"
        "    designation TEXT DEFAULT '',"
        "    material TEXT DEFAULT '',"
        "    mass TEXT DEFAULT '',"
        "    scale TEXT DEFAULT '',"
        "    format TEXT DEFAULT '',"
        "    status TEXT DEFAULT 'В работе',"
        "    comment TEXT DEFAULT '',"
        "    responsible_user TEXT DEFAULT '',"
        "    size INTEGER DEFAULT 0,"
        "    modified DATETIME,"
        "    created_by TEXT DEFAULT '',"
        "    modified_by TEXT DEFAULT '',"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    locked_by TEXT DEFAULT NULL,"
        "    locked_at DATETIME DEFAULT NULL"
        ");",

        "CREATE TABLE IF NOT EXISTS notifications ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    icon TEXT DEFAULT 'ℹ️',"
        "    title TEXT NOT NULL,"
        "    text TEXT DEFAULT '',"
        "    file_path TEXT DEFAULT '',"
        "    folder_path TEXT DEFAULT '',"
        "    time DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    is_read INTEGER DEFAULT 0,"
        "    type INTEGER DEFAULT 0,"
        "    status INTEGER DEFAULT 1,"
        "    source_user TEXT DEFAULT '',"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",

        "CREATE TABLE IF NOT EXISTS comments ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    file_path TEXT NOT NULL,"
        "    folder_path TEXT NOT NULL,"
        "    doc_key TEXT DEFAULT '',"
        "    user TEXT NOT NULL,"
        "    text TEXT NOT NULL,"
        "    time DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    is_reply INTEGER DEFAULT 0,"
        "    reply_to INTEGER DEFAULT -1,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",

        "CREATE TABLE IF NOT EXISTS online_users ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    fio TEXT NOT NULL,"
        "    computer_name TEXT DEFAULT '',"
        "    last_seen DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    is_active INTEGER DEFAULT 1,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    UNIQUE(fio, computer_name)"
        ");"
    };

    for (const QString& sql : createQueries) {
        if (!executeQuery(sql)) {
            qDebug() << "❌ Ошибка создания таблицы:" << getLastError();
            return false;
        }
    }

    qDebug() << "✅ Таблицы SQLite созданы/проверены";
    return true;
}

bool DatabaseManager::executeQuery(const QString& sql, const QMap<QString, QVariant>& params)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        qDebug() << "❌ База данных не открыта!";
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.prepare(sql)) {
        qDebug() << "❌ Ошибка подготовки запроса:" << sql << query.lastError().text();
        return false;
    }

    for (auto it = params.begin(); it != params.end(); ++it) {
        query.bindValue(":" + it.key(), it.value());
    }

    if (!query.exec()) {
        qDebug() << "❌ Ошибка выполнения запроса:" << sql << query.lastError().text();
        return false;
    }

    return true;
}

QSqlQuery DatabaseManager::executeQueryWithResult(const QString& sql, const QMap<QString, QVariant>& params)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare(sql);

    for (auto it = params.begin(); it != params.end(); ++it) {
        query.bindValue(":" + it.key(), it.value());
    }

    query.exec();
    return query;
}

QString DatabaseManager::getLastError() const
{
    return m_db.lastError().text();
}

// ============================================================
// ЛОГИРОВАНИЕ
// ============================================================

void DatabaseManager::logOperation(const QString& operation, const QString& details)
{
    QString logEntry = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
    .arg(operation);
    if (!details.isEmpty()) {
        logEntry += ": " + details;
    }
    qDebug() << logEntry;

    QString logPath = QCoreApplication::applicationDirPath() + "/kd.log";
    QFile logFile(logPath);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << logEntry << "\n";
        logFile.close();
    }
}

void DatabaseManager::logError(const QString& operation, const QString& error)
{
    QString logEntry = QString("[%1] ❌ ОШИБКА %2: %3")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                           .arg(operation)
                           .arg(error);
    qDebug() << logEntry;

    QString logPath = QCoreApplication::applicationDirPath() + "/kd.log";
    QFile logFile(logPath);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << logEntry << "\n";
        logFile.close();
    }
}

// ============================================================
// ВЕРСИЯ БД
// ============================================================

bool DatabaseManager::isVersionTableExists()
{
    if (!m_db.isOpen()) {
        return false;
    }

    QSqlQuery query(m_db);

    // Для PostgreSQL
    if (DatabaseAbstraction::isPostgreSQL()) {
        query.prepare("SELECT EXISTS(SELECT 1 FROM information_schema.tables WHERE table_name='version')");
    } else {
        // Для SQLite
        query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='version'");
    }

    query.exec();
    if (query.next()) {
        if (DatabaseAbstraction::isPostgreSQL()) {
            return query.value(0).toBool();
        } else {
            return true;
        }
    }
    return false;
}

void DatabaseManager::createVersionTable()
{
    if (!m_db.isOpen()) {
        return;
    }

    QSqlQuery query(m_db);
    if (!query.exec("CREATE TABLE IF NOT EXISTS version ("
                    "    version INTEGER NOT NULL,"
                    "    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                    "    updated_by TEXT DEFAULT ''"
                    ")")) {
        qDebug() << "❌ Ошибка создания таблицы version:" << query.lastError().text();
        return;
    }

    if (!isVersionTableExists()) {
        return;
    }

    query.exec("SELECT COUNT(*) FROM version");
    if (query.next() && query.value(0).toInt() == 0) {
        query.prepare("INSERT INTO version (version, updated_by) VALUES (:version, :user)");
        query.bindValue(":version", DATABASE_VERSION);
        query.bindValue(":user", qgetenv("USERNAME"));
        if (!query.exec()) {
            qDebug() << "❌ Ошибка вставки версии:" << query.lastError().text();
        }
    }
}

int DatabaseManager::getDatabaseVersion()
{
    if (!m_db.isOpen()) {
        return -1;
    }

    if (!isVersionTableExists()) {
        createVersionTable();
        return DATABASE_VERSION;
    }

    QSqlQuery query(m_db);
    query.exec("SELECT version FROM version ORDER BY updated_at DESC LIMIT 1");

    if (query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}

QString DatabaseManager::getDatabaseVersionInfo()
{
    if (!m_db.isOpen()) {
        return "БД не открыта";
    }

    if (!isVersionTableExists()) {
        return "Таблица версии не найдена (версия 1)";
    }

    QSqlQuery query(m_db);
    query.exec("SELECT version, updated_at, updated_by FROM version ORDER BY updated_at DESC LIMIT 1");

    if (query.next()) {
        int version = query.value(0).toInt();
        QString updatedAt = query.value(1).toDateTime().toString("dd.MM.yyyy hh:mm");
        QString updatedBy = query.value(2).toString();
        return QString("Версия %1 (обновлена: %2, пользователь: %3)")
            .arg(version).arg(updatedAt).arg(updatedBy);
    }

    return "Информация о версии не найдена";
}

bool DatabaseManager::checkDatabaseVersion()
{
    if (!m_db.isOpen()) {
        qDebug() << "❌ checkDatabaseVersion: БД не открыта";
        return false;
    }

    logOperation("checkDatabaseVersion", "Начало проверки версии");

    if (!isVersionTableExists()) {
        qDebug() << "📦 Таблица version не найдена, создаем...";
        createVersionTable();
        logOperation("checkDatabaseVersion", "Создана таблица version с версией " + QString::number(DATABASE_VERSION));
        return true;
    }

    int dbVersion = getDatabaseVersion();

    if (dbVersion == -1) {
        qDebug() << "❌ Не удалось определить версию БД";
        return false;
    }

    qDebug() << "📊 Версия БД:" << dbVersion << ", версия программы:" << DATABASE_VERSION;

    if (dbVersion == DATABASE_VERSION) {
        qDebug() << "✅ Версия БД совпадает с версией программы";
        return true;
    }

    if (dbVersion > DATABASE_VERSION) {
        qDebug() << "⚠️ Версия БД (" << dbVersion << ") новее версии программы (" << DATABASE_VERSION << ")";
        logOperation("checkDatabaseVersion", "БД новее программы");
        return false;
    }

    QString errorMessage;
    if (upgradeDatabase(dbVersion, DATABASE_VERSION, errorMessage)) {
        qDebug() << "✅ БД обновлена с версии" << dbVersion << "до" << DATABASE_VERSION;
        logOperation("checkDatabaseVersion", "Обновление успешно: " + errorMessage);
        return true;
    } else {
        qDebug() << "❌ Ошибка обновления БД:" << errorMessage;
        logOperation("checkDatabaseVersion", "Ошибка обновления: " + errorMessage);
        return false;
    }
}

bool DatabaseManager::upgradeDatabase(int fromVersion, int toVersion, QString& errorMessage)
{
    if (fromVersion >= toVersion) {
        errorMessage = "Обновление не требуется";
        return true;
    }

    qDebug() << "📦 Обновление БД с версии" << fromVersion << "до" << toVersion;
    logOperation("upgradeDatabase", QString("Обновление с %1 до %2").arg(fromVersion).arg(toVersion));

    QSqlQuery query(m_db);

    if (!query.exec("BEGIN TRANSACTION;")) {
        errorMessage = "Не удалось начать транзакцию: " + query.lastError().text();
        logError("upgradeDatabase", errorMessage);
        return false;
    }

    try {
        if (fromVersion < 2 && toVersion >= 2) {
            qDebug() << "📦 Применяем обновление до версии 2...";

            QSqlQuery alterQuery(m_db);
            if (!alterQuery.exec("ALTER TABLE folders ADD COLUMN created_by TEXT DEFAULT ''")) {
                errorMessage = "Ошибка добавления поля created_by в folders: " + alterQuery.lastError().text();
                query.exec("ROLLBACK;");
                logError("upgradeDatabase", errorMessage);
                return false;
            }

            if (!alterQuery.exec("ALTER TABLE folders ADD COLUMN modified_by TEXT DEFAULT ''")) {
                errorMessage = "Ошибка добавления поля modified_by в folders: " + alterQuery.lastError().text();
                query.exec("ROLLBACK;");
                logError("upgradeDatabase", errorMessage);
                return false;
            }

            if (!alterQuery.exec("ALTER TABLE documents ADD COLUMN created_by TEXT DEFAULT ''")) {
                errorMessage = "Ошибка добавления поля created_by в documents: " + alterQuery.lastError().text();
                query.exec("ROLLBACK;");
                logError("upgradeDatabase", errorMessage);
                return false;
            }

            if (!alterQuery.exec("ALTER TABLE documents ADD COLUMN modified_by TEXT DEFAULT ''")) {
                errorMessage = "Ошибка добавления поля modified_by в documents: " + alterQuery.lastError().text();
                query.exec("ROLLBACK;");
                logError("upgradeDatabase", errorMessage);
                return false;
            }

            qDebug() << "✅ Обновление до версии 2 применено";
        }

        if (fromVersion < 3 && toVersion >= 3) {
            qDebug() << "📦 Применяем обновление до версии 3...";

            QSqlQuery alterQuery(m_db);
            // Проверяем, есть ли уже поле responsible_user (может быть если это новая БД)
            QSqlQuery checkQuery(m_db);
            bool columnExists = false;

            if (DatabaseAbstraction::isSQLite()) {
                checkQuery.exec("PRAGMA table_info(documents)");
                while (checkQuery.next()) {
                    if (checkQuery.value(1).toString() == "responsible_user") {
                        columnExists = true;
                        break;
                    }
                }
            } else {
                checkQuery.prepare("SELECT column_name FROM information_schema.columns WHERE table_name='documents' AND column_name='responsible_user'");
                columnExists = checkQuery.exec() && checkQuery.next();
            }

            if (!columnExists) {
                if (!alterQuery.exec("ALTER TABLE documents ADD COLUMN responsible_user TEXT DEFAULT ''")) {
                    errorMessage = "Ошибка добавления поля responsible_user в documents: " + alterQuery.lastError().text();
                    query.exec("ROLLBACK;");
                    logError("upgradeDatabase", errorMessage);
                    return false;
                }
                qDebug() << "✅ Добавлено поле responsible_user в documents";
            } else {
                qDebug() << "ℹ️ Поле responsible_user уже существует в documents";
            }

            qDebug() << "✅ Обновление до версии 3 применено";
        }

        if (fromVersion < 4 && toVersion >= 4) {
            qDebug() << "📦 Применяем обновление до версии 4 (добавляем поля в folders)...";

            QSqlQuery alterQuery(m_db);

            // Добавляем status в folders
            QSqlQuery checkQuery(m_db);
            bool statusExists = false, responsibleExists = false;

            if (DatabaseAbstraction::isSQLite()) {
                checkQuery.exec("PRAGMA table_info(folders)");
                while (checkQuery.next()) {
                    QString colName = checkQuery.value(1).toString();
                    if (colName == "status") statusExists = true;
                    if (colName == "responsible_user") responsibleExists = true;
                }
            } else {
                checkQuery.prepare("SELECT column_name FROM information_schema.columns WHERE table_name='folders'");
                checkQuery.exec();
                while (checkQuery.next()) {
                    QString colName = checkQuery.value(0).toString();
                    if (colName == "status") statusExists = true;
                    if (colName == "responsible_user") responsibleExists = true;
                }
            }

            if (!statusExists) {
                if (!alterQuery.exec("ALTER TABLE folders ADD COLUMN status TEXT DEFAULT 'В работе'")) {
                    qDebug() << "⚠️ Ошибка добавления status в folders (возможно уже есть):" << alterQuery.lastError().text();
                }
            }

            if (!responsibleExists) {
                if (!alterQuery.exec("ALTER TABLE folders ADD COLUMN responsible_user TEXT DEFAULT ''")) {
                    qDebug() << "⚠️ Ошибка добавления responsible_user в folders (возможно уже есть):" << alterQuery.lastError().text();
                }
            }

            qDebug() << "✅ Обновление до версии 4 применено";
        }

        if (fromVersion < 5 && toVersion >= 5) {
            qDebug() << "📦 Применяем обновление до версии 5 (истории назначений и статусов)...";

            QSqlQuery alterQuery(m_db);

            // Создаем таблицу assignments (история назначений)
            QString createAssignmentsTable = "CREATE TABLE IF NOT EXISTS assignments ("
                "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "    doc_key TEXT NOT NULL,"
                "    assigned_to TEXT NOT NULL,"
                "    assigned_by TEXT NOT NULL,"
                "    assigned_date DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "    previous_assignee TEXT DEFAULT '',"
                "    comment TEXT DEFAULT ''"
                ");";

            if (!alterQuery.exec(createAssignmentsTable)) {
                qDebug() << "⚠️ Ошибка создания таблицы assignments:" << alterQuery.lastError().text();
            } else {
                qDebug() << "✅ Таблица assignments создана";
            }

            // Создаем таблицу document_status_history (история статусов)
            QString createStatusHistoryTable = "CREATE TABLE IF NOT EXISTS document_status_history ("
                "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "    doc_key TEXT NOT NULL,"
                "    old_status TEXT DEFAULT '',"
                "    new_status TEXT NOT NULL,"
                "    changed_by TEXT NOT NULL,"
                "    changed_date DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "    reason TEXT DEFAULT ''"
                ");";

            if (!alterQuery.exec(createStatusHistoryTable)) {
                qDebug() << "⚠️ Ошибка создания таблицы document_status_history:" << alterQuery.lastError().text();
            } else {
                qDebug() << "✅ Таблица document_status_history создана";
            }

            qDebug() << "✅ Обновление до версии 5 применено";
        }

        if (fromVersion < 7 && toVersion >= 7) {
            qDebug() << "📦 Применяем обновление до версии 7 (архив удалённых документов)...";

            QSqlQuery alterQuery(m_db);

            // Создаем таблицу deleted_documents (архив удалённых документов с их историей)
            QString createDeletedDocsTable = "CREATE TABLE IF NOT EXISTS deleted_documents ("
                "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "    doc_key TEXT NOT NULL,"
                "    path TEXT NOT NULL,"
                "    folder_path TEXT NOT NULL,"
                "    name TEXT NOT NULL,"
                "    designation TEXT DEFAULT '',"
                "    material TEXT DEFAULT '',"
                "    mass TEXT DEFAULT '',"
                "    scale TEXT DEFAULT '',"
                "    format TEXT DEFAULT '',"
                "    status TEXT DEFAULT '',"
                "    comment TEXT DEFAULT '',"
                "    responsible_user TEXT DEFAULT '',"
                "    last_known_size INTEGER DEFAULT 0,"
                "    deleted_date DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "    deleted_by TEXT NOT NULL,"
                "    deletion_reason TEXT DEFAULT ''"
                ");";

            if (!alterQuery.exec(createDeletedDocsTable)) {
                qDebug() << "⚠️ Ошибка создания таблицы deleted_documents:" << alterQuery.lastError().text();
            } else {
                qDebug() << "✅ Таблица deleted_documents создана";
            }

            // Создаем таблицу deleted_assignments (архив назначений удалённых документов)
            QString createDeletedAssignmentsTable = "CREATE TABLE IF NOT EXISTS deleted_assignments ("
                "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "    doc_key TEXT NOT NULL,"
                "    assigned_to TEXT NOT NULL,"
                "    assigned_by TEXT NOT NULL,"
                "    assigned_date DATETIME,"
                "    previous_assignee TEXT DEFAULT '',"
                "    comment TEXT DEFAULT '',"
                "    document_name TEXT NOT NULL"
                ");";

            if (!alterQuery.exec(createDeletedAssignmentsTable)) {
                qDebug() << "⚠️ Ошибка создания таблицы deleted_assignments:" << alterQuery.lastError().text();
            } else {
                qDebug() << "✅ Таблица deleted_assignments создана";
            }

            // Создаем таблицу deleted_status_history (архив истории статусов удалённых документов)
            QString createDeletedStatusHistoryTable = "CREATE TABLE IF NOT EXISTS deleted_status_history ("
                "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "    doc_key TEXT NOT NULL,"
                "    old_status TEXT DEFAULT '',"
                "    new_status TEXT NOT NULL,"
                "    changed_by TEXT NOT NULL,"
                "    changed_date DATETIME,"
                "    reason TEXT DEFAULT '',"
                "    document_name TEXT NOT NULL"
                ");";

            if (!alterQuery.exec(createDeletedStatusHistoryTable)) {
                qDebug() << "⚠️ Ошибка создания таблицы deleted_status_history:" << alterQuery.lastError().text();
            } else {
                qDebug() << "✅ Таблица deleted_status_history создана";
            }

            // ===== НОВОЕ (v7): Добавляем поле doc_key в таблицу comments для надёжной привязки =====
            QSqlQuery checkCommentsQuery(m_db);
            bool docKeyExists = false;

            if (DatabaseAbstraction::isSQLite()) {
                checkCommentsQuery.exec("PRAGMA table_info(comments)");
                while (checkCommentsQuery.next()) {
                    if (checkCommentsQuery.value(1).toString() == "doc_key") {
                        docKeyExists = true;
                        break;
                    }
                }
            } else {
                checkCommentsQuery.prepare("SELECT column_name FROM information_schema.columns WHERE table_name='comments' AND column_name='doc_key'");
                docKeyExists = checkCommentsQuery.exec() && checkCommentsQuery.next();
            }

            if (!docKeyExists) {
                QSqlQuery addDocKeyQuery(m_db);
                if (!addDocKeyQuery.exec("ALTER TABLE comments ADD COLUMN doc_key TEXT DEFAULT ''")) {
                    qDebug() << "⚠️ Ошибка добавления doc_key в comments:" << addDocKeyQuery.lastError().text();
                } else {
                    qDebug() << "✅ Добавлено поле doc_key в таблицу comments";

                    // Заполняем doc_key для существующих комментариев
                    QSqlQuery fillDocKeyQuery(m_db);
                    if (!fillDocKeyQuery.exec("UPDATE comments SET doc_key = folder_path || '|' || file_path WHERE doc_key = ''")) {
                        qDebug() << "⚠️ Ошибка заполнения doc_key:" << fillDocKeyQuery.lastError().text();
                    } else {
                        qDebug() << "✅ Заполнены doc_key для существующих комментариев";
                    }
                }
            }

            qDebug() << "✅ Обновление до версии 7 применено (архив удалённых документов + привязка комментариев)";
        }

        // ===== МИГРАЦИЯ v7 → v8: Добавляем поле is_manual в assignments =====
        if (fromVersion <= 7 && toVersion >= 8) {
            qDebug() << "📦 Применяем обновление до версии 8 (разделение ручных и автоматических назначений)...";

            QSqlQuery alterQuery(m_db);

            // Проверяем наличие поля is_manual в таблице assignments
            bool isManualExists = false;

            if (DatabaseAbstraction::isSQLite()) {
                QSqlQuery checkQuery(m_db);
                checkQuery.exec("PRAGMA table_info(assignments)");
                while (checkQuery.next()) {
                    if (checkQuery.value(1).toString() == "is_manual") {
                        isManualExists = true;
                        break;
                    }
                }
            } else {
                QSqlQuery checkQuery(m_db);
                checkQuery.prepare("SELECT column_name FROM information_schema.columns WHERE table_name='assignments' AND column_name='is_manual'");
                isManualExists = checkQuery.exec() && checkQuery.next();
            }

            if (!isManualExists) {
                QSqlQuery addIsManualQuery(m_db);
                if (!addIsManualQuery.exec("ALTER TABLE assignments ADD COLUMN is_manual BOOLEAN DEFAULT 1")) {
                    qDebug() << "⚠️ Ошибка добавления is_manual в assignments:" << addIsManualQuery.lastError().text();
                } else {
                    qDebug() << "✅ Добавлено поле is_manual в таблицу assignments";
                }
            }

            // Проверяем наличие поля is_manual в таблице deleted_assignments
            bool isManualExistsDeleted = false;

            if (DatabaseAbstraction::isSQLite()) {
                QSqlQuery checkQuery(m_db);
                checkQuery.exec("PRAGMA table_info(deleted_assignments)");
                while (checkQuery.next()) {
                    if (checkQuery.value(1).toString() == "is_manual") {
                        isManualExistsDeleted = true;
                        break;
                    }
                }
            } else {
                QSqlQuery checkQuery(m_db);
                checkQuery.prepare("SELECT column_name FROM information_schema.columns WHERE table_name='deleted_assignments' AND column_name='is_manual'");
                isManualExistsDeleted = checkQuery.exec() && checkQuery.next();
            }

            if (!isManualExistsDeleted) {
                QSqlQuery addIsManualQueryDeleted(m_db);
                if (!addIsManualQueryDeleted.exec("ALTER TABLE deleted_assignments ADD COLUMN is_manual BOOLEAN DEFAULT 1")) {
                    qDebug() << "⚠️ Ошибка добавления is_manual в deleted_assignments:" << addIsManualQueryDeleted.lastError().text();
                } else {
                    qDebug() << "✅ Добавлено поле is_manual в таблицу deleted_assignments";
                }
            }

            qDebug() << "✅ Обновление до версии 8 применено (разделение ручных и автоматических назначений)";
        }

        if (fromVersion < 9 && toVersion >= 9) {
            qDebug() << "📦 Применяем обновление до версии 9 (синхронизация сессий пользователей)...";

            QSqlQuery alterQuery(m_db);

            // Создаем таблицу user_sessions для привязки пользователя к сессии
            QString createSessionsTable = "CREATE TABLE IF NOT EXISTS user_sessions ("
                "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "    user_fio TEXT NOT NULL,"
                "    computer_name TEXT NOT NULL,"
                "    last_login DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "    is_active BOOLEAN DEFAULT 1,"
                "    UNIQUE(user_fio, computer_name)"
                ");";

            if (!alterQuery.exec(createSessionsTable)) {
                qDebug() << "⚠️ Ошибка создания таблицы user_sessions:" << alterQuery.lastError().text();
            } else {
                qDebug() << "✅ Таблица user_sessions создана";
            }

            qDebug() << "✅ Обновление до версии 9 применено (синхронизация сессий пользователей)";
        }

        QSqlQuery updateQuery(m_db);
        updateQuery.prepare("UPDATE version SET version = :version, updated_at = CURRENT_TIMESTAMP, updated_by = :user");
        updateQuery.bindValue(":version", toVersion);
        updateQuery.bindValue(":user", qgetenv("USERNAME"));

        if (!updateQuery.exec()) {
            errorMessage = "Ошибка обновления версии: " + updateQuery.lastError().text();
            query.exec("ROLLBACK;");
            logError("upgradeDatabase", errorMessage);
            return false;
        }

        if (!query.exec("COMMIT;")) {
            errorMessage = "Ошибка COMMIT: " + query.lastError().text();
            query.exec("ROLLBACK;");
            logError("upgradeDatabase", errorMessage);
            return false;
        }

        errorMessage = "БД успешно обновлена до версии " + QString::number(toVersion);
        return true;

    } catch (const std::exception& e) {
        errorMessage = "Исключение при обновлении: " + QString(e.what());
        query.exec("ROLLBACK;");
        logError("upgradeDatabase", errorMessage);
        return false;
    }
}

// ============================================================
// ПРОВЕРКА ЦЕЛОСТНОСТИ
// ============================================================

bool DatabaseManager::checkDatabaseIntegrity(QStringList& errors)
{
    errors.clear();
    bool valid = true;

    if (!m_db.isOpen()) {
        errors << "База данных не открыта";
        return false;
    }

    QSqlQuery query(m_db);

    // ===== ИСПРАВЛЕНИЕ: используем DatabaseAbstraction для PRAGMA (только для SQLite) =====
    if (DatabaseAbstraction::isSQLite()) {
        if (query.exec("PRAGMA integrity_check;")) {
            if (query.next()) {
                QString result = query.value(0).toString();
                if (result != "ok") {
                    errors << QString("Ошибка целостности SQLite: %1").arg(result);
                    valid = false;
                }
            }
        } else {
            errors << "Не удалось выполнить integrity_check: " + query.lastError().text();
            valid = false;
        }

        if (query.exec("PRAGMA foreign_key_check;")) {
            while (query.next()) {
                QString table = query.value(0).toString();
                QString rowid = query.value(1).toString();
                QString parent = query.value(2).toString();
                errors << QString("Нарушение внешнего ключа: таблица '%1', rowid=%2, ссылается на '%3'")
                              .arg(table).arg(rowid).arg(parent);
                valid = false;
            }
        }
    } else if (DatabaseAbstraction::isPostgreSQL()) {
        // PostgreSQL: проверяем консистентность через простой SELECT
        if (!query.exec("SELECT 1;")) {
            errors << "PostgreSQL БД недоступна или повреждена";
            valid = false;
        }
    }

    // ===== ПРОВЕРКА ДОКУМЕНТОВ =====
    for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
        const DocInfo& doc = it.value();
        QString key = it.key();

        // Извлекаем folderPath из ключа
        int pipePos = key.indexOf('|');
        QString folderPath = (pipePos != -1) ? key.left(pipePos) : "";

        if (doc.path.isEmpty()) {
            errors << "Обнаружен документ с пустым путем";
            valid = false;
            continue;
        }

        // Проверяем, существует ли папка
        if (!folderPath.isEmpty() && !m_folders.contains(folderPath)) {
            errors << QString("Документ '%1' ссылается на несуществующую папку '%2'")
                          .arg(doc.path).arg(folderPath);
            valid = false;
        }
    }

    // ===== ПРОВЕРКА ЦИКЛИЧЕСКИХ ССЫЛОК В ПАПКАХ =====
    QMap<QString, bool> visited;
    for (auto it = m_folders.begin(); it != m_folders.end(); ++it) {
        if (it.key().isEmpty()) continue;
        visited.clear();
        if (hasCircularReference(it.key(), visited)) {
            errors << QString("Обнаружена циклическая ссылка в папке '%1'").arg(it.key());
            valid = false;
        }
    }

    // ===== ПРОВЕРКА ПУСТЫХ ИМЕН =====
    for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
        if (it.value().name.isEmpty()) {
            errors << QString("Документ '%1' имеет пустое имя").arg(it.key());
            valid = false;
        }
    }

    for (auto it = m_folders.begin(); it != m_folders.end(); ++it) {
        if (!it.key().isEmpty() && it.value().name.isEmpty()) {
            errors << QString("Папка '%1' имеет пустое имя").arg(it.key());
            valid = false;
        }
    }

    return valid;
}

bool DatabaseManager::hasCircularReference(const QString& path, QMap<QString, bool>& visited)
{
    if (visited.contains(path)) {
        return true;
    }
    visited[path] = true;

    int lastSlash = path.lastIndexOf('/');
    QString parentPath = (lastSlash != -1) ? path.left(lastSlash) : "";

    if (!parentPath.isEmpty() && m_folders.contains(parentPath)) {
        return hasCircularReference(parentPath, visited);
    }

    return false;
}

// ============================================================
// ВОССТАНОВЛЕНИЕ
// ============================================================

bool DatabaseManager::repairDatabase(QString& errorMessage)
{
    logOperation("repairDatabase", "Начало восстановления БД");

    QStringList errors;
    if (checkDatabaseIntegrity(errors)) {
        errorMessage = "База данных цела, восстановление не требуется";
        logOperation("repairDatabase", "БД цела");
        return true;
    }

    qDebug() << "⚠️ Обнаружены ошибки в БД:" << errors.join(", ");

    QSqlQuery query(m_db);
    if (query.exec("VACUUM;")) {
        errors.clear();
        if (checkDatabaseIntegrity(errors)) {
            errorMessage = "База данных восстановлена через VACUUM";
            logOperation("repairDatabase", "Успешно восстановлена через VACUUM");
            return true;
        }
        qDebug() << "⚠️ VACUUM не помог, ошибки:" << errors.join(", ");
    }

    if (query.exec("REINDEX;")) {
        errors.clear();
        if (checkDatabaseIntegrity(errors)) {
            errorMessage = "База данных восстановлена через REINDEX";
            logOperation("repairDatabase", "Успешно восстановлена через REINDEX");
            return true;
        }
        qDebug() << "⚠️ REINDEX не помог, ошибки:" << errors.join(", ");
    }

    QString backupDir = QCoreApplication::applicationDirPath() + "/backups";
    QDir dir(backupDir);

    if (!dir.exists()) {
        errorMessage = "Папка бэкапов не найдена";
        logOperation("repairDatabase", "Папка бэкапов не найдена: " + backupDir);
        return false;
    }

    QStringList backupFolders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

    for (const QString& folder : backupFolders) {
        QDate backupDate = QDate::fromString(folder, "yyyy-MM-dd");
        if (!backupDate.isValid()) continue;

        QString folderPath = backupDir + "/" + folder;
        QDir backupDir2(folderPath);

        QStringList backupFiles = backupDir2.entryList(QStringList() << "kd_db_*.dat", QDir::Files, QDir::Name | QDir::Reversed);

        for (const QString& backupFile : backupFiles) {
            QString fullBackupPath = folderPath + "/" + backupFile;
            qDebug() << "🔍 Пробуем восстановить из:" << fullBackupPath;

            if (restoreFromBackup(fullBackupPath, errorMessage)) {
                errorMessage = "База данных восстановлена из бэкапа: " + fullBackupPath;
                logOperation("repairDatabase", "Успешно восстановлена из бэкапа: " + fullBackupPath);
                return true;
            }
        }
    }

    errorMessage = "Не удалось восстановить базу данных. Ошибки: " + errors.join("; ");
    logOperation("repairDatabase", "Ошибка восстановления: " + errorMessage);
    return false;
}

bool DatabaseManager::restoreFromBackup(const QString& backupPath, QString& errorMessage)
{
    if (!QFile::exists(backupPath)) {
        errorMessage = "Файл бэкапа не найден: " + backupPath;
        return false;
    }

    logOperation("restoreFromBackup", "Восстановление из: " + backupPath);

    if (m_db.isOpen()) {
        m_db.close();
    }

    QString damagedPath = m_dbPath + ".damaged." + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    if (QFile::exists(m_dbPath)) {
        QFile::copy(m_dbPath, damagedPath);
        qDebug() << "💾 Поврежденная БД сохранена как:" << damagedPath;
    }

    if (!QFile::copy(backupPath, m_dbPath)) {
        if (!QFile::rename(backupPath, m_dbPath)) {
            errorMessage = "Не удалось скопировать бэкап: " + backupPath;
            logOperation("restoreFromBackup", "Ошибка копирования: " + errorMessage);
            return false;
        }
    }

    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open()) {
        errorMessage = "Не удалось открыть восстановленную БД: " + m_db.lastError().text();
        logOperation("restoreFromBackup", "Ошибка открытия: " + errorMessage);
        return false;
    }

    QStringList errors;
    if (!checkDatabaseIntegrity(errors)) {
        errorMessage = "Восстановленная БД содержит ошибки: " + errors.join("; ");
        logOperation("restoreFromBackup", "Ошибка целостности: " + errorMessage);
        return false;
    }

    loadDatabase();

    m_lastBackupPath = backupPath;
    logOperation("restoreFromBackup", "Восстановление успешно завершено");

    return true;
}

// ============================================================
// КОНКУРЕНТНЫЙ ДОСТУП
// ============================================================

bool DatabaseManager::isNetworkDatabase() const
{
    QString path = QDir::toNativeSeparators(m_dbPath);

    if (path.startsWith("//") || path.startsWith("\\\\")) {
        return true;
    }

    QStringList networkDrives = {"N:", "Z:", "X:", "Y:"};
    for (const QString& drive : networkDrives) {
        if (path.startsWith(drive + "/") || path.startsWith(drive + "\\")) {
            return true;
        }
    }

#ifdef Q_OS_WIN
    if (path.contains("ЛИЧНЫЕ ПАПКИ") || path.contains("Server") || path.contains("SERVER")) {
        return true;
    }
#endif

    return false;
}

bool DatabaseManager::setLockingMode(LockMode mode)
{
    if (!m_db.isOpen()) {
        qDebug() << "❌ setLockingMode: БД не открыта";
        return false;
    }

    QString modeStr;
    switch (mode) {
    case LOCK_MODE_NORMAL:
        modeStr = "NORMAL";
        break;
    case LOCK_MODE_EXCLUSIVE:
        modeStr = "EXCLUSIVE";
        break;
    default:
        modeStr = "NORMAL";
        break;
    }

    // ===== ИСПРАВЛЕНИЕ: PRAGMA locking_mode работает только для SQLite =====
    if (DatabaseAbstraction::isSQLite()) {
        QString sql = "PRAGMA locking_mode = " + modeStr + ";";
        if (!executeQuery(sql)) {
            qDebug() << "❌ Не удалось установить режим блокировки:" << modeStr;
            return false;
        }
    } else if (DatabaseAbstraction::isPostgreSQL()) {
        // PostgreSQL управляет блокировками автоматически через транзакции
        qDebug() << "ℹ️ PostgreSQL: режим блокировки управляется автоматически через транзакции";
    }

    qDebug() << "🔒 Режим блокировки установлен:" << modeStr;
    return true;
}

bool DatabaseManager::isDatabaseLocked() const
{
    if (m_dbPath.isEmpty() || !QFile::exists(m_dbPath)) {
        return false;
    }

#ifdef Q_OS_WIN
    HANDLE hFile = CreateFileW(
        m_dbPath.toStdWString().c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,  // ✅ ПОЗВОЛЯЕМ ВСЕМ ЧИТАТЬ И ПИСАТЬ
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
        );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) {
            qDebug() << "🔒 Файл БД заблокирован (ошибка:" << error << ")";
            return true;
        }
        return false;
    }

    CloseHandle(hFile);
    return false;
#else
    QFile file(m_dbPath);
    if (!file.open(QIODevice::ReadWrite)) {
        if (file.error() == QFile::OpenError) {
            qDebug() << "🔒 Файл БД заблокирован";
            return true;
        }
        return false;
    }
    file.close();
    return false;
#endif
}

bool DatabaseManager::waitForDatabaseUnlock(int maxWaitMs)
{
    qDebug() << "⏳ Ожидание разблокировки БД (макс:" << maxWaitMs << "мс)";

    int elapsed = 0;
    int checkInterval = 200;

    while (elapsed < maxWaitMs) {
        if (!isDatabaseLocked()) {
            qDebug() << "✅ БД разблокирована через" << elapsed << "мс";
            return true;
        }

        QThread::msleep(checkInterval);
        elapsed += checkInterval;
    }

    qDebug() << "❌ Таймаут ожидания разблокировки БД (" << maxWaitMs << "мс)";
    return false;
}

DatabaseManager::FileAccessResult DatabaseManager::checkDatabaseAccess()
{
    if (m_dbPath.isEmpty()) {
        return ACCESS_ERROR;
    }

    if (!QFile::exists(m_dbPath)) {
        return ACCESS_NOT_EXISTS;
    }

#ifdef Q_OS_WIN
    HANDLE hFile = CreateFileW(
        m_dbPath.toStdWString().c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
        );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) {
            qDebug() << "🔒 Файл БД заблокирован другим процессом";
            return ACCESS_LOCKED;
        }
        qDebug() << "❌ Ошибка доступа к БД:" << error;
        return ACCESS_ERROR;
    }

    DWORD bytesRead;
    char buffer[1024];
    if (!ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL)) {
        CloseHandle(hFile);
        qDebug() << "❌ Не удается прочитать БД";
        return ACCESS_ERROR;
    }

    CloseHandle(hFile);
    return ACCESS_OK;
#else
    QFile file(m_dbPath);
    if (!file.open(QIODevice::ReadWrite)) {
        if (file.error() == QFile::OpenError) {
            return ACCESS_LOCKED;
        }
        return ACCESS_ERROR;
    }
    file.close();
    return ACCESS_OK;
#endif
}

bool DatabaseManager::isDatabaseAccessible()
{
    FileAccessResult result = checkDatabaseAccess();
    return result == ACCESS_OK;
}

bool DatabaseManager::acquireDatabaseLock(int timeoutMs)
{
    QMutexLocker locker(&m_lockMutex);

    if (m_hasLock) {
        return true;
    }

    qDebug() << "🔒 Попытка захвата блокировки БД (таймаут:" << timeoutMs << "мс)";

    int elapsed = 0;
    int checkInterval = 100;

    while (elapsed < timeoutMs) {
        FileAccessResult result = checkDatabaseAccess();

        if (result == ACCESS_OK) {
            m_hasLock = true;
            qDebug() << "✅ Блокировка БД захвачена через" << elapsed << "мс";
            return true;
        }

        if (result == ACCESS_NOT_EXISTS) {
            m_hasLock = true;
            qDebug() << "✅ Блокировка БД захвачена (файл создан)";
            return true;
        }

        if (result == ACCESS_ERROR) {
            qDebug() << "❌ Ошибка доступа к БД";
            return false;
        }

        QThread::msleep(checkInterval);
        elapsed += checkInterval;
    }

    qDebug() << "❌ Таймаут захвата блокировки БД";
    return false;
}

void DatabaseManager::releaseDatabaseLock()
{
    QMutexLocker locker(&m_lockMutex);

    if (!m_hasLock) {
        return;
    }

    m_hasLock = false;
    qDebug() << "🔓 Блокировка БД освобождена";
}

bool DatabaseManager::isDatabaseCorrupted()
{
    if (!m_db.isOpen()) {
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec("PRAGMA integrity_check;")) {
        qDebug() << "❌ Не удалось выполнить integrity_check:" << query.lastError().text();
        return true;
    }

    if (query.next()) {
        QString result = query.value(0).toString();
        bool corrupted = (result != "ok");
        if (corrupted) {
            qDebug() << "⚠️ БД повреждена:" << result;
        }
        return corrupted;
    }

    return false;
}

// ============================================================
// ИНДЕКСЫ
// ============================================================

void DatabaseManager::createAllIndexes()
{
    qDebug() << "📊 Создание индексов для быстрого поиска...";

    QStringList indexQueries = {
        "CREATE INDEX IF NOT EXISTS idx_folders_name ON folders(name);",
        "CREATE INDEX IF NOT EXISTS idx_folders_parent ON folders(parent_path);",
        "CREATE INDEX IF NOT EXISTS idx_folders_type ON folders(type);",
        "CREATE INDEX IF NOT EXISTS idx_folders_path ON folders(path);",
        "CREATE INDEX IF NOT EXISTS idx_folders_parent_type ON folders(parent_path, type);",
        "CREATE INDEX IF NOT EXISTS idx_folders_name_parent ON folders(name, parent_path);",

        "CREATE INDEX IF NOT EXISTS idx_documents_name ON documents(name);",
        "CREATE INDEX IF NOT EXISTS idx_documents_folder ON documents(folder_path);",
        "CREATE INDEX IF NOT EXISTS idx_documents_status ON documents(status);",
        "CREATE INDEX IF NOT EXISTS idx_documents_modified ON documents(modified);",
        "CREATE INDEX IF NOT EXISTS idx_documents_path ON documents(path);",
        "CREATE INDEX IF NOT EXISTS idx_documents_folder_name ON documents(folder_path, name);",
        "CREATE INDEX IF NOT EXISTS idx_documents_status_folder ON documents(status, folder_path);",
        "CREATE INDEX IF NOT EXISTS idx_documents_modified_folder ON documents(modified, folder_path);",
        "CREATE INDEX IF NOT EXISTS idx_documents_comment ON documents(comment);",
        "CREATE INDEX IF NOT EXISTS idx_documents_designation ON documents(designation);",
        "CREATE INDEX IF NOT EXISTS idx_documents_material ON documents(material);",

        "CREATE INDEX IF NOT EXISTS idx_notifications_time ON notifications(time DESC);",
        "CREATE INDEX IF NOT EXISTS idx_notifications_read ON notifications(is_read);",
        "CREATE INDEX IF NOT EXISTS idx_notifications_user ON notifications(source_user);",
        "CREATE INDEX IF NOT EXISTS idx_notifications_type ON notifications(type);",

        "CREATE INDEX IF NOT EXISTS idx_comments_file ON comments(file_path);",
        "CREATE INDEX IF NOT EXISTS idx_comments_folder ON comments(folder_path);",
        "CREATE INDEX IF NOT EXISTS idx_comments_user ON comments(user);",
        "CREATE INDEX IF NOT EXISTS idx_comments_time ON comments(time DESC);",
        "CREATE INDEX IF NOT EXISTS idx_comments_reply ON comments(reply_to);"
    };

    int successCount = 0;
    int errorCount = 0;

    for (const QString& sql : indexQueries) {
        if (executeQuery(sql)) {
            successCount++;
        } else {
            errorCount++;
            qDebug() << "⚠️ Ошибка создания индекса:" << sql;
        }
    }

    qDebug() << "✅ Индексы созданы: успешно =" << successCount << ", ошибок =" << errorCount;
}

// ============================================================
// ОСТАЛЬНЫЕ МЕТОДЫ (СУЩЕСТВУЮЩИЕ)
// ============================================================

void DatabaseManager::createDefaultStructure()
{
    if (!m_db.isOpen()) {
        qDebug() << "❌ База данных не открыта, не могу создать структуру!";
        return;
    }

    qDebug() << "📁 Создаём структуру базы данных...";

    QSqlQuery checkQuery(m_db);
    checkQuery.exec("SELECT COUNT(*) FROM folders");
    if (checkQuery.next() && checkQuery.value(0).toInt() > 0) {
        qDebug() << "⚠️ База данных уже содержит" << checkQuery.value(0).toInt() << "папок, пропускаем создание";
        return;
    }

    // ===== ИСПРАВЛЕНИЕ: используем DatabaseAbstraction вместо INSERT OR IGNORE =====
    QMap<QString, QVariant> rootValues;
    rootValues["path"] = "";
    rootValues["name"] = "";
    rootValues["type"] = static_cast<int>(TYPE_ROOT);
    rootValues["parent_path"] = QVariant();
    rootValues["created_by"] = qgetenv("USERNAME");

    QString rootInsertSql = DatabaseAbstraction::insertOrIgnore("folders", rootValues, QStringList() << "path");

    QSqlQuery insertQuery(m_db);
    if (insertQuery.prepare(rootInsertSql)) {
        for (auto it = rootValues.begin(); it != rootValues.end(); ++it) {
            insertQuery.bindValue(":" + it.key(), it.value());
        }
        if (!insertQuery.exec()) {
            qDebug() << "❌ Ошибка создания корневой папки:" << insertQuery.lastError().text();
        } else {
            qDebug() << "✅ Создана корневая папка";
        }
    }

    QList<QPair<QString, FolderType>> sections = {
        {"Изделия", TYPE_PRODUCT},
        {"Справочники", TYPE_REFERENCE},
        {"Архив", TYPE_ARCHIVE},
        {"Техпроцессы", TYPE_TECH_PROCESS},
        {"Другое", TYPE_OTHER}
    };

    for (const auto& section : sections) {
        QMap<QString, QVariant> sectionValues;
        sectionValues["path"] = section.first;
        sectionValues["name"] = section.first;
        sectionValues["type"] = static_cast<int>(section.second);
        sectionValues["parent_path"] = "";
        sectionValues["created_by"] = qgetenv("USERNAME");

        QString sectionInsertSql = DatabaseAbstraction::insertOrIgnore("folders", sectionValues, QStringList() << "path");

        QSqlQuery insertQuery2(m_db);
        if (insertQuery2.prepare(sectionInsertSql)) {
            for (auto it = sectionValues.begin(); it != sectionValues.end(); ++it) {
                insertQuery2.bindValue(":" + it.key(), it.value());
            }
            if (insertQuery2.exec()) {
                qDebug() << "✅ Создан раздел:" << section.first;
            } else {
                qDebug() << "❌ Ошибка создания раздела:" << section.first << insertQuery2.lastError().text();
            }
        }
    }

    qDebug() << "✅ Структура базы данных создана!";
}

void DatabaseManager::setDatabasePath(const QString& path)
{
    if (m_db.isOpen()) {
        m_db.close();
    }

    m_dbPath = path;
    m_backupPath = m_dbPath;
    m_backupPath = m_backupPath.replace(".db", "_backup.db");

    if (!initDatabase(path)) {
        qDebug() << "❌ Не удалось инициализировать БД по пути:" << path;
        QString appDir = QCoreApplication::applicationDirPath();
        m_dbPath = appDir + "/kd.db";
        m_backupPath = m_dbPath;
        m_backupPath = m_backupPath.replace(".db", "_backup.db");
        initDatabase(m_dbPath);
    }

    loadDatabase();
}

// ===== ИНИЦИАЛИЗАЦИЯ БД (DEPRECATED) =====
bool DatabaseManager::initDatabase(const QString& path)
{
    QSettings settings("KDManager", "Settings");
    settings.setValue("database/sqlite_path", path);
    settings.sync();

    return initSQLite(settings);
}

void DatabaseManager::loadDatabase()
{
    qDebug() << "📂 Загрузка базы данных...";

    clearAllData();
    loadFoldersFromDb();
    loadDocumentsFromDb();

    // ===== ИСПРАВЛЕНИЕ: НЕ УДАЛЯЕМ ДОКУМЕНТЫ-СИРОТЫ =====
    // Теперь документы-сироты остаются в m_docs, но не отображаются в дереве

    // Просто проверяем и выводим информацию
    int orphanCount = 0;
    for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
        const QString& key = it.key();
        int pipePos = key.indexOf('|');
        if (pipePos == -1) continue;

        QString folderPath = key.left(pipePos);
        if (!m_folders.contains(folderPath)) {
            orphanCount++;
        }
    }

    if (orphanCount > 0) {
        qDebug() << "⚠️ В m_docs найдено документов-сирот:" << orphanCount;
        qDebug() << "   💡 Используйте getOrphanFiles() для получения списка";
    }

    qDebug() << "✅ Загружено папок:" << m_folders.size() << ", документов:" << m_docs.size();

    emit databaseChanged();
    m_lastSaveTime = QDateTime::currentDateTime();
    m_dirty = false;
}
void DatabaseManager::clearAllData()
{
    m_folders.clear();
    m_docs.clear();
}

void DatabaseManager::loadFoldersFromDb()
{
    if (!m_db.isOpen()) {
        qDebug() << "❌ loadFoldersFromDb: БД не открыта!";
        return;
    }

    qDebug() << "=== loadFoldersFromDb ===";

    QSqlQuery query = executeQueryWithResult(
        "SELECT path, name, type, parent_path FROM folders ORDER BY path"
        );

    if (query.lastError().isValid()) {
        qDebug() << "❌ Ошибка загрузки папок:" << query.lastError().text();
        return;
    }

    int count = 0;
    while (query.next()) {
        FolderData data;
        QString path = query.value("path").toString();
        data.name = query.value("name").toString();
        data.type = static_cast<FolderType>(query.value("type").toInt());
        data.files.clear();

        m_folders[path] = data;
        count++;
        qDebug() << "   Загружена папка:" << path << "имя:" << data.name << "тип:" << data.type;
    }

    qDebug() << "✅ Загружено папок из БД:" << count;

    // ============================================================
    // ЗАГРУЗКА ФАЙЛОВ ДЛЯ КАЖДОЙ ПАПКИ
    // ============================================================
    int fileCount = 0;
    int missingCount = 0;

    qDebug() << "=== НАЧАЛО ЗАГРУЗКИ ФАЙЛОВ ===";

    QSqlQuery countQuery(m_db);
    countQuery.exec("SELECT COUNT(*) FROM documents");
    if (countQuery.next()) {
        int totalDocs = countQuery.value(0).toInt();
        qDebug() << "📊 Всего записей в таблице documents:" << totalDocs;
    }

    // Загружаем файлы для каждой папки
    for (auto it = m_folders.begin(); it != m_folders.end(); ++it) {
        if (it.key().isEmpty()) continue;

        QString folderPath = it.key();
        qDebug() << "🔍 Загружаем файлы для папки:" << folderPath;

        QSqlQuery fileQuery = executeQueryWithResult(
            "SELECT path, name, responsible_user, status FROM documents WHERE folder_path = :folder_path",
            {{"folder_path", folderPath}}
            );

        if (fileQuery.lastError().isValid()) {
            qDebug() << "   ❌ Ошибка запроса:" << fileQuery.lastError().text();
            continue;
        }

        int filesInFolder = 0;
        while (fileQuery.next()) {
            QString filePath = fileQuery.value("path").toString();
            QString fileName = fileQuery.value("name").toString();
            qDebug() << "   📄 Найден файл в БД: path=" << filePath << "name=" << fileName;

            // Проверяем существование файла на диске
            if (QFile::exists(filePath)) {
                it.value().files.append(filePath);
                fileCount++;
                filesInFolder++;
                qDebug() << "      ✅ Файл существует на диске";
            } else {
                missingCount++;
                qDebug() << "      ⚠️ Файл НЕ СУЩЕСТВУЕТ на диске (будет помечен)";
                // ===== НЕ УДАЛЯЕМ! Просто добавляем в список =====
                it.value().files.append(filePath);
                fileCount++;
                filesInFolder++;
            }
        }
        qDebug() << "   📁 В папке" << folderPath << "загружено файлов:" << filesInFolder;
    }

    qDebug() << "✅ Загружено файлов из БД:" << fileCount;
    if (missingCount > 0) {
        qDebug() << "⚠️ Отсутствующих файлов (помечены):" << missingCount;
    }

    // ============================================================
    // ===== НЕ УДАЛЯЕМ ФАЙЛЫ-СИРОТЫ! ТОЛЬКО ПРЕДУПРЕЖДАЕМ =====
    // ============================================================
    QSqlQuery orphanQuery(m_db);
    orphanQuery.exec(
        "SELECT path, folder_path FROM documents d "
        "WHERE NOT EXISTS (SELECT 1 FROM folders f WHERE f.path = d.folder_path)"
        );

    int orphanCount = 0;
    while (orphanQuery.next()) {
        orphanCount++;
        QString filePath = orphanQuery.value(0).toString();
        QString folderPath = orphanQuery.value(1).toString();
        qDebug() << "⚠️ Файл-сирота (нет папки):" << filePath << "folder_path:" << folderPath;
    }

    if (orphanCount > 0) {
        qDebug() << "⚠️ Найдено файлов-сирот:" << orphanCount;
        qDebug() << "   💡 Файлы-сироты сохранены, но не отображаются в дереве";
        qDebug() << "   💡 Используйте getOrphanFiles() и adoptOrphanFile() для восстановления";
    } else {
        qDebug() << "✅ Все файлы привязаны к папкам";
    }

    // ===== НИЧЕГО НЕ УДАЛЯЕМ! =====
    // Удален код, который удалял файлы-сироты из БД

    qDebug() << "=== КОНЕЦ loadFoldersFromDb ===";
}

void DatabaseManager::loadDocumentsFromDb()
{
    qDebug() << "=== loadDocumentsFromDb ===";

    if (!m_db.isOpen()) {
        qDebug() << "❌ loadDocumentsFromDb: БД не открыта!";
        return;
    }

    // ===== СНАЧАЛА ПОСЧИТАЙ ЗАПИСИ =====
    QSqlQuery countQuery(m_db);
    countQuery.exec("SELECT COUNT(*) FROM documents");
    if (countQuery.next()) {
        qDebug() << "📊 Всего записей в таблице documents:" << countQuery.value(0).toInt();
    }

    // ===== ВЫВЕДИ ВСЕ ЗАПИСИ ДЛЯ ОТЛАДКИ =====
    QSqlQuery debugQuery(m_db);
    debugQuery.exec("SELECT folder_path, path FROM documents ORDER BY folder_path");
    qDebug() << "=== ВСЕ ЗАПИСИ В documents ===";
    int debugCount = 0;
    while (debugQuery.next()) {
        debugCount++;
        qDebug() << "  " << debugCount << ": folder_path:" << debugQuery.value(0).toString()
                 << "path:" << debugQuery.value(1).toString();
    }
    qDebug() << "=== ИТОГО ЗАПИСЕЙ:" << debugCount << " ===";

    // ===== ОСНОВНОЙ ЗАПРОС =====
    QSqlQuery query = executeQueryWithResult(
        "SELECT path, name, folder_path, designation, material, mass, scale, "
        "format, status, comment, size, modified FROM documents"
        );

    if (query.lastError().isValid()) {
        qDebug() << "❌ Ошибка загрузки документов:" << query.lastError().text();
        return;
    }

    int count = 0;
    while (query.next()) {
        DocInfo doc;
        QString filePath = query.value("path").toString();
        QString folderPath = query.value("folder_path").toString();

        doc.path = filePath;
        doc.folderPath = folderPath;  // НОВОЕ: добавляем folderPath
        doc.name = query.value("name").toString();
        doc.designation = query.value("designation").toString();
        doc.material = query.value("material").toString();
        doc.mass = query.value("mass").toString();
        doc.scale = query.value("scale").toString();
        doc.format = query.value("format").toString();
        doc.status = query.value("status").toString();
        doc.comment = query.value("comment").toString();
        doc.responsibleUser = query.value("responsible_user").toString();  // НОВОЕ: загружаем ответственного
        doc.size = query.value("size").toLongLong();
        doc.modified = query.value("modified").toDateTime();

        // КЛЮЧ: folderPath + "|" + filePath
        QString docKey = folderPath + "|" + filePath;

        // ===== ВАЖНО: ДОБАВЛЯЕМ ВСЕ ДОКУМЕНТЫ, ДАЖЕ СИРОТЫ =====
        m_docs[docKey] = doc;
        count++;

        qDebug() << "   📄 Загружен документ: folder_path=" << folderPath
                 << ", path=" << filePath << ", name=" << doc.name;
    }

    qDebug() << "✅ Загружено документов:" << count;

    // ===== ПРОВЕРКА: ВСЕ ЛИ ЗАПИСИ ЗАГРУЗИЛИСЬ =====
    if (count != debugCount) {
        qDebug() << "⚠️ НЕСООТВЕТСТВИЕ: загружено" << count << ", а в БД" << debugCount;
    }

    // ===== НИЧЕГО НЕ УДАЛЯЕМ! =====
    // Удален код, который удалял документы-сироты из m_docs
}

void DatabaseManager::saveDatabase()
{
    // ===== КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Минимизируем время захвата мьютекса =====
    // Копируем данные ЭТО захватывая мьютекс, потом отпускаем его
    QMap<QString, FolderData> foldersCopy;
    QMap<QString, DocInfo> docsCopy;

    {
        QMutexLocker locker(&m_mutex);

        if (!m_dirty) {
            qDebug() << "ℹ️ saveDatabase: Нет изменений для сохранения";
            emit saveFinished(true);
            return;
        }

        // Копируем данные с захватом мьютекса
        foldersCopy = m_folders;
        docsCopy = m_docs;
    }
    // <-- мьютекс ОТПУЩЕН здесь

    if (!m_db.isOpen()) {
        qDebug() << "❌ saveDatabase: База данных не открыта!";
        emit saveFinished(false);
        return;
    }

    if (!acquireDatabaseLock(3000)) {
        qDebug() << "❌ saveDatabase: Не удалось захватить блокировку БД";
        emit saveFinished(false);
        return;
    }

    QElapsedTimer timer;
    timer.start();

    qDebug() << "💾 Начинаем сохранение базы данных...";
    qDebug() << "   📊 Папок:" << foldersCopy.size() << ", Документов:" << docsCopy.size();

    // ===== ИСПОЛЬЗУЕМ КОПИИ ВМЕСТО m_folders и m_docs =====
    // Теперь можно читать из UI потока без deadlock

    QSqlQuery query(m_db);
    if (!query.exec("BEGIN TRANSACTION;")) {
        qDebug() << "❌ saveDatabase: Не удалось начать транзакцию:" << query.lastError().text();
        releaseDatabaseLock();
        emit saveFinished(false);
        return;
    }

    // ===== КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: НЕ УДАЛЯЕМ ВСЕ ЗАПИСИ =====
    // Вместо DELETE всех + INSERT - используем UPSERT для безопасности
    // Это гарантирует, что данные других папок не будут потеряны

    // НЕ удаляем старые записи! Вместо этого используем INSERT OR REPLACE
    // DELETE FROM documents;  // ← УДАЛЕНО! Опасно!
    // DELETE FROM folders;    // ← УДАЛЕНО! Опасно!

    bool hasError = false;
    int savedFolders = 0;
    int savedDocuments = 0;

    // ===== СОХРАНЯЕМ ПАПКИ =====
    qDebug() << "📁 Сохраняем папки...";

    for (auto it = foldersCopy.begin(); it != foldersCopy.end() && !hasError; ++it) {
        const QString& path = it.key();
        const FolderData& data = it.value();

        if (path.isEmpty() && data.type != TYPE_ROOT) {
            qDebug() << "⚠️ saveDatabase: Пропуск папки с пустым путем (тип:" << data.type << ")";
            continue;
        }

        QSqlQuery insertQuery(m_db);
        // ===== ИСПОЛЬЗУЕМ INSERT OR REPLACE ВМЕСТО DELETE + INSERT =====
        insertQuery.prepare(
            "INSERT OR REPLACE INTO folders (path, name, type, parent_path, updated_at, modified_by) "
            "VALUES (:path, :name, :type, :parent_path, CURRENT_TIMESTAMP, :modified_by)"
            );

        insertQuery.bindValue(":path", path);
        insertQuery.bindValue(":name", data.name);
        insertQuery.bindValue(":type", static_cast<int>(data.type));
        insertQuery.bindValue(":modified_by", qgetenv("USERNAME"));

        QString parentPath;
        if (path.isEmpty()) {
            parentPath = "";
        } else {
            int lastSlash = path.lastIndexOf('/');
            parentPath = (lastSlash != -1) ? path.left(lastSlash) : "";
        }
        insertQuery.bindValue(":parent_path", parentPath);

        if (!insertQuery.exec()) {
            qDebug() << "❌ saveDatabase: Ошибка сохранения папки:" << path
                     << "Ошибка:" << insertQuery.lastError().text();
            hasError = true;
            break;
        }

        savedFolders++;
    }

    if (!hasError) {
        qDebug() << "📄 Сохраняем документы...";

        for (auto it = docsCopy.begin(); it != docsCopy.end() && !hasError; ++it) {
            const DocInfo& doc = it.value();
            const QString& key = it.key();

            if (doc.path.isEmpty()) {
                qDebug() << "⚠️ saveDatabase: Пропуск документа с пустым путем";
                continue;
            }

            QString folderPath;
            int pipePos = key.indexOf('|');
            if (pipePos != -1) {
                folderPath = key.left(pipePos);
            } else {
                int lastSlash = doc.path.lastIndexOf('/');
                folderPath = (lastSlash != -1) ? doc.path.left(lastSlash) : "";
            }

            qDebug() << "   📝 Сохраняем документ: key=" << key << " folder_path=" << folderPath << " path=" << doc.path;

            // ===== КРИТИЧЕСКОЕ: Получаем текущее значение responsible_user из БД =====
            // Если в памяти пусто, но в БД есть - используем БД значение
            QString responsibleFromDb = "";
            {
                QSqlQuery selectQuery(m_db);
                selectQuery.prepare("SELECT responsible_user FROM documents WHERE path = :path AND folder_path = :folder_path LIMIT 1");
                selectQuery.bindValue(":path", doc.path);
                selectQuery.bindValue(":folder_path", folderPath);
                if (selectQuery.exec() && selectQuery.next()) {
                    responsibleFromDb = selectQuery.value(0).toString();
                }
            }

            QSqlQuery deleteQuery(m_db);
            deleteQuery.prepare("DELETE FROM documents WHERE path = :path AND folder_path = :folder_path");
            deleteQuery.bindValue(":path", doc.path);
            deleteQuery.bindValue(":folder_path", folderPath);

            if (!deleteQuery.exec()) {
                qDebug() << "⚠️ Ошибка удаления старой записи документа:" << doc.path;
            }

            QSqlQuery insertQuery(m_db);
            insertQuery.prepare(
                "INSERT INTO documents (path, folder_path, name, designation, material, mass, scale, format, status, comment, responsible_user, size, modified, modified_by) "
                "VALUES (:path, :folder_path, :name, :designation, :material, :mass, :scale, :format, :status, :comment, :responsible_user, :size, :modified, :modified_by)"
            );

            insertQuery.bindValue(":path", doc.path);
            insertQuery.bindValue(":folder_path", folderPath);
            insertQuery.bindValue(":name", doc.name);
            insertQuery.bindValue(":designation", doc.designation.isEmpty() ? "" : doc.designation);
            insertQuery.bindValue(":material", doc.material.isEmpty() ? "" : doc.material);
            insertQuery.bindValue(":mass", doc.mass.isEmpty() ? "" : doc.mass);
            insertQuery.bindValue(":scale", doc.scale.isEmpty() ? "" : doc.scale);
            insertQuery.bindValue(":format", doc.format.isEmpty() ? "" : doc.format);
            insertQuery.bindValue(":status", doc.status.isEmpty() ? "В работе" : doc.status);
            insertQuery.bindValue(":comment", doc.comment.isEmpty() ? "" : doc.comment);
            // ===== НОВОЕ: Если в памяти пусто, используем БД значение =====
            insertQuery.bindValue(":responsible_user", doc.responsibleUser.isEmpty() ? responsibleFromDb : doc.responsibleUser);
            insertQuery.bindValue(":size", doc.size);
            insertQuery.bindValue(":modified", doc.modified.isValid() ? doc.modified : QDateTime::currentDateTime());
            insertQuery.bindValue(":modified_by", qgetenv("USERNAME"));

            if (!insertQuery.exec()) {
                qDebug() << "❌ saveDatabase: Ошибка сохранения документа:" << doc.path
                         << "Ошибка:" << insertQuery.lastError().text();
                hasError = true;
                break;
            }

            savedDocuments++;
            if (savedDocuments % 50 == 0) {
                qDebug() << "   📄 Сохранено документов:" << savedDocuments;
            }
        }
    }

    if (hasError) {
        qDebug() << "❌ saveDatabase: Ошибка, выполняем ROLLBACK...";
        if (!query.exec("ROLLBACK;")) {
            qDebug() << "❌ saveDatabase: Ошибка ROLLBACK:" << query.lastError().text();
        }
        // ===== ВАЖНО: НЕ устанавливаем m_dirty = false при ошибке! =====
        // Данные в памяти остаются измененными, попытка сохранения повторится
        releaseDatabaseLock();
        emit saveFinished(false);
        return;
    }

    if (!query.exec("COMMIT;")) {
        qDebug() << "❌ saveDatabase: Ошибка COMMIT:" << query.lastError().text();
        // ===== ОТКАТЫВАЕМ ИЗМЕНЕНИЯ В ПАМЯТИ =====
        loadDatabase();  // Перезагружаем данные из БД, чтобы вернуть в согласованное состояние
        releaseDatabaseLock();
        emit saveFinished(false);
        return;
    }

    // ===== ТОЛЬКО ПОСЛЕ УСПЕШНОГО COMMIT =====
    {
        QMutexLocker locker(&m_mutex);
        m_dirty = false;
        m_changeCounter = 0;
        m_lastSaveTime = QDateTime::currentDateTime();
    }

    releaseDatabaseLock();

    qDebug() << "✅ saveDatabase: Сохранение завершено успешно!";
    qDebug() << "   📊 Сохранено папок:" << savedFolders
             << ", документов:" << savedDocuments;
    qDebug() << "   ⏱️ Время:" << timer.elapsed() << "мс";

    if (m_changeCounter % 10 == 0 && m_changeCounter > 0) {
        qDebug() << "💾 Создаем автоматический бэкап (изменений:" << m_changeCounter << ")";
        backupDatabase();
    }

    emit saveFinished(true);

    // ===== КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: НЕ перезагружаем ВСЕ документы! =====
    // loadDocumentsFromDb() перезаписывает ВСЕ m_docs, теряя свежие данные из UI
    // Вместо этого просто отправляем сигналы, чтобы UI обновился
    // Данные в памяти уже актуальные, так как мы их копировали перед сохранением

    // loadDocumentsFromDb();  // ← УДАЛЕНО! Это перезаписывает память!

    emit databaseChanged();
    emit statisticsChanged();

    qDebug() << "📢 Отправлены сигналы: saveFinished, databaseChanged, statisticsChanged";
}

bool DatabaseManager::validateDataIntegrity()
{
    for (auto it = m_folders.begin(); it != m_folders.end(); ++it) {
        const QString& path = it.key();
        const FolderData& data = it.value();

        if (!path.isEmpty() && data.name.isEmpty()) {
            qDebug() << "⚠️ validateDataIntegrity: Папка с пустым именем:" << path;
            return false;
        }

        int typeInt = static_cast<int>(data.type);
        if (typeInt < 0 || typeInt > 12) {
            qDebug() << "⚠️ validateDataIntegrity: Неверный тип папки:" << typeInt
                     << "для пути:" << path;
            return false;
        }
    }

    for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
        const DocInfo& doc = it.value();
        if (doc.path.isEmpty()) {
            qDebug() << "⚠️ validateDataIntegrity: Документ с пустым путем";
            return false;
        }
        if (doc.name.isEmpty()) {
            qDebug() << "⚠️ validateDataIntegrity: Документ с пустым именем:" << doc.path;
            return false;
        }
    }

    return true;
}

void DatabaseManager::rollbackDatabase()
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        return;
    }

    QSqlQuery query(m_db);
    if (!query.exec("ROLLBACK;")) {
        qDebug() << "❌ rollbackDatabase: Ошибка:" << query.lastError().text();
    }

    m_dirty = false;
    loadDatabase();
    qDebug() << "🔄 База данных восстановлена после ошибки";
}

void DatabaseManager::forceSave()
{
    if (m_dirty) {
        saveDatabase();
    }
}

// ============================================================
// BATCH-РЕЖИМ
// ============================================================

void DatabaseManager::beginBatch()
{
    m_batchMode = true;
    m_batchHasChanges = false;
}

void DatabaseManager::endBatch()
{
    m_batchMode = false;

    if (m_batchHasChanges) {
        markDirty();
        m_batchHasChanges = false;
    }
}

void DatabaseManager::markDirty()
{
    m_dirty = true;
    m_changeCounter++;
    if (!m_debounceTimer->isActive()) {
        m_debounceTimer->start(m_debounceDelay);
    }
}

void DatabaseManager::onAutoSave()
{
    if (m_dirty) {
        saveDatabase();
    }
}

void DatabaseManager::onDebounceSave()
{
    if (m_dirty && !m_batchMode) {
        saveDatabase();
    }
}

void DatabaseManager::processSaveQueue()
{
    QMutexLocker locker(&m_queueMutex);

    if (m_isSaving) {
        m_queueTimer->start(500);
        return;
    }

    if (m_saveQueue.isEmpty()) {
        return;
    }

    m_isSaving = true;
    locker.unlock();

    while (!m_saveQueue.isEmpty()) {
        m_saveQueue.dequeue();
        if (m_dirty) {
            saveDatabase();
        }
    }

    m_isSaving = false;
}

// ============================================================
// РАБОТА С ПАПКАМИ
// ============================================================

bool DatabaseManager::addFolder(const QString& parentPath, const QString& name, FolderType type)
{
    if (name.isEmpty()) {
        qDebug() << "❌ addFolder: имя папки пустое";
        return false;
    }

    logOperation("addFolder", QString("parent=%1, name=%2, type=%3")
                                  .arg(parentPath).arg(name).arg(type));

    QString newPath;
    if (parentPath.isEmpty() || parentPath == "\\") {
        newPath = name;
    } else {
        newPath = parentPath + "/" + name;
    }

    if (m_folders.contains(newPath)) {
        qDebug() << "❌ addFolder: папка уже существует:" << newPath;
        return false;
    }

    // ===== ПРОВЕРКА ЦИКЛИЧЕСКИХ ССЫЛОК =====
    if (!parentPath.isEmpty() && parentPath != "\\") {
        QMap<QString, bool> visited;
        if (hasCircularReference(parentPath, visited)) {
            qDebug() << "❌ addFolder: попытка создать циклическую ссылку! parent=" << parentPath;
            logError("addFolder", "Обнаружена попытка создать циклическую ссылку");
            return false;
        }

        // ===== ПРОВЕРКА: НЕЛЬЗЯ СОЗДАТЬ ПАПКУ ВНУТРИ ЕЁ СОБСТВЕННОЙ ПОДПАПКИ =====
        // Если пытаемся создать "А/Б/В" где родитель уже содержит "А/Б/В" где-то в иерархии
        for (auto it = m_folders.begin(); it != m_folders.end(); ++it) {
            const QString& existingPath = it.key();
            // Если новый путь будет являться родителем для какой-то папки, а та папка - для parentPath
            if (existingPath.startsWith(newPath + "/") && parentPath.startsWith(existingPath + "/")) {
                qDebug() << "❌ addFolder: циклическая иерархия! " << newPath << " -> " << parentPath;
                logError("addFolder", "Обнаружена циклическая иерархия папок");
                return false;
            }
        }
    }

    if (!m_db.isOpen()) {
        FolderData fd;
        fd.name = name;
        fd.type = type;
        m_folders[newPath] = fd;
        qDebug() << "✅ addFolder (в память):" << newPath;
        return true;
    }

    if (!acquireDatabaseLock(2000)) {
        qDebug() << "❌ addFolder: не удалось захватить блокировку БД";
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec("BEGIN TRANSACTION;")) {
        qDebug() << "❌ addFolder: не удалось начать транзакцию";
        releaseDatabaseLock();
        return false;
    }

    FolderData fd;
    fd.name = name;
    fd.type = type;
    m_folders[newPath] = fd;

    // ===== ИСПРАВЛЕНИЕ: используем DatabaseAbstraction для INSERT OR IGNORE =====
    QMap<QString, QVariant> folderValues;
    folderValues["path"] = newPath;
    folderValues["name"] = name;
    folderValues["type"] = static_cast<int>(type);
    folderValues["created_by"] = qgetenv("USERNAME");

    if (parentPath.isEmpty() || parentPath == "\\") {
        folderValues["parent_path"] = "";
    } else {
        folderValues["parent_path"] = parentPath;
    }

    QString insertFolderSql = DatabaseAbstraction::insertOrIgnore("folders", folderValues, QStringList() << "path");

    QSqlQuery insertQuery(m_db);
    if (insertQuery.prepare(insertFolderSql)) {
        for (auto it = folderValues.begin(); it != folderValues.end(); ++it) {
            insertQuery.bindValue(":" + it.key(), it.value());
        }

        if (!insertQuery.exec()) {
            qDebug() << "❌ Ошибка сохранения папки в БД:" << newPath << insertQuery.lastError().text();
            m_folders.remove(newPath);
            query.exec("ROLLBACK;");
            releaseDatabaseLock();
            return false;
        }
    }

    if (!query.exec("COMMIT;")) {
        qDebug() << "❌ addFolder: ошибка COMMIT:" << query.lastError().text();
        query.exec("ROLLBACK;");
        m_folders.remove(newPath);
        releaseDatabaseLock();
        return false;
    }

    releaseDatabaseLock();

    qDebug() << "✅ addFolder:" << newPath << "(тип:" << type << ")";

    if (!m_batchMode) {
        markDirty();
    } else {
        m_batchHasChanges = true;
    }

    logOperation("addFolder_SUCCESS", newPath);

    return true;
}

bool DatabaseManager::removeFolder(const QString& path)
{
    if (path.isEmpty()) {
        qDebug() << "❌ removeFolder: путь пустой";
        return false;
    }

    if (!m_folders.contains(path)) {
        qDebug() << "❌ removeFolder: папка не найдена:" << path;
        return false;
    }

    logOperation("removeFolder", path);

    if (!acquireDatabaseLock(2000)) {
        qDebug() << "❌ removeFolder: не удалось захватить блокировку БД";
        return false;
    }

    qDebug() << "🗑️ removeFolder:" << path;

    // Собираем все вложенные папки
    QStringList toRemove;
    for (auto it = m_folders.begin(); it != m_folders.end(); ++it) {
        if (it.key().startsWith(path + "/") || it.key() == path) {
            toRemove.append(it.key());
        }
    }

    // ===== ИСПРАВЛЕНИЕ 10: УДАЛЯЕМ ВСЕ ДОКУМЕНТЫ ИЗ m_docs =====
    QList<QString> keysToRemove;
    for (const QString& p : toRemove) {
        for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
            // Проверяем ключ: folderPath + "|" + filePath
            if (it.key().startsWith(p + "|")) {
                keysToRemove.append(it.key());
                qDebug() << "   🗑️ Документ для удаления:" << it.key();
            }
        }
    }

    // Удаляем все найденные документы
    for (const QString& key : keysToRemove) {
        m_docs.remove(key);
        qDebug() << "   ✅ Удален DocInfo для:" << key;
    }

    // Удаляем папки из памяти
    for (const QString& p : toRemove) {
        m_folders.remove(p);
        qDebug() << "   ✅ Удалена папка из памяти:" << p;
    }

    // Удаляем из БД
    if (m_db.isOpen()) {
        QSqlQuery query(m_db);
        if (!query.exec("BEGIN TRANSACTION;")) {
            qDebug() << "❌ removeFolder: не удалось начать транзакцию";
            releaseDatabaseLock();
            return false;
        }

        // Удаляем папки
        for (const QString& p : toRemove) {
            removeFolderFromDb(p);
        }

        // ===== ИСПРАВЛЕНИЕ 11: УДАЛЯЕМ ВСЕ ДОКУМЕНТЫ ДЛЯ ЭТИХ ПАПОК =====
        QSqlQuery deleteDocsQuery(m_db);
        for (const QString& p : toRemove) {
            deleteDocsQuery.prepare("DELETE FROM documents WHERE folder_path = :folder_path");
            deleteDocsQuery.bindValue(":folder_path", p);
            if (!deleteDocsQuery.exec()) {
                qDebug() << "❌ Ошибка удаления документов для папки:" << p << deleteDocsQuery.lastError().text();
            } else {
                qDebug() << "   ✅ Удалены документы для папки:" << p;
            }
        }

        if (!query.exec("COMMIT;")) {
            qDebug() << "❌ removeFolder: ошибка COMMIT:" << query.lastError().text();
            query.exec("ROLLBACK;");
            releaseDatabaseLock();
            return false;
        }
    }

    releaseDatabaseLock();

    if (!m_batchMode) {
        markDirty();
        emit folderRemoved(path);
    } else {
        m_batchHasChanges = true;
    }

    logOperation("removeFolder_SUCCESS", path);

    return true;
}

bool DatabaseManager::renameFolder(const QString& oldPath, const QString& newName)
{
    if (oldPath.isEmpty()) {
        qDebug() << "❌ renameFolder: старый путь пустой";
        return false;
    }

    if (newName.isEmpty()) {
        qDebug() << "❌ renameFolder: новое имя пустое";
        return false;
    }

    if (!m_folders.contains(oldPath)) {
        qDebug() << "❌ renameFolder: папка не найдена:" << oldPath;
        return false;
    }

    logOperation("renameFolder", QString("%1 → %2").arg(oldPath).arg(newName));

    if (!acquireDatabaseLock(2000)) {
        qDebug() << "❌ renameFolder: не удалось захватить блокировку БД";
        return false;
    }

    qDebug() << "✏️ renameFolder:" << oldPath << "→" << newName;

    m_folders[oldPath].name = newName;

    if (m_db.isOpen()) {
        QSqlQuery query(m_db);
        if (!query.exec("BEGIN TRANSACTION;")) {
            qDebug() << "❌ renameFolder: не удалось начать транзакцию";
            releaseDatabaseLock();
            return false;
        }

        // ===== ОБНОВЛЯЕМ САМУ ПАПКУ =====
        QSqlQuery updateQuery(m_db);
        updateQuery.prepare(
            "UPDATE folders SET name = :name, updated_at = CURRENT_TIMESTAMP, modified_by = :user "
            "WHERE path = :path"
            );
        updateQuery.bindValue(":name", newName);
        updateQuery.bindValue(":path", oldPath);
        updateQuery.bindValue(":user", qgetenv("USERNAME"));

        if (!updateQuery.exec()) {
            qDebug() << "❌ Ошибка переименования папки в БД:" << oldPath << updateQuery.lastError().text();
            query.exec("ROLLBACK;");
            m_folders[oldPath].name = oldPath.mid(oldPath.lastIndexOf('/') + 1);
            releaseDatabaseLock();
            return false;
        }

        // ===== ОБНОВЛЯЕМ ВСЕ ВЛОЖЕННЫЕ ПАПКИ =====
        QSqlQuery updateChildFoldersQuery(m_db);
        updateChildFoldersQuery.prepare(
            "UPDATE folders SET path = :new_path || substr(path, :old_len + 1), "
            "parent_path = CASE WHEN parent_path = :old_path THEN :new_path ELSE :new_parent_path || substr(parent_path, :old_len + 1) END, "
            "updated_at = CURRENT_TIMESTAMP, modified_by = :user "
            "WHERE path LIKE :old_path || '/%' OR parent_path LIKE :old_path || '/%'"
            );
        updateChildFoldersQuery.bindValue(":new_path", oldPath.mid(0, oldPath.lastIndexOf('/') + 1) + newName);
        updateChildFoldersQuery.bindValue(":old_path", oldPath);
        updateChildFoldersQuery.bindValue(":new_parent_path", oldPath.mid(0, oldPath.lastIndexOf('/') + 1) + newName);
        updateChildFoldersQuery.bindValue(":old_len", oldPath.length());
        updateChildFoldersQuery.bindValue(":user", qgetenv("USERNAME"));

        if (!updateChildFoldersQuery.exec()) {
            qDebug() << "⚠️ renameFolder: ошибка обновления вложенных папок:" << updateChildFoldersQuery.lastError().text();
        } else {
            qDebug() << "✅ renameFolder: обновлены вложенные папки";
        }

        // ===== ОБНОВЛЯЕМ FOLDER_PATH ДЛЯ ВСЕХ ДОКУМЕНТОВ =====
        QString newFolderPath = oldPath.mid(0, oldPath.lastIndexOf('/') + 1) + newName;

        QSqlQuery updateDocsQuery(m_db);
        updateDocsQuery.prepare(
            "UPDATE documents SET folder_path = :new_path || substr(folder_path, :old_len + 1), "
            "updated_at = CURRENT_TIMESTAMP, modified_by = :user "
            "WHERE folder_path = :old_path OR folder_path LIKE :old_path || '/%'"
            );
        updateDocsQuery.bindValue(":new_path", newFolderPath);
        updateDocsQuery.bindValue(":old_path", oldPath);
        updateDocsQuery.bindValue(":old_len", oldPath.length());
        updateDocsQuery.bindValue(":user", qgetenv("USERNAME"));

        if (!updateDocsQuery.exec()) {
            qDebug() << "❌ Ошибка обновления документов:" << updateDocsQuery.lastError().text();
            query.exec("ROLLBACK;");
            m_folders[oldPath].name = oldPath.mid(oldPath.lastIndexOf('/') + 1);
            releaseDatabaseLock();
            return false;
        }

        qDebug() << "✅ renameFolder: обновлены документы для папки и подпапок";

        if (!query.exec("COMMIT;")) {
            qDebug() << "❌ renameFolder: ошибка COMMIT:" << query.lastError().text();
            query.exec("ROLLBACK;");
            m_folders[oldPath].name = oldPath.mid(oldPath.lastIndexOf('/') + 1);
            releaseDatabaseLock();
            return false;
        }

        // ===== ОБНОВЛЯЕМ ПУТИ В ПАМЯТИ =====
        QMap<QString, FolderData> foldersToUpdate;
        for (auto it = m_folders.begin(); it != m_folders.end(); ++it) {
            if (it.key() == oldPath) {
                // Папка уже обновлена, пропускаем
                continue;
            }
            if (it.key().startsWith(oldPath + "/")) {
                // Вложенная папка
                QString newKey = newFolderPath + it.key().mid(oldPath.length());
                foldersToUpdate[newKey] = it.value();
            }
        }

        // Удаляем старые пути и добавляем новые
        for (auto it = foldersToUpdate.begin(); it != foldersToUpdate.end(); ++it) {
            QString oldKey = it.key();
            // Восстанавливаем старый ключ из нового
            QString reconstructedOldKey = oldPath + oldKey.mid(newFolderPath.length());
            m_folders.remove(reconstructedOldKey);
            m_folders[it.key()] = it.value();
            qDebug() << "   ✅ Обновлена вложенная папка в памяти:" << reconstructedOldKey << "→" << it.key();
        }

        // ===== ОБНОВЛЯЕМ ДОКУМЕНТЫ В ПАМЯТИ =====
        QList<QString> docsToUpdate;
        for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
            if (it.key().startsWith(oldPath + "|")) {
                docsToUpdate.append(it.key());
            }
        }

        for (const QString& oldDocKey : docsToUpdate) {
            DocInfo doc = m_docs[oldDocKey];
            m_docs.remove(oldDocKey);

            // Вычисляем новый ключ
            QString newDocKey = newFolderPath + oldDocKey.mid(oldPath.length());
            m_docs[newDocKey] = doc;
            qDebug() << "   ✅ Обновлен документ в памяти:" << oldDocKey << "→" << newDocKey;
        }
    }

    releaseDatabaseLock();

    if (!m_batchMode) {
        markDirty();
        emit folderRenamed(oldPath, newName);
    } else {
        m_batchHasChanges = true;
    }

    logOperation("renameFolder_SUCCESS", QString("%1 → %2").arg(oldPath).arg(newName));

    return true;
}

void DatabaseManager::removeFolderFromDb(const QString& path)
{
    if (!m_db.isOpen()) return;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM folders WHERE path = :path");
    query.bindValue(":path", path);

    if (!query.exec()) {
        qDebug() << "❌ Ошибка удаления папки из БД:" << path << query.lastError().text();
    }
}

// ============================================================
// РАБОТА С ФАЙЛАМИ
// ============================================================

bool DatabaseManager::addFile(const QString& folderPath, const QString& filePath,
                              const QString& responsibleUser, const QString& status)
{
    qDebug() << "=== DatabaseManager::addFile ===";
    qDebug() << "folderPath:" << folderPath;
    qDebug() << "filePath:" << filePath;
    qDebug() << "responsibleUser:" << responsibleUser;
    qDebug() << "status:" << status;

    if (folderPath.isEmpty() || filePath.isEmpty()) {
        qDebug() << "❌ addFile: путь пустой";
        return false;
    }

    if (!m_folders.contains(folderPath)) {
        qDebug() << "❌ addFile: папка не найдена:" << folderPath;
        return false;
    }

    QString docKey = folderPath + "|" + filePath;

    // ===== ИСПРАВЛЕНИЕ 1: ПРОВЕРКА В m_docs =====
    if (m_docs.contains(docKey)) {
        qDebug() << "ℹ️ addFile: документ уже есть в m_docs, пропускаем:" << filePath;
        if (!m_folders[folderPath].files.contains(filePath)) {
            m_folders[folderPath].files.append(filePath);
        }
        return true;
    }

    // ===== ПРОВЕРКА В ПАМЯТИ ПАПКИ =====
    if (m_folders[folderPath].files.contains(filePath)) {
        qDebug() << "ℹ️ addFile: файл уже есть в папке, пропускаем:" << filePath;
        return true;
    }

    // ===== ПРОВЕРКА В БД =====
    if (m_db.isOpen()) {
        QSqlQuery checkQuery(m_db);
        checkQuery.prepare("SELECT COUNT(*) FROM documents WHERE folder_path = :folder_path AND path = :path");
        checkQuery.bindValue(":folder_path", folderPath);
        checkQuery.bindValue(":path", filePath);
        checkQuery.exec();

        if (checkQuery.next() && checkQuery.value(0).toInt() > 0) {
            qDebug() << "ℹ️ addFile: запись уже есть в БД:" << filePath;
            // Добавляем в память
            if (!m_folders[folderPath].files.contains(filePath)) {
                m_folders[folderPath].files.append(filePath);
            }
            if (!m_docs.contains(docKey)) {
                DocInfo d;
                d.path = filePath;
                d.name = QFileInfo(filePath).fileName();
                d.format = QFileInfo(filePath).suffix().toLower();
                d.status = "Новый";
                d.folderPath = folderPath;
                QFileInfo fi(filePath);
                d.modified = fi.exists() ? fi.lastModified() : QDateTime::currentDateTime();
                d.size = fi.exists() ? fi.size() : 0;
                m_docs[docKey] = d;
            }
            return true;
        }
    }

    logOperation("addFile", QString("%1 → %2").arg(filePath).arg(folderPath));

    if (!acquireDatabaseLock(2000)) {
        qDebug() << "❌ addFile: не удалось захватить блокировку БД";
        return false;
    }

    // ===== ДОБАВЛЯЕМ В ПАМЯТЬ =====
    m_folders[folderPath].files.append(filePath);

    if (!m_docs.contains(docKey)) {
        DocInfo d;
        d.path = filePath;
        d.folderPath = folderPath;  // ВАЖНО: устанавливаем folderPath
        d.name = QFileInfo(filePath).fileName();
        d.format = QFileInfo(filePath).suffix().toLower();
        d.status = status.isEmpty() ? "Новый" : status;  // ИСПРАВЛЕНИЕ: используем параметр status
        d.responsibleUser = responsibleUser;  // НОВОЕ: устанавливаем ответственного
        d.comment = "";

        QFileInfo fi(filePath);
        if (fi.exists()) {
            d.modified = fi.lastModified();
            d.size = fi.size();
        } else {
            d.modified = QDateTime::currentDateTime();
            d.size = 0;
        }

        m_docs[docKey] = d;
        qDebug() << "   ✅ Добавлен в m_docs:" << docKey;
    }

    // ===== СОХРАНЯЕМ В БД =====
    if (m_db.isOpen()) {
        const DocInfo& doc = m_docs[docKey];

        // ===== ИСПРАВЛЕНИЕ: используем DatabaseAbstraction для INSERT OR REPLACE =====
        QMap<QString, QVariant> fileValues;
        fileValues["path"] = filePath;
        fileValues["name"] = doc.name;
        fileValues["folder_path"] = folderPath;
        fileValues["designation"] = doc.designation.isEmpty() ? "" : doc.designation;
        fileValues["material"] = doc.material.isEmpty() ? "" : doc.material;
        fileValues["mass"] = doc.mass.isEmpty() ? "" : doc.mass;
        fileValues["scale"] = doc.scale.isEmpty() ? "" : doc.scale;
        fileValues["format"] = doc.format.isEmpty() ? "" : doc.format;
        fileValues["status"] = doc.status.isEmpty() ? "Новый" : doc.status;
        fileValues["comment"] = doc.comment.isEmpty() ? "" : doc.comment;
        fileValues["responsible_user"] = responsibleUser.isEmpty() ? "" : responsibleUser;  // НОВОЕ: добавляем в БД
        fileValues["size"] = doc.size;
        fileValues["modified"] = doc.modified.isValid() ? doc.modified : QDateTime::currentDateTime();
        fileValues["created_by"] = qgetenv("USERNAME");

        QString insertFileSql = DatabaseAbstraction::insertOrReplace("documents", fileValues, QStringList() << "path");

        QSqlQuery insertQuery(m_db);
        if (insertQuery.prepare(insertFileSql)) {
            for (auto it = fileValues.begin(); it != fileValues.end(); ++it) {
                insertQuery.bindValue(":" + it.key(), it.value());
            }

            if (!insertQuery.exec()) {
                qDebug() << "❌ Ошибка сохранения файла в БД:" << filePath << insertQuery.lastError().text();
                m_folders[folderPath].files.removeAll(filePath);
                m_docs.remove(docKey);
                releaseDatabaseLock();
                return false;
            }
        }
    }

    // НОВОЕ: Если ответственный назначен, добавляем запись в историю
    if (!responsibleUser.isEmpty()) {
        addAssignment(folderPath, filePath, responsibleUser, QString::fromLocal8Bit(qgetenv("USERNAME")), "", "Назначено при добавлении файла", false);
    }

    releaseDatabaseLock();

    if (!m_batchMode) {
        markDirty();
    } else {
        m_batchHasChanges = true;
    }

    logOperation("addFile_SUCCESS", QString("%1 → %2").arg(filePath).arg(folderPath));

    qDebug() << "✅ addFile: файл успешно добавлен в папку" << folderPath;
    return true;
}

bool DatabaseManager::removeFile(const QString& folderPath, const QString& filePath)
{
    qDebug() << "=== DatabaseManager::removeFile ===";
    qDebug() << "folderPath:" << folderPath;
    qDebug() << "filePath:" << filePath;

    if (folderPath.isEmpty() || filePath.isEmpty()) {
        qDebug() << "❌ removeFile: путь пустой";
        return false;
    }

    if (!m_folders.contains(folderPath)) {
        qDebug() << "❌ removeFile: папка не найдена:" << folderPath;
        return false;
    }

    if (!m_folders[folderPath].files.contains(filePath)) {
        qDebug() << "❌ removeFile: файл не найден в папке:" << filePath;
        return false;
    }

    logOperation("removeFile", QString("%1 из %2").arg(filePath).arg(folderPath));

    if (!acquireDatabaseLock(2000)) {
        qDebug() << "❌ removeFile: не удалось захватить блокировку БД";
        return false;
    }

    qDebug() << "🗑️ removeFile:" << filePath << "из папки:" << folderPath;

    // ===== УДАЛЯЕМ ИЗ ПАМЯТИ =====
    // 1. Удаляем из списка файлов папки
    m_folders[folderPath].files.removeAll(filePath);

    // 2. Удаляем ТОЛЬКО ДЛЯ ЭТОЙ КОНКРЕТНОЙ ПАПКИ
    QString docKey = folderPath + "|" + filePath;
    if (m_docs.contains(docKey)) {
        m_docs.remove(docKey);
        qDebug() << "   ✅ Удален DocInfo для:" << docKey;
    }

    // ===== ИСПРАВЛЕНИЕ 7: УДАЛЯЕМ ИЗ БД ТОЛЬКО ДЛЯ ЭТОЙ ПАПКИ =====
    if (m_db.isOpen()) {
        QSqlQuery deleteQuery(m_db);
        deleteQuery.prepare("DELETE FROM documents WHERE path = :path AND folder_path = :folder_path");
        deleteQuery.bindValue(":path", filePath);
        deleteQuery.bindValue(":folder_path", folderPath);

        if (!deleteQuery.exec()) {
            qDebug() << "❌ Ошибка удаления файла из БД:" << filePath << deleteQuery.lastError().text();
            // Восстанавливаем в памяти
            m_folders[folderPath].files.append(filePath);
            // Восстанавливаем DocInfo
            DocInfo d;
            d.path = filePath;
            d.folderPath = folderPath;  // НОВОЕ
            d.name = QFileInfo(filePath).fileName();
            d.status = "В работе";
            QFileInfo fi(filePath);
            d.modified = fi.exists() ? fi.lastModified() : QDateTime::currentDateTime();
            d.size = fi.exists() ? fi.size() : 0;
            m_docs[docKey] = d;
            releaseDatabaseLock();
            return false;
        }

        qDebug() << "   ✅ Удален из БД: path=" << filePath << ", folder_path=" << folderPath;

        // ===== НОВОЕ (v7): АРХИВИРУЕМ ВСЕ ДАННЫЕ ПЕРЕД УДАЛЕНИЕМ =====
        QString fullDocKey = folderPath + "|" + filePath;
        QString currentUser = QString::fromLocal8Bit(qgetenv("USERNAME"));

        // 1. Архивируем основные данные документа
        if (m_docs.contains(docKey)) {
            const DocInfo& doc = m_docs[docKey];
            QSqlQuery archiveQuery(m_db);
            archiveQuery.prepare(
                "INSERT INTO deleted_documents "
                "(doc_key, path, folder_path, name, designation, material, mass, scale, format, status, comment, responsible_user, last_known_size, deleted_by) "
                "VALUES (:doc_key, :path, :folder_path, :name, :designation, :material, :mass, :scale, :format, :status, :comment, :responsible_user, :size, :deleted_by)"
            );
            archiveQuery.bindValue(":doc_key", fullDocKey);
            archiveQuery.bindValue(":path", filePath);
            archiveQuery.bindValue(":folder_path", folderPath);
            archiveQuery.bindValue(":name", doc.name);
            archiveQuery.bindValue(":designation", doc.designation);
            archiveQuery.bindValue(":material", doc.material);
            archiveQuery.bindValue(":mass", doc.mass);
            archiveQuery.bindValue(":scale", doc.scale);
            archiveQuery.bindValue(":format", doc.format);
            archiveQuery.bindValue(":status", doc.status);
            archiveQuery.bindValue(":comment", doc.comment);
            archiveQuery.bindValue(":responsible_user", doc.responsibleUser);
            archiveQuery.bindValue(":size", doc.size);
            archiveQuery.bindValue(":deleted_by", currentUser);

            if (!archiveQuery.exec()) {
                qDebug() << "⚠️ Ошибка архивирования документа:" << archiveQuery.lastError().text();
            } else {
                qDebug() << "   ✅ Документ заархивирован в deleted_documents";
            }
        }

        // 2. Архивируем историю назначений
        QSqlQuery getAssignmentsQuery(m_db);
        getAssignmentsQuery.prepare("SELECT assigned_to, assigned_by, assigned_date, previous_assignee, comment FROM assignments WHERE doc_key = :doc_key");
        getAssignmentsQuery.bindValue(":doc_key", fullDocKey);

        if (getAssignmentsQuery.exec()) {
            while (getAssignmentsQuery.next()) {
                QSqlQuery archiveAssignQuery(m_db);
                archiveAssignQuery.prepare(
                    "INSERT INTO deleted_assignments (doc_key, assigned_to, assigned_by, assigned_date, previous_assignee, comment, document_name) "
                    "VALUES (:doc_key, :assigned_to, :assigned_by, :assigned_date, :previous_assignee, :comment, :doc_name)"
                );
                archiveAssignQuery.bindValue(":doc_key", fullDocKey);
                archiveAssignQuery.bindValue(":assigned_to", getAssignmentsQuery.value(0).toString());
                archiveAssignQuery.bindValue(":assigned_by", getAssignmentsQuery.value(1).toString());
                archiveAssignQuery.bindValue(":assigned_date", getAssignmentsQuery.value(2).toDateTime());
                archiveAssignQuery.bindValue(":previous_assignee", getAssignmentsQuery.value(3).toString());
                archiveAssignQuery.bindValue(":comment", getAssignmentsQuery.value(4).toString());
                archiveAssignQuery.bindValue(":doc_name", m_docs.contains(docKey) ? m_docs[docKey].name : QFileInfo(filePath).fileName());

                if (!archiveAssignQuery.exec()) {
                    qDebug() << "⚠️ Ошибка архивирования назначения:" << archiveAssignQuery.lastError().text();
                }
            }
            qDebug() << "   ✅ История назначений заархивирована в deleted_assignments";
        }

        // 3. Архивируем историю статусов
        QSqlQuery getStatusQuery(m_db);
        getStatusQuery.prepare("SELECT old_status, new_status, changed_by, changed_date, reason FROM document_status_history WHERE doc_key = :doc_key");
        getStatusQuery.bindValue(":doc_key", fullDocKey);

        if (getStatusQuery.exec()) {
            while (getStatusQuery.next()) {
                QSqlQuery archiveStatusQuery(m_db);
                archiveStatusQuery.prepare(
                    "INSERT INTO deleted_status_history (doc_key, old_status, new_status, changed_by, changed_date, reason, document_name) "
                    "VALUES (:doc_key, :old_status, :new_status, :changed_by, :changed_date, :reason, :doc_name)"
                );
                archiveStatusQuery.bindValue(":doc_key", fullDocKey);
                archiveStatusQuery.bindValue(":old_status", getStatusQuery.value(0).toString());
                archiveStatusQuery.bindValue(":new_status", getStatusQuery.value(1).toString());
                archiveStatusQuery.bindValue(":changed_by", getStatusQuery.value(2).toString());
                archiveStatusQuery.bindValue(":changed_date", getStatusQuery.value(3).toDateTime());
                archiveStatusQuery.bindValue(":reason", getStatusQuery.value(4).toString());
                archiveStatusQuery.bindValue(":doc_name", m_docs.contains(docKey) ? m_docs[docKey].name : QFileInfo(filePath).fileName());

                if (!archiveStatusQuery.exec()) {
                    qDebug() << "⚠️ Ошибка архивирования статуса:" << archiveStatusQuery.lastError().text();
                }
            }
            qDebug() << "   ✅ История статусов заархивирована в deleted_status_history";
        }

        // ===== УДАЛЕНИЕ ИЗ ТАБЛИЦ ИСТОРИИ (с безопасностью от ошибок) =====
        // fullDocKey уже объявлен выше, используем его

        // 1. Удаляем из document_status_history (может не существовать)
        {
            QSqlQuery query(m_db);
            query.prepare("DELETE FROM document_status_history WHERE doc_key = :doc_key");
            query.bindValue(":doc_key", fullDocKey);
            if (!query.exec()) {
                qDebug() << "⚠️ Таблица document_status_history: " << query.lastError().text();
            } else {
                qDebug() << "   ✅ Удалена из document_status_history";
            }
        }

        // 2. Удаляем из assignments (может не существовать)
        {
            QSqlQuery query(m_db);
            query.prepare("DELETE FROM assignments WHERE doc_key = :doc_key");
            query.bindValue(":doc_key", fullDocKey);
            if (!query.exec()) {
                qDebug() << "⚠️ Таблица assignments: " << query.lastError().text();
            } else {
                qDebug() << "   ✅ Удалена из assignments";
            }
        }

        // 3. Удаляем из comments
        {
            QSqlQuery query(m_db);
            query.prepare("DELETE FROM comments WHERE file_path = :file_path AND folder_path = :folder_path");
            query.bindValue(":file_path", filePath);
            query.bindValue(":folder_path", folderPath);
            if (!query.exec()) {
                qDebug() << "⚠️ Ошибка удаления комментариев:" << query.lastError().text();
            } else {
                qDebug() << "   ✅ Удалены комментарии";
            }
        }

        // 4. Удаляем из file_locks (может не существовать)
        {
            QSqlQuery query(m_db);
            query.prepare("DELETE FROM file_locks WHERE file_path = :file_path");
            query.bindValue(":file_path", filePath);
            if (!query.exec()) {
                qDebug() << "⚠️ Таблица file_locks: " << query.lastError().text();
            } else {
                qDebug() << "   ✅ Удалены блокировки";
            }
        }
    }

    releaseDatabaseLock();

    // ===== ОТПРАВЛЯЕМ СИГНАЛЫ =====
    if (!m_batchMode) {
        markDirty();
    } else {
        m_batchHasChanges = true;
    }

    logOperation("removeFile_SUCCESS", QString("%1 из %2").arg(filePath).arg(folderPath));

    qDebug() << "✅ removeFile: файл удален из папки" << folderPath;
    return true;
}

bool DatabaseManager::renameFile(const QString& folderPath, const QString& oldName, const QString& newName)
{
    qDebug() << "=== DatabaseManager::renameFile ===";
    qDebug() << "folderPath:" << folderPath;
    qDebug() << "oldName:" << oldName;
    qDebug() << "newName:" << newName;

    if (folderPath.isEmpty() || oldName.isEmpty() || newName.isEmpty()) {
        qDebug() << "❌ renameFile: параметры пустые";
        return false;
    }

    if (!m_folders.contains(folderPath)) {
        qDebug() << "❌ renameFile: папка не найдена:" << folderPath;
        return false;
    }

    // Ищем полный путь к файлу в папке
    QString oldPath;
    for (const QString& fp : m_folders[folderPath].files) {
        QFileInfo fi(fp);
        if (fi.fileName() == oldName) {
            oldPath = fp;
            break;
        }
    }

    if (oldPath.isEmpty()) {
        qDebug() << "❌ renameFile: файл не найден:" << oldName;
        return false;
    }

    logOperation("renameFile", QString("%1 → %2 в папке %3").arg(oldName).arg(newName).arg(folderPath));

    if (!acquireDatabaseLock(2000)) {
        qDebug() << "❌ renameFile: не удалось захватить блокировку БД";
        return false;
    }

    qDebug() << "✏️ renameFile:" << oldName << "→" << newName << "в папке:" << folderPath;

    // ===== ПЕРЕИМЕНОВЫВАЕМ В ПАМЯТИ =====
    QFileInfo oldFi(oldPath);
    QString newPath = oldFi.absolutePath() + "/" + newName;

    // ===== ИСПРАВЛЕНИЕ 2: ПРОВЕРКА НА СУЩЕСТВОВАНИЕ НОВОГО ФАЙЛА =====
    if (QFile::exists(newPath) && newPath != oldPath) {
        qDebug() << "❌ renameFile: файл с именем" << newName << "уже существует";
        releaseDatabaseLock();
        return false;
    }

    QString oldDocKey = folderPath + "|" + oldPath;
    QString newDocKey = folderPath + "|" + newPath;

    // ===== ИСПРАВЛЕНИЕ 3: ПРОВЕРЯЕМ, ЕСТЬ ЛИ УЖЕ НОВЫЙ ДОКУМЕНТ =====
    if (m_docs.contains(newDocKey) && newDocKey != oldDocKey) {
        qDebug() << "❌ renameFile: документ с новым ключом уже существует:" << newDocKey;
        releaseDatabaseLock();
        return false;
    }

    // Копируем информацию о документе
    if (m_docs.contains(oldDocKey)) {
        DocInfo doc = m_docs[oldDocKey];
        doc.path = newPath;
        doc.name = newName;

        // Удаляем старую запись
        m_docs.remove(oldDocKey);

        // Добавляем новую запись
        m_docs[newDocKey] = doc;

        qDebug() << "   ✅ Обновлен DocInfo: " << oldDocKey << " → " << newDocKey;
    } else {
        // ===== ИСПРАВЛЕНИЕ 4: СОЗДАЕМ НОВЫЙ ДОКУМЕНТ, ЕСЛИ НЕ БЫЛО =====
        qDebug() << "   ⚠️ DocInfo не найден для:" << oldDocKey << ", создаем новый";
        DocInfo doc;
        doc.path = newPath;
        doc.name = newName;
        doc.format = QFileInfo(newPath).suffix().toLower();
        doc.status = "В работе";
        QFileInfo fi(newPath);
        doc.modified = fi.exists() ? fi.lastModified() : QDateTime::currentDateTime();
        doc.size = fi.exists() ? fi.size() : 0;
        m_docs[newDocKey] = doc;
    }

    // Обновляем список файлов в папке
    m_folders[folderPath].files.removeAll(oldPath);
    m_folders[folderPath].files.append(newPath);

    // ===== ОБНОВЛЯЕМ В БД =====
    if (m_db.isOpen()) {
        QSqlQuery query(m_db);

        // Начинаем транзакцию
        if (!query.exec("BEGIN TRANSACTION;")) {
            qDebug() << "❌ renameFile: не удалось начать транзакцию:" << query.lastError().text();
            // Откатываем изменения в памяти
            m_folders[folderPath].files.removeAll(newPath);
            m_folders[folderPath].files.append(oldPath);
            if (m_docs.contains(newDocKey)) {
                DocInfo doc = m_docs[newDocKey];
                m_docs.remove(newDocKey);
                m_docs[oldDocKey] = doc;
            }
            releaseDatabaseLock();
            return false;
        }

        // ===== ИСПРАВЛЕНИЕ 5: УДАЛЯЕМ СТАРУЮ ЗАПИСЬ =====
        QSqlQuery deleteQuery(m_db);
        deleteQuery.prepare("DELETE FROM documents WHERE path = :path AND folder_path = :folder_path");
        deleteQuery.bindValue(":path", oldPath);
        deleteQuery.bindValue(":folder_path", folderPath);

        if (!deleteQuery.exec()) {
            qDebug() << "❌ renameFile: ошибка удаления старого документа:" << deleteQuery.lastError().text();
            query.exec("ROLLBACK;");
            // Откатываем изменения в памяти
            m_folders[folderPath].files.removeAll(newPath);
            m_folders[folderPath].files.append(oldPath);
            if (m_docs.contains(newDocKey)) {
                DocInfo doc = m_docs[newDocKey];
                m_docs.remove(newDocKey);
                m_docs[oldDocKey] = doc;
            }
            releaseDatabaseLock();
            return false;
        }

        // Получаем обновленный DocInfo
        const DocInfo& doc = m_docs[newDocKey];

        // ===== ИСПРАВЛЕНИЕ 6: ВСТАВЛЯЕМ НОВУЮ ЗАПИСЬ =====
        QMap<QString, QVariant> renameFileValues;
        renameFileValues["path"] = newPath;
        renameFileValues["name"] = doc.name;
        renameFileValues["folder_path"] = folderPath;
        renameFileValues["designation"] = doc.designation.isEmpty() ? "" : doc.designation;
        renameFileValues["material"] = doc.material.isEmpty() ? "" : doc.material;
        renameFileValues["mass"] = doc.mass.isEmpty() ? "" : doc.mass;
        renameFileValues["scale"] = doc.scale.isEmpty() ? "" : doc.scale;
        renameFileValues["format"] = doc.format.isEmpty() ? "" : doc.format;
        renameFileValues["status"] = doc.status.isEmpty() ? "В работе" : doc.status;
        renameFileValues["comment"] = doc.comment.isEmpty() ? "" : doc.comment;
        renameFileValues["responsible_user"] = doc.responsibleUser.isEmpty() ? "" : doc.responsibleUser;  // БАГ 3: ДОБАВЛЯЕМ ОТВЕТСТВЕННОГО
        renameFileValues["size"] = doc.size;
        renameFileValues["modified"] = doc.modified.isValid() ? doc.modified : QDateTime::currentDateTime();
        renameFileValues["modified_by"] = qgetenv("USERNAME");

        QString insertRenameSql = DatabaseAbstraction::insertOrReplace("documents", renameFileValues, QStringList() << "path");

        QSqlQuery insertQuery(m_db);
        if (insertQuery.prepare(insertRenameSql)) {
            for (auto it = renameFileValues.begin(); it != renameFileValues.end(); ++it) {
                insertQuery.bindValue(":" + it.key(), it.value());
            }

            if (!insertQuery.exec()) {
                qDebug() << "❌ renameFile: ошибка вставки нового документа:" << insertQuery.lastError().text();
                query.exec("ROLLBACK;");
                // Откатываем изменения в памяти
                m_folders[folderPath].files.removeAll(newPath);
                m_folders[folderPath].files.append(oldPath);
                if (m_docs.contains(newDocKey)) {
                    DocInfo doc2 = m_docs[newDocKey];
                    m_docs.remove(newDocKey);
                    m_docs[oldDocKey] = doc2;
                }
                releaseDatabaseLock();
                return false;
            }
        }

        // Фиксируем транзакцию
        if (!query.exec("COMMIT;")) {
            qDebug() << "❌ renameFile: ошибка COMMIT:" << query.lastError().text();
            query.exec("ROLLBACK;");
            // Откатываем изменения в памяти
            m_folders[folderPath].files.removeAll(newPath);
            m_folders[folderPath].files.append(oldPath);
            if (m_docs.contains(newDocKey)) {
                DocInfo doc2 = m_docs[newDocKey];
                m_docs.remove(newDocKey);
                m_docs[oldDocKey] = doc2;
            }
            releaseDatabaseLock();
            return false;
        }

        qDebug() << "   ✅ Обновлен в БД: " << oldPath << " → " << newPath;
    }

    releaseDatabaseLock();

    if (!m_batchMode) {
        markDirty();
        emit fileRenamed(folderPath, oldName, newName);
        emit databaseChanged();
    } else {
        m_batchHasChanges = true;
    }

    logOperation("renameFile_SUCCESS", QString("%1 → %2 в папке %3").arg(oldName).arg(newName).arg(folderPath));

    qDebug() << "✅ renameFile: файл успешно переименован в папке" << folderPath;
    return true;
}  // <-- ВОТ ЭТА СКОБКА БЫЛА ПРОПУЩЕНА!

bool DatabaseManager::moveFile(const QString& oldFolderPath, const QString& newFolderPath, const QString& filePath)
{
    qDebug() << "=== DatabaseManager::moveFile ===";
    qDebug() << "oldFolderPath:" << oldFolderPath;
    qDebug() << "newFolderPath:" << newFolderPath;
    qDebug() << "filePath:" << filePath;

    if (oldFolderPath.isEmpty() || newFolderPath.isEmpty() || filePath.isEmpty()) {
        qDebug() << "❌ moveFile: параметры пустые";
        return false;
    }

    if (!m_folders.contains(oldFolderPath)) {
        qDebug() << "❌ moveFile: старая папка не найдена:" << oldFolderPath;
        return false;
    }

    if (!m_folders.contains(newFolderPath)) {
        qDebug() << "❌ moveFile: новая папка не найдена:" << newFolderPath;
        return false;
    }

    if (!m_folders[oldFolderPath].files.contains(filePath)) {
        qDebug() << "❌ moveFile: файл не найден в старой папке:" << filePath;
        return false;
    }

    if (oldFolderPath == newFolderPath) {
        qDebug() << "ℹ️ moveFile: старая и новая папки совпадают, операция не требуется";
        return true;
    }

    logOperation("moveFile", QString("%1 из %2 в %3").arg(filePath).arg(oldFolderPath).arg(newFolderPath));

    if (!acquireDatabaseLock(2000)) {
        qDebug() << "❌ moveFile: не удалось захватить блокировку БД";
        return false;
    }

    qDebug() << "📦 moveFile:" << filePath << "из" << oldFolderPath << "в" << newFolderPath;

    QString oldDocKey = oldFolderPath + "|" + filePath;
    QString newDocKey = newFolderPath + "|" + filePath;

    // ===== ОБНОВЛЯЕМ В ПАМЯТИ =====
    // 1. Удаляем из старой папки
    m_folders[oldFolderPath].files.removeAll(filePath);

    // 2. Добавляем в новую папку
    if (!m_folders[newFolderPath].files.contains(filePath)) {
        m_folders[newFolderPath].files.append(filePath);
    }

    // 3. Обновляем DocInfo
    if (m_docs.contains(oldDocKey)) {
        DocInfo doc = m_docs[oldDocKey];
        m_docs.remove(oldDocKey);
        m_docs[newDocKey] = doc;
        qDebug() << "   ✅ Обновлен DocInfo: " << oldDocKey << " → " << newDocKey;
    } else {
        qDebug() << "   ⚠️ DocInfo не найден для:" << oldDocKey;
        // Создаем новый документ если не было
        DocInfo doc;
        doc.path = filePath;
        doc.name = QFileInfo(filePath).fileName();
        doc.format = QFileInfo(filePath).suffix().toLower();
        doc.status = "В работе";
        QFileInfo fi(filePath);
        doc.modified = fi.exists() ? fi.lastModified() : QDateTime::currentDateTime();
        doc.size = fi.exists() ? fi.size() : 0;
        m_docs[newDocKey] = doc;
    }

    // ===== ОБНОВЛЯЕМ В БД =====
    if (m_db.isOpen()) {
        QSqlQuery query(m_db);
        if (!query.exec("BEGIN TRANSACTION;")) {
            qDebug() << "❌ moveFile: не удалось начать транзакцию:" << query.lastError().text();
            // Откатываем изменения в памяти
            m_folders[oldFolderPath].files.append(filePath);
            m_folders[newFolderPath].files.removeAll(filePath);
            if (m_docs.contains(newDocKey)) {
                DocInfo doc = m_docs[newDocKey];
                m_docs.remove(newDocKey);
                m_docs[oldDocKey] = doc;
            }
            releaseDatabaseLock();
            return false;
        }

        // ===== ОБНОВЛЯЕМ folder_path ДЛЯ ДОКУМЕНТА =====
        QSqlQuery updateQuery(m_db);
        updateQuery.prepare("UPDATE documents SET folder_path = :new_folder_path, "
                            "updated_at = CURRENT_TIMESTAMP, modified_by = :user "
                            "WHERE path = :path AND folder_path = :old_folder_path");
        updateQuery.bindValue(":new_folder_path", newFolderPath);
        updateQuery.bindValue(":path", filePath);
        updateQuery.bindValue(":old_folder_path", oldFolderPath);
        updateQuery.bindValue(":user", qgetenv("USERNAME"));

        if (!updateQuery.exec()) {
            qDebug() << "❌ moveFile: ошибка обновления документа:" << updateQuery.lastError().text();
            query.exec("ROLLBACK;");
            // Откатываем изменения в памяти
            m_folders[oldFolderPath].files.append(filePath);
            m_folders[newFolderPath].files.removeAll(filePath);
            if (m_docs.contains(newDocKey)) {
                DocInfo doc = m_docs[newDocKey];
                m_docs.remove(newDocKey);
                m_docs[oldDocKey] = doc;
            }
            releaseDatabaseLock();
            return false;
        }

        // БАГ 4: ОБНОВЛЯЕМ doc_key В ТАБЛИЦЕ ИСТОРИИ СТАТУСОВ
        QSqlQuery updateStatusHistoryQuery(m_db);
        updateStatusHistoryQuery.prepare(
            "UPDATE document_status_history SET doc_key = :new_doc_key "
            "WHERE doc_key = :old_doc_key"
        );
        updateStatusHistoryQuery.bindValue(":new_doc_key", newDocKey);
        updateStatusHistoryQuery.bindValue(":old_doc_key", oldDocKey);

        if (!updateStatusHistoryQuery.exec()) {
            qDebug() << "⚠️ moveFile: ошибка обновления document_status_history:" << updateStatusHistoryQuery.lastError().text();
            // Не откатываем транзакцию - это некритичная ошибка
        } else {
            qDebug() << "   ✅ Обновлены записи в document_status_history:" << oldDocKey << " → " << newDocKey;
        }

        // БАГ 4: ОБНОВЛЯЕМ doc_key В ТАБЛИЦЕ ИСТОРИИ НАЗНАЧЕНИЙ
        QSqlQuery updateAssignmentsQuery(m_db);
        updateAssignmentsQuery.prepare(
            "UPDATE assignments SET doc_key = :new_doc_key "
            "WHERE doc_key = :old_doc_key"
        );
        updateAssignmentsQuery.bindValue(":new_doc_key", newDocKey);
        updateAssignmentsQuery.bindValue(":old_doc_key", oldDocKey);

        if (!updateAssignmentsQuery.exec()) {
            qDebug() << "⚠️ moveFile: ошибка обновления assignments:" << updateAssignmentsQuery.lastError().text();
            // Не откатываем транзакцию - это некритичная ошибка
        } else {
            qDebug() << "   ✅ Обновлены записи в assignments:" << oldDocKey << " → " << newDocKey;
        }

        if (!query.exec("COMMIT;")) {
            qDebug() << "❌ moveFile: ошибка COMMIT:" << query.lastError().text();
            query.exec("ROLLBACK;");
            // Откатываем изменения в памяти
            m_folders[oldFolderPath].files.append(filePath);
            m_folders[newFolderPath].files.removeAll(filePath);
            if (m_docs.contains(newDocKey)) {
                DocInfo doc = m_docs[newDocKey];
                m_docs.remove(newDocKey);
                m_docs[oldDocKey] = doc;
            }
            releaseDatabaseLock();
            return false;
        }

        qDebug() << "   ✅ Обновлен в БД: " << oldDocKey << " → " << newDocKey;
    }

    releaseDatabaseLock();

    if (!m_batchMode) {
        markDirty();
    } else {
        m_batchHasChanges = true;
    }

    logOperation("moveFile_SUCCESS", QString("%1 из %2 в %3").arg(filePath).arg(oldFolderPath).arg(newFolderPath));

    qDebug() << "✅ moveFile: файл успешно перемещен";
    return true;
}

void DatabaseManager::removeDocumentFromDb(const QString& path)
{
    if (!m_db.isOpen()) return;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM documents WHERE path = :path");
    query.bindValue(":path", path);

    if (!query.exec()) {
        qDebug() << "❌ Ошибка удаления документа из БД:" << path << query.lastError().text();
    }
}

// ============================================================
// УВЕДОМЛЕНИЯ
// ============================================================

void DatabaseManager::saveNotifications(const QList<NotificationItem>& notifications)
{
    if (!m_db.isOpen()) return;

    QSqlQuery query(m_db);
    query.exec("BEGIN TRANSACTION;");

    query.exec("DELETE FROM notifications;");

    for (const NotificationItem& item : notifications) {
        QSqlQuery insertQuery(m_db);
        insertQuery.prepare(
            "INSERT INTO notifications "
            "(icon, title, text, file_path, folder_path, time, is_read, type, status, source_user) "
            "VALUES (:icon, :title, :text, :file_path, :folder_path, :time, :is_read, :type, :status, :source_user)"
            );
        insertQuery.bindValue(":icon", item.icon);
        insertQuery.bindValue(":title", item.title);
        insertQuery.bindValue(":text", item.text);
        insertQuery.bindValue(":file_path", item.filePath);
        insertQuery.bindValue(":folder_path", item.folderPath);
        insertQuery.bindValue(":time", item.time);
        insertQuery.bindValue(":is_read", item.isRead ? 1 : 0);
        insertQuery.bindValue(":type", static_cast<int>(item.type));
        insertQuery.bindValue(":status", static_cast<int>(item.status));
        insertQuery.bindValue(":source_user", item.sourceUser);

        if (!insertQuery.exec()) {
            qDebug() << "❌ Ошибка сохранения уведомления:" << insertQuery.lastError().text();
            query.exec("ROLLBACK;");
            return;
        }
    }

    query.exec("COMMIT;");
    emit notificationsChanged();
}

void DatabaseManager::loadNotifications(QList<NotificationItem>& notifications)
{
    notifications.clear();

    if (!m_db.isOpen()) return;

    QSqlQuery query = executeQueryWithResult(
        "SELECT icon, title, text, file_path, folder_path, time, is_read, type, status, source_user "
        "FROM notifications ORDER BY time DESC"
        );

    while (query.next()) {
        NotificationItem item;
        item.icon = query.value("icon").toString();
        item.title = query.value("title").toString();
        item.text = query.value("text").toString();
        item.filePath = query.value("file_path").toString();
        item.folderPath = query.value("folder_path").toString();
        item.time = query.value("time").toDateTime();
        item.isRead = query.value("is_read").toInt() == 1;
        item.type = static_cast<NotificationManager::NotificationType>(query.value("type").toInt());
        item.status = static_cast<DocumentStatus>(query.value("status").toInt());
        item.sourceUser = query.value("source_user").toString();

        notifications.append(item);
    }
    emit notificationsChanged();
}

void DatabaseManager::clearNotifications()
{
    if (m_db.isOpen()) {
        executeQuery("DELETE FROM notifications;");
    }
}

// ============================================================
// БЭКАП
// ============================================================

void DatabaseManager::backupDatabase()
{
    if (!m_db.isOpen()) return;

    QString backupPath = m_backupPath;
    if (QFile::exists(backupPath)) {
        QFile::remove(backupPath);
    }

    QSqlDatabase backupDb = QSqlDatabase::addDatabase("QSQLITE", "backup_connection");
    backupDb.setDatabaseName(backupPath);

    if (!backupDb.open()) {
        qDebug() << "❌ Не удалось создать бэкап:" << backupDb.lastError().text();
        return;
    }

    QSqlQuery sourceQuery(m_db);
    QSqlQuery destQuery(backupDb);

    QStringList tables = {"folders", "documents", "notifications", "comments"};
    for (const QString& table : tables) {
        sourceQuery.exec("SELECT * FROM " + table);
        if (sourceQuery.lastError().isValid()) continue;

        QSqlRecord record = sourceQuery.record();
        QStringList fieldNames;
        for (int i = 0; i < record.count(); ++i) {
            fieldNames << record.fieldName(i);
        }

        QString createSql = "CREATE TABLE IF NOT EXISTS " + table + " (";
        for (const QString& field : fieldNames) {
            createSql += field + ", ";
        }
        createSql = createSql.left(createSql.length() - 2) + ");";
        destQuery.exec(createSql);

        while (sourceQuery.next()) {
            QString insertSql = "INSERT INTO " + table + " (";
            QString valuesSql = "VALUES (";
            for (const QString& field : fieldNames) {
                insertSql += field + ", ";
                valuesSql += ":" + field + ", ";
            }
            insertSql = insertSql.left(insertSql.length() - 2) + ") ";
            valuesSql = valuesSql.left(valuesSql.length() - 2) + ")";

            QSqlQuery insertQuery(backupDb);
            insertQuery.prepare(insertSql + valuesSql);

            for (const QString& field : fieldNames) {
                insertQuery.bindValue(":" + field, sourceQuery.value(field));
            }

            insertQuery.exec();
        }
    }

    backupDb.close();
    QSqlDatabase::removeDatabase("backup_connection");

    qDebug() << "✅ Бэкап создан:" << backupPath;
}

void DatabaseManager::restoreDatabase(const QString& backupPath)
{
    if (!QFile::exists(backupPath)) return;

    if (m_db.isOpen()) {
        m_db.close();
    }

    QFile::remove(m_dbPath);
    QFile::copy(backupPath, m_dbPath);

    m_db.setDatabaseName(m_dbPath);
    if (m_db.open()) {
        loadDatabase();
        emit databaseChanged();
    }
}
void DatabaseManager::debugPrintDocuments()
{
    if (!m_db.isOpen()) {
        qDebug() << "❌ debugPrintDocuments: БД не открыта";
        return;
    }

    QSqlQuery query(m_db);
    if (!query.exec("SELECT path, folder_path, name FROM documents")) {
        qDebug() << "❌ Ошибка запроса:" << query.lastError().text();
        return;
    }

    qDebug() << "=== СОДЕРЖИМОЕ ТАБЛИЦЫ documents ===";
    int count = 0;
    while (query.next()) {
        qDebug() << "  path:" << query.value(0).toString()
        << "folder_path:" << query.value(1).toString()
        << "name:" << query.value(2).toString();
        count++;
    }
    qDebug() << "Всего записей:" << count;
    qDebug() << "=== КОНЕЦ ===";
}
QStringList DatabaseManager::getMissingFiles() const
{
    QStringList missing;
    for (auto it = m_folders.begin(); it != m_folders.end(); ++it) {
        for (const QString& filePath : it.value().files) {
            if (!QFile::exists(filePath)) {
                missing.append(filePath);
            }
        }
    }
    return missing;
}

bool DatabaseManager::checkFileExists(const QString& filePath) const
{
    // ИСПРАВЛЕНИЕ 12: Проверка на диске и обновление информации в m_docs
    QFileInfo fi(filePath);

    if (!fi.exists()) {
        qDebug() << "⚠️ Файл не существует на диске:" << filePath;
        return false;
    }

    // Проверяем, нужно ли обновить информацию в m_docs
    for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
        if (it.value().path == filePath) {
            // Обновляем размер и время модификации если они изменились
            qint64 newSize = fi.size();
            QDateTime newModified = fi.lastModified();

            if (newSize != it.value().size || newModified != it.value().modified) {
                qDebug() << "🔄 Обновляем информацию о файле:" << filePath;
                const_cast<DocInfo&>(it.value()).size = newSize;
                const_cast<DocInfo&>(it.value()).modified = newModified;
                const_cast<DatabaseManager*>(this)->markDirty();
            }
            break;
        }
    }

    return true;
}

// ============================================================
// БЕЗОПАСНЫЕ МЕТОДЫ ЧТЕНИЯ (для UI потока)
// ============================================================

QMap<QString, FolderData> DatabaseManager::getFoldersCopy() const
{
    QReadLocker locker(&m_dataLock);
    return m_folders;  // Возвращаем копию, поэтому безопасно
}

QMap<QString, DocInfo> DatabaseManager::getDocumentsCopy() const
{
    QReadLocker locker(&m_dataLock);
    return m_docs;  // Возвращаем копию, поэтому безопасно
}

// ============================================================
// РАБОТА С ДОКУМЕНТАМИ
// ============================================================

// ============================================================
// РАБОТА С ДОКУМЕНТАМИ
// ============================================================

DocInfo* DatabaseManager::getDocument(const QString& folderPath, const QString& filePath)
{
    QString docKey = folderPath + "|" + filePath;
    if (m_docs.contains(docKey)) {
        return &m_docs[docKey];
    }
    return nullptr;
}

bool DatabaseManager::hasDocument(const QString& folderPath, const QString& filePath)
{
    QString docKey = folderPath + "|" + filePath;
    return m_docs.contains(docKey);
}

// НОВАЯ ВЕРСИЯ (v6) - с использованием folderPath и полного ключа
bool DatabaseManager::updateDocumentStatus(const QString& folderPath, const QString& filePath, const QString& status, const QString& reason, const QString& changedBy)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        qDebug() << "❌ БД не открыта";
        return false;
    }

    qDebug() << "🔍 updateDocumentStatus (v6): folderPath=" << folderPath << ", filePath=" << filePath << ", status=" << status;

    // Используем полный ключ для поиска в памяти
    QString docKey = folderPath + "|" + filePath;

    // Получаем старый статус перед обновлением
    QString oldStatus;
    if (m_docs.contains(docKey)) {
        oldStatus = m_docs[docKey].status;
    } else {
        qDebug() << "⚠️ Документ не найден в памяти:" << docKey;
        return false;
    }

    // Обновляем в БД по полному ключу (folder_path + path)
    QSqlQuery query(m_db);
    query.prepare("UPDATE documents SET status = :status WHERE folder_path = :folder_path AND path = :path");
    query.bindValue(":status", status);
    query.bindValue(":folder_path", folderPath);
    query.bindValue(":path", filePath);

    if (!query.exec()) {
        qDebug() << "❌ Ошибка обновления статуса в БД:" << query.lastError().text();
        return false;
    }

    int rowsAffected = query.numRowsAffected();
    qDebug() << "📝 Обновлено строк в БД:" << rowsAffected;

    if (rowsAffected == 0) {
        qDebug() << "⚠️ ВНИМАНИЕ: Строк не обновлено! Документ с key=" << docKey << "не найден в БД";
        return false;
    }

    // Добавляем запись в историю статусов
    if (!oldStatus.isEmpty() && oldStatus != status) {
        addStatusChange(folderPath, filePath, oldStatus, status, changedBy, reason);
    }

    // Обновляем в памяти
    m_docs[docKey].status = status;
    qDebug() << "✅ Статус обновлен в памяти:" << docKey << "→" << status;
    m_dirty = true;

    return true;
}

// СТАРАЯ ВЕРСИЯ (для совместимости, DEPRECATED)
bool DatabaseManager::updateDocumentStatus_Legacy(const QString& filePath, const QString& status, const QString& reason, const QString& changedBy)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        qDebug() << "❌ БД не открыта";
        return false;
    }

    qDebug() << "🔍 updateDocumentStatus_Legacy (DEPRECATED): filePath=" << filePath << ", status=" << status;

    // Получаем старый статус перед обновлением
    QString oldStatus;
    QString foundFolderPath;
    for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
        if (it.value().path == filePath) {
            oldStatus = it.value().status;
            foundFolderPath = it.value().folderPath;
            break;
        }
    }

    if (foundFolderPath.isEmpty()) {
        qDebug() << "❌ Документ не найден для филипа:" << filePath;
        return false;
    }

    // Используем новую версию с полным ключом
    return updateDocumentStatus(foundFolderPath, filePath, status, reason, changedBy);
}

bool DatabaseManager::updateDocumentResponsible(const QString& folderPath, const QString& filePath, const QString& responsible, const QString& assignedBy)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        qDebug() << "❌ БД не открыта";
        return false;
    }

    qDebug() << "🔍 updateDocumentResponsible (v6): folderPath=" << folderPath << ", filePath=" << filePath << ", responsible=" << responsible;

    // Используем полный ключ для поиска в памяти
    QString docKey = folderPath + "|" + filePath;

    if (!m_docs.contains(docKey)) {
        qDebug() << "❌ Документ не найден в памяти:" << docKey;
        return false;
    }

    QString previousAssignee = m_docs[docKey].responsibleUser;

    // Обновляем в БД
    QSqlQuery query(m_db);
    query.prepare("UPDATE documents SET responsible_user = :responsible WHERE folder_path = :folder_path AND path = :path");
    query.bindValue(":responsible", responsible);
    query.bindValue(":folder_path", folderPath);
    query.bindValue(":path", filePath);

    if (!query.exec()) {
        qDebug() << "❌ Ошибка обновления ответственного в БД:" << query.lastError().text();
        return false;
    }

    // Добавляем запись в историю назначений
    if (!responsible.isEmpty() && responsible != previousAssignee) {
        addAssignment(folderPath, filePath, responsible, assignedBy, previousAssignee);
    }

    // Обновляем в памяти
    m_docs[docKey].responsibleUser = responsible;
    qDebug() << "✅ Ответственный обновлен в памяти:" << docKey << "→" << responsible;
    m_dirty = true;

    return true;
}

// СТАРАЯ ВЕРСИЯ (для совместимости, DEPRECATED)
bool DatabaseManager::updateDocumentResponsible_Legacy(const QString& filePath, const QString& responsible, const QString& assignedBy)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        qDebug() << "❌ БД не открыта";
        return false;
    }

    qDebug() << "🔍 updateDocumentResponsible_Legacy (DEPRECATED): filePath=" << filePath << ", responsible=" << responsible;

    // Получаем папку для филипа
    QString foundFolderPath;
    for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
        if (it.value().path == filePath) {
            foundFolderPath = it.value().folderPath;
            break;
        }
    }

    if (foundFolderPath.isEmpty()) {
        qDebug() << "❌ Документ не найден для филипа:" << filePath;
        return false;
    }

    // Используем новую версию с полным ключом
    return updateDocumentResponsible(foundFolderPath, filePath, responsible, assignedBy);
}

// НОВАЯ ВЕРСИЯ (v6) - с полным ключом folderPath|filePath
bool DatabaseManager::addStatusChange(const QString& folderPath, const QString& filePath, const QString& oldStatus, const QString& newStatus, const QString& changedBy, const QString& reason)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        qDebug() << "❌ БД не открыта";
        return false;
    }

    QString docKey = folderPath + "|" + filePath;
    qDebug() << "📝 addStatusChange (v6): doc_key=" << docKey << ", status:" << oldStatus << "→" << newStatus;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO document_status_history (doc_key, old_status, new_status, changed_by, reason) "
                  "VALUES (:doc_key, :old_status, :new_status, :changed_by, :reason)");
    query.bindValue(":doc_key", docKey);
    query.bindValue(":old_status", oldStatus);
    query.bindValue(":new_status", newStatus);
    query.bindValue(":changed_by", changedBy);
    query.bindValue(":reason", reason);

    if (!query.exec()) {
        qDebug() << "❌ Ошибка добавления в историю статусов:" << query.lastError().text();
        return false;
    }

    // Обновляем в памяти
    if (m_docs.contains(docKey)) {
        StatusChange change(oldStatus, newStatus, changedBy, QDateTime::currentDateTime(), reason);
        m_docs[docKey].statusHistory.append(change);
        qDebug() << "✅ История статусов обновлена в памяти";
    }

    return true;
}

// СТАРАЯ ВЕРСИЯ (для совместимости, DEPRECATED)
bool DatabaseManager::addStatusChange_Legacy(const QString& filePath, const QString& oldStatus, const QString& newStatus, const QString& changedBy, const QString& reason)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        qDebug() << "❌ БД не открыта";
        return false;
    }

    qDebug() << "📝 addStatusChange_Legacy (DEPRECATED): filePath=" << filePath;

    // Найти папку для филипа
    QString foundFolderPath;
    for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
        if (it.value().path == filePath) {
            foundFolderPath = it.value().folderPath;
            break;
        }
    }

    if (foundFolderPath.isEmpty()) {
        qDebug() << "❌ Документ не найден для филипа:" << filePath;
        return false;
    }

    // Используем новую версию с полным ключом
    return addStatusChange(foundFolderPath, filePath, oldStatus, newStatus, changedBy, reason);
}

// НОВАЯ ВЕРСИЯ (v6+v8) - с полным ключом folderPath|filePath и поддержкой is_manual
bool DatabaseManager::addAssignment(const QString& folderPath, const QString& filePath, const QString& assignedTo, const QString& assignedBy, const QString& previousAssignee, const QString& comment, bool isManual)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        qDebug() << "❌ БД не открыта";
        return false;
    }

    QString docKey = folderPath + "|" + filePath;
    qDebug() << "👤 addAssignment (v8): doc_key=" << docKey << ", assigned_to=" << assignedTo << ", is_manual=" << isManual;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO assignments (doc_key, assigned_to, assigned_by, previous_assignee, comment, is_manual) "
                  "VALUES (:doc_key, :assigned_to, :assigned_by, :previous_assignee, :comment, :is_manual)");
    query.bindValue(":doc_key", docKey);
    query.bindValue(":assigned_to", assignedTo);
    query.bindValue(":assigned_by", assignedBy);
    query.bindValue(":previous_assignee", previousAssignee);
    query.bindValue(":comment", comment);
    query.bindValue(":is_manual", isManual ? 1 : 0);

    if (!query.exec()) {
        qDebug() << "❌ Ошибка добавления в историю назначений:" << query.lastError().text();
        return false;
    }

    // Обновляем в памяти
    if (m_docs.contains(docKey)) {
        Assignment assignment(assignedTo, assignedBy, QDateTime::currentDateTime(), previousAssignee, comment);
        m_docs[docKey].assignmentHistory.append(assignment);
        qDebug() << "✅ История назначений обновлена в памяти";
    }

    // Испускаем сигнал об изменении назначения
    emit assignmentChanged(docKey, assignedTo);
    qDebug() << "📢 Сигнал assignmentChanged отправлен для docKey=" << docKey;

    return true;
}

// СТАРАЯ ВЕРСИЯ (для совместимости, DEPRECATED)
bool DatabaseManager::addAssignment_Legacy(const QString& filePath, const QString& assignedTo, const QString& assignedBy, const QString& previousAssignee, const QString& comment)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        qDebug() << "❌ БД не открыта";
        return false;
    }

    qDebug() << "👤 addAssignment_Legacy (DEPRECATED): filePath=" << filePath;

    // Найти папку для филипа
    QString foundFolderPath;
    for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
        if (it.value().path == filePath) {
            foundFolderPath = it.value().folderPath;
            break;
        }
    }

    if (foundFolderPath.isEmpty()) {
        qDebug() << "❌ Документ не найден для филипа:" << filePath;
        return false;
    }

    // Используем новую версию с полным ключом
    return addAssignment(foundFolderPath, filePath, assignedTo, assignedBy, previousAssignee, comment);
}

// ===== ИСТОРИЯ ДОКУМЕНТОВ =====
QList<StatusChange> DatabaseManager::getStatusHistory(const QString& folderPath, const QString& filePath)
{
    QMutexLocker locker(&m_mutex);

    QString docKey = folderPath + "|" + filePath;

    if (m_docs.contains(docKey)) {
        return m_docs[docKey].statusHistory;
    }

    return QList<StatusChange>();
}

QList<Assignment> DatabaseManager::getAssignmentHistory(const QString& folderPath, const QString& filePath)
{
    QMutexLocker locker(&m_mutex);

    QString docKey = folderPath + "|" + filePath;

    if (m_docs.contains(docKey)) {
        return m_docs[docKey].assignmentHistory;
    }

    return QList<Assignment>();
}

// ===== СИНХРОНИЗАЦИЯ С ДИСКОМ =====
bool DatabaseManager::syncFolderWithDisk(const QString& folderPath)
{
    if (!m_folders.contains(folderPath)) {
        qDebug() << "❌ syncFolderWithDisk: папка не найдена:" << folderPath;
        return false;
    }

    qDebug() << "🔄 Синхронизация папки с диском:" << folderPath;

    FolderData& folder = m_folders[folderPath];
    QStringList filesOnDisk;
    QStringList filesToRemove;

    // ===== ПРОВЕРЯЕМ ВСЕ ФАЙЛЫ, ЧТО ЕСТЬ НА ДИСКЕ =====
    QDir folderDir(folderPath);
    if (folderDir.exists()) {
        QFileInfoList files = folderDir.entryInfoList(QDir::Files);
        for (const QFileInfo& fi : files) {
            filesOnDisk.append(fi.absoluteFilePath());
        }
    } else {
        qDebug() << "⚠️ Папка на диске не существует:" << folderPath;
    }

    // ===== НАХОДИМ УДАЛЕННЫЕ ФАЙЛЫ (в БД но НЕ на диске) =====
    for (const QString& filePath : folder.files) {
        if (!QFile::exists(filePath)) {
            filesToRemove.append(filePath);
            qDebug() << "   🗑️ Файл удален на диске:" << filePath;
        }
    }

    // ===== НАХОДИМ НОВЫЕ ФАЙЛЫ (на диске но НЕ в БД) =====
    QStringList filesToAdd;
    for (const QString& diskFilePath : filesOnDisk) {
        if (!folder.files.contains(diskFilePath)) {
            filesToAdd.append(diskFilePath);
            qDebug() << "   ➕ Новый файл на диске:" << diskFilePath;
        }
    }

    // ===== УДАЛЯЕМ НЕСУЩЕСТВУЮЩИЕ ФАЙЛЫ =====
    if (!m_db.isOpen()) {
        qDebug() << "⚠️ syncFolderWithDisk: БД не открыта, работаем только с памятью";
        for (const QString& filePath : filesToRemove) {
            folder.files.removeAll(filePath);
            QString docKey = folderPath + "|" + filePath;
            m_docs.remove(docKey);
        }
    } else {
        QSqlQuery deleteQuery(m_db);
        for (const QString& filePath : filesToRemove) {
            folder.files.removeAll(filePath);
            QString docKey = folderPath + "|" + filePath;
            m_docs.remove(docKey);

            // Удаляем из БД
            deleteQuery.prepare("DELETE FROM documents WHERE path = :path AND folder_path = :folder_path");
            deleteQuery.bindValue(":path", filePath);
            deleteQuery.bindValue(":folder_path", folderPath);
            if (!deleteQuery.exec()) {
                qDebug() << "⚠️ Ошибка удаления файла из БД:" << filePath;
            } else {
                qDebug() << "   ✅ Удален из БД:" << filePath;
            }
        }
    }

    // ===== ДОБАВЛЯЕМ НОВЫЕ ФАЙЛЫ =====
    for (const QString& filePath : filesToAdd) {
        if (!folder.files.contains(filePath)) {
            folder.files.append(filePath);

            QString docKey = folderPath + "|" + filePath;
            if (!m_docs.contains(docKey)) {
                DocInfo d;
                d.path = filePath;
                d.name = QFileInfo(filePath).fileName();
                d.format = QFileInfo(filePath).suffix().toLower();
                d.status = "В работе";

                QFileInfo fi(filePath);
                d.modified = fi.lastModified();
                d.size = fi.size();

                m_docs[docKey] = d;

                // Добавляем в БД
                if (m_db.isOpen()) {
                    // ===== ИСПРАВЛЕНИЕ: используем DatabaseAbstraction для INSERT OR REPLACE =====
                    QMap<QString, QVariant> syncFileValues;
                    syncFileValues["path"] = filePath;
                    syncFileValues["name"] = d.name;
                    syncFileValues["folder_path"] = folderPath;
                    syncFileValues["format"] = d.format;
                    syncFileValues["status"] = d.status;
                    syncFileValues["size"] = d.size;
                    syncFileValues["modified"] = d.modified;
                    syncFileValues["created_by"] = qgetenv("USERNAME");

                    QString insertSyncSql = DatabaseAbstraction::insertOrReplace("documents", syncFileValues, QStringList() << "path");

                    QSqlQuery insertQuery(m_db);
                    if (insertQuery.prepare(insertSyncSql)) {
                        for (auto it = syncFileValues.begin(); it != syncFileValues.end(); ++it) {
                            insertQuery.bindValue(":" + it.key(), it.value());
                        }

                        if (!insertQuery.exec()) {
                            qDebug() << "⚠️ Ошибка добавления файла в БД:" << filePath;
                        } else {
                            qDebug() << "   ✅ Добавлен в БД:" << filePath;
                        }
                    }
                }
            }
        }
    }

    qDebug() << "✅ syncFolderWithDisk: удалено" << filesToRemove.size() << ", добавлено" << filesToAdd.size();

    if (!m_batchMode && (filesToRemove.size() > 0 || filesToAdd.size() > 0)) {
        markDirty();
        emit databaseChanged();
    }

    return true;
}

QStringList DatabaseManager::getOrphanFiles() const
{
    QStringList orphans;

    // Проверяем все документы
    for (auto it = m_docs.begin(); it != m_docs.end(); ++it) {
        const QString& key = it.key();
        int pipePos = key.indexOf('|');
        if (pipePos == -1) continue;

        QString folderPath = key.left(pipePos);

        // Если папка не существует в m_folders - это сирота
        if (!m_folders.contains(folderPath)) {
            orphans.append(key);
        }
    }

    return orphans;
}

bool DatabaseManager::adoptOrphanFile(const QString& docKey, const QString& newFolderPath)
{
    if (!m_docs.contains(docKey)) {
        qDebug() << "❌ adoptOrphanFile: документ не найден:" << docKey;
        return false;
    }

    if (!m_folders.contains(newFolderPath)) {
        qDebug() << "❌ adoptOrphanFile: папка не найдена:" << newFolderPath;
        return false;
    }

    // Получаем документ
    DocInfo doc = m_docs[docKey];

    // Удаляем старый ключ
    m_docs.remove(docKey);

    // Создаем новый ключ
    QString newKey = newFolderPath + "|" + doc.path;

    // Добавляем документ с новым ключом
    m_docs[newKey] = doc;

    // Добавляем файл в папку
    if (!m_folders[newFolderPath].files.contains(doc.path)) {
        m_folders[newFolderPath].files.append(doc.path);
    }

    // Обновляем БД
    if (m_db.isOpen()) {
        QSqlQuery updateQuery(m_db);
        updateQuery.prepare("UPDATE documents SET folder_path = :new_folder_path "
                            "WHERE path = :path AND folder_path = :old_folder_path");

        int pipePos = docKey.indexOf('|');
        QString oldFolderPath = docKey.left(pipePos);

        updateQuery.bindValue(":new_folder_path", newFolderPath);
        updateQuery.bindValue(":path", doc.path);
        updateQuery.bindValue(":old_folder_path", oldFolderPath);

        if (!updateQuery.exec()) {
            qDebug() << "❌ Ошибка обновления документа:" << updateQuery.lastError().text();
            return false;
        }
    }

    qDebug() << "✅ adoptOrphanFile: файл" << doc.path << "перенесен в папку" << newFolderPath;
    return true;
}

int DatabaseManager::adoptAllOrphanFiles(const QString& newFolderPath)
{
    if (!m_folders.contains(newFolderPath)) {
        qDebug() << "❌ adoptAllOrphanFiles: папка не найдена:" << newFolderPath;
        return 0;
    }

    QStringList orphans = getOrphanFiles();
    int adopted = 0;

    for (const QString& docKey : orphans) {
        if (adoptOrphanFile(docKey, newFolderPath)) {
            adopted++;
        }
    }

    qDebug() << "✅ adoptAllOrphanFiles: принято" << adopted << "из" << orphans.size() << "файлов-сирот";
    return adopted;
}

// ============================================================
// БЛОКИРОВКА ФАЙЛОВ (ДЛЯ МНОГОПОЛЬЗОВАТЕЛЬСКОЙ РАБОТЫ)
// ============================================================

bool DatabaseManager::lockFile(const QString& filePath, const QString& userName)
{
    if (filePath.isEmpty() || userName.isEmpty()) {
        qDebug() << "❌ lockFile: параметры пустые";
        return false;
    }

    if (!m_db.isOpen()) {
        qDebug() << "❌ lockFile: БД не открыта";
        return false;
    }

    qDebug() << "🔒 Попытка заблокировать файл:" << filePath << "пользователем:" << userName;

    // ===== ПРОВЕРЯЕМ, НЕ ЗАБЛОКИРОВАН ЛИ УЖЕ =====
    QSqlQuery checkQuery(m_db);
    checkQuery.prepare(
        "SELECT locked_by, locked_at FROM documents WHERE path = :path AND locked_by IS NOT NULL"
        );
    checkQuery.bindValue(":path", filePath);

    if (checkQuery.exec() && checkQuery.next()) {
        QString lockedBy = checkQuery.value(0).toString();
        if (lockedBy != userName) {
            qDebug() << "❌ lockFile: файл уже заблокирован пользователем:" << lockedBy;
            return false;
        } else {
            qDebug() << "ℹ️ lockFile: файл уже заблокирован этим пользователем";
            return true;
        }
    }

    // ===== БЛОКИРУЕМ ФАЙЛ =====
    QSqlQuery lockQuery(m_db);
    lockQuery.prepare(
        "UPDATE documents SET locked_by = :locked_by, locked_at = CURRENT_TIMESTAMP "
        "WHERE path = :path"
        );
    lockQuery.bindValue(":locked_by", userName);
    lockQuery.bindValue(":path", filePath);

    if (!lockQuery.exec()) {
        qDebug() << "❌ Ошибка блокировки файла:" << lockQuery.lastError().text();
        return false;
    }

    qDebug() << "✅ Файл заблокирован:" << filePath << "пользователем:" << userName;
    return true;
}

bool DatabaseManager::unlockFile(const QString& filePath, const QString& userName)
{
    if (filePath.isEmpty()) {
        qDebug() << "❌ unlockFile: путь пустой";
        return false;
    }

    if (!m_db.isOpen()) {
        qDebug() << "❌ unlockFile: БД не открыта";
        return false;
    }

    qDebug() << "🔓 Разблокировка файла:" << filePath << "пользователем:" << userName;

    // ===== ПРОВЕРЯЕМ, КТО ЗАБЛОКИРОВАЛ =====
    QSqlQuery checkQuery(m_db);
    checkQuery.prepare("SELECT locked_by FROM documents WHERE path = :path");
    checkQuery.bindValue(":path", filePath);

    if (checkQuery.exec() && checkQuery.next()) {
        QString lockedBy = checkQuery.value(0).toString();
        if (!lockedBy.isEmpty() && lockedBy != userName) {
            qDebug() << "❌ unlockFile: файл заблокирован другим пользователем:" << lockedBy;
            return false;
        }
    }

    // ===== РАЗБЛОКИРУЕМ =====
    QSqlQuery unlockQuery(m_db);
    unlockQuery.prepare(
        "UPDATE documents SET locked_by = NULL, locked_at = NULL "
        "WHERE path = :path"
        );
    unlockQuery.bindValue(":path", filePath);

    if (!unlockQuery.exec()) {
        qDebug() << "❌ Ошибка разблокировки файла:" << unlockQuery.lastError().text();
        return false;
    }

    qDebug() << "✅ Файл разблокирован:" << filePath;
    return true;
}

bool DatabaseManager::isFileLocked(const QString& filePath) const
{
    if (filePath.isEmpty() || !m_db.isOpen()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT locked_by FROM documents WHERE path = :path");
    query.bindValue(":path", filePath);

    if (query.exec() && query.next()) {
        QString lockedBy = query.value(0).toString();
        return !lockedBy.isEmpty();
    }

    return false;
}

QString DatabaseManager::getFileLocker(const QString& filePath) const
{
    if (filePath.isEmpty() || !m_db.isOpen()) {
        return "";
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT locked_by, locked_at FROM documents WHERE path = :path");
    query.bindValue(":path", filePath);

    if (query.exec() && query.next()) {
        QString lockedBy = query.value(0).toString();
        if (!lockedBy.isEmpty()) {
            QDateTime lockedAt = query.value(1).toDateTime();
            return QString("%1 (с %2)").arg(lockedBy).arg(lockedAt.toString("dd.MM.yyyy hh:mm:ss"));
        }
    }

    return "";
}

// ============================================================
// ДВУСТОРОННЯЯ СИНХРОНИЗАЦИЯ POSTGRESQL ↔ SQLITE
// ============================================================

void DatabaseManager::synchronizeBothDatabases()
{
    QMutexLocker locker(&m_syncMutex);

    if (!m_isPostgresqlAvailable || !m_isSqliteAvailable) {
        return;
    }

    if (!m_dbPostgreSQL.isOpen() || !m_dbSQLite.isOpen()) {
        qDebug() << "⚠️ synchronizeBothDatabases: одна из БД не открыта";
        return;
    }

    qDebug() << "🔄 Начало синхронизации PostgreSQL ↔ SQLite...";
    m_lastSyncTime = QDateTime::currentDateTime();

    // Синхронизируем таблицы
    QStringList tables = {"folders", "documents", "file_locks", "notifications", "users_online"};

    for (const QString& table : tables) {
        // Получаем максимальный timestamp из обеих БД
        QSqlQuery postgresQuery(m_dbPostgreSQL);
        QSqlQuery sqliteQuery(m_dbSQLite);

        // PostgreSQL → SQLite (если PostgreSQL есть новые данные)
        postgresQuery.prepare(QString("SELECT * FROM %1 WHERE modified > :lastSync ORDER BY modified DESC").arg(table));
        postgresQuery.bindValue(":lastSync", m_lastSyncTime.addSecs(-SYNC_INTERVAL / 1000));

        if (postgresQuery.exec()) {
            while (postgresQuery.next()) {
                // Копируем запись в SQLite
                QSqlRecord record = postgresQuery.record();
                QString insertSql = QString("INSERT OR REPLACE INTO %1 (").arg(table);
                QString valuesSql = "VALUES (";

                for (int i = 0; i < record.count(); ++i) {
                    insertSql += record.fieldName(i) + ", ";
                    valuesSql += ":" + record.fieldName(i) + ", ";
                }

                insertSql = insertSql.left(insertSql.length() - 2) + ") ";
                valuesSql = valuesSql.left(valuesSql.length() - 2) + ")";

                QSqlQuery insertQuery(m_dbSQLite);
                if (insertQuery.prepare(insertSql + valuesSql)) {
                    for (int i = 0; i < record.count(); ++i) {
                        insertQuery.bindValue(":" + record.fieldName(i), record.value(i));
                    }
                    if (!insertQuery.exec()) {
                        qDebug() << "⚠️ Ошибка синхронизации из PostgreSQL в SQLite:" << insertQuery.lastError().text();
                    }
                }
            }
        }

        // SQLite → PostgreSQL (если SQLite есть новые данные)
        sqliteQuery.prepare(QString("SELECT * FROM %1 WHERE modified > :lastSync ORDER BY modified DESC").arg(table));
        sqliteQuery.bindValue(":lastSync", m_lastSyncTime.addSecs(-SYNC_INTERVAL / 1000));

        if (sqliteQuery.exec()) {
            while (sqliteQuery.next()) {
                QSqlRecord record = sqliteQuery.record();
                QString insertSql = QString("INSERT INTO %1 (").arg(table);
                QString valuesSql = "VALUES (";
                QString onConflictSql = " ON CONFLICT DO UPDATE SET ";

                for (int i = 0; i < record.count(); ++i) {
                    insertSql += record.fieldName(i) + ", ";
                    valuesSql += ":" + record.fieldName(i) + ", ";
                    onConflictSql += record.fieldName(i) + " = EXCLUDED." + record.fieldName(i) + ", ";
                }

                insertSql = insertSql.left(insertSql.length() - 2) + ") ";
                valuesSql = valuesSql.left(valuesSql.length() - 2) + ")";
                onConflictSql = onConflictSql.left(onConflictSql.length() - 2);

                QSqlQuery insertQuery(m_dbPostgreSQL);
                QString fullSql = insertSql + valuesSql + onConflictSql;
                if (insertQuery.prepare(fullSql)) {
                    for (int i = 0; i < record.count(); ++i) {
                        insertQuery.bindValue(":" + record.fieldName(i), record.value(i));
                    }
                    if (!insertQuery.exec()) {
                        qDebug() << "⚠️ Ошибка синхронизации из SQLite в PostgreSQL:" << insertQuery.lastError().text();
                    }
                }
            }
        }
    }

    qDebug() << "✅ Синхронизация завершена в" << m_lastSyncTime.toString("hh:mm:ss");
}

void DatabaseManager::onSyncTimeout()
{
    if (m_isPostgresqlAvailable && m_isSqliteAvailable) {
        synchronizeBothDatabases();
    }
}

// ===== СИНХРОНИЗАЦИЯ ПОЛЬЗОВАТЕЛЬСКИХ СЕССИЙ (v0.21) =====

void DatabaseManager::registerUserSession(const QString& userFIO)
{
    if (userFIO.isEmpty()) {
        qWarning() << "❌ registerUserSession: ФИО пуста";
        return;
    }

    m_currentUserFIO = userFIO;

    QString computerName = QSysInfo::machineHostName();

    qDebug() << "📝 Регистрация сессии пользователя:" << userFIO << "на ПК:" << computerName;

    // Используем основную БД (PostgreSQL если есть, иначе SQLite)
    QSqlDatabase& db = m_db;

    if (!db.isOpen()) {
        qWarning() << "❌ БД не открыта при registerUserSession";
        return;
    }

    QSqlQuery query(db);

    // INSERT OR UPDATE user_sessions
    query.prepare(
        "INSERT INTO user_sessions (user_fio, computer_name, last_login, is_active) "
        "VALUES (:user_fio, :computer_name, datetime('now'), 1) "
        "ON CONFLICT(user_fio, computer_name) DO UPDATE SET "
        "last_login = datetime('now'), is_active = 1"
    );

    query.bindValue(":user_fio", userFIO);
    query.bindValue(":computer_name", computerName);

    if (!query.exec()) {
        qWarning() << "❌ Ошибка registerUserSession:" << query.lastError().text();
    } else {
        qDebug() << "✅ Сессия зарегистрирована:" << userFIO;
    }
}

QString DatabaseManager::getLastActiveUser()
{
    if (!m_db.isOpen()) {
        qWarning() << "❌ БД не открыта при getLastActiveUser";
        return "";
    }

    QString computerName = QSysInfo::machineHostName();

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT user_fio FROM user_sessions "
        "WHERE computer_name = :computer_name AND is_active = 1 "
        "ORDER BY last_login DESC LIMIT 1"
    );
    query.bindValue(":computer_name", computerName);

    if (query.exec() && query.next()) {
        QString fio = query.value(0).toString();
        qDebug() << "📋 Последний активный пользователь на этом ПК:" << fio;
        return fio;
    }

    qDebug() << "⚠️ Нет активных пользователей на этом ПК";
    return "";
}

QString DatabaseManager::getCurrentActiveUserFIO() const
{
    return m_currentUserFIO;
}

void DatabaseManager::closeUserSession(const QString& userFIO)
{
    if (userFIO.isEmpty()) {
        qWarning() << "❌ closeUserSession: ФИО пуста";
        return;
    }

    QString computerName = QSysInfo::machineHostName();

    qDebug() << "🔚 Закрытие сессии:" << userFIO << "на ПК:" << computerName;

    if (!m_db.isOpen()) {
        qWarning() << "❌ БД не открыта при closeUserSession";
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(
        "UPDATE user_sessions SET is_active = 0 "
        "WHERE user_fio = :user_fio AND computer_name = :computer_name"
    );
    query.bindValue(":user_fio", userFIO);
    query.bindValue(":computer_name", computerName);

    if (!query.exec()) {
        qWarning() << "❌ Ошибка closeUserSession:" << query.lastError().text();
    } else {
        qDebug() << "✅ Сессия закрыта:" << userFIO;
    }

    m_currentUserFIO = "";
}