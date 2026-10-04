// DatabaseAbstraction.h
#ifndef DATABASEABSTRACTION_H
#define DATABASEABSTRACTION_H

#include <QString>
#include <QMap>
#include <QVariant>
#include <QSqlDatabase>

/**
 * Слой абстракции для генерации SQL запросов, совместимых с SQLite и PostgreSQL
 * Решает все 30 проблем совместимости между двумя СУБД
 */
class DatabaseAbstraction
{
public:
    enum DatabaseType {
        SQLite,
        PostgreSQL,
        Unknown
    };

    // Установить текущий тип БД
    static void setDatabaseType(DatabaseType type);
    static DatabaseType getDatabaseType();
    static void detectDatabaseType(const QSqlDatabase& db);

    // ===== INSERT ОПЕРАЦИИ =====
    // INSERT OR REPLACE (для SQLite) → INSERT ... ON CONFLICT DO UPDATE (для PostgreSQL)
    static QString insertOrReplace(
        const QString& table,
        const QMap<QString, QVariant>& values,
        const QStringList& conflictKeys = QStringList()
    );

    // INSERT OR IGNORE (для SQLite) → INSERT ... ON CONFLICT DO NOTHING (для PostgreSQL)
    static QString insertOrIgnore(
        const QString& table,
        const QMap<QString, QVariant>& values,
        const QStringList& conflictKeys = QStringList()
    );

    // ===== УСЛОВИЯ ПОИСКА =====
    // Регистронезависимый LIKE для обеих БД
    static QString makeLikeClause(const QString& field, const QString& pattern);
    static QString makeLikeCaseInsensitive(const QString& field, const QString& pattern);

    // ===== ФУНКЦИИ ДАТ =====
    // Форматирование даты: strftime для SQLite, TO_CHAR для PostgreSQL
    static QString formatDate(const QString& fieldName, const QString& format = "YYYY-MM-DD");
    static QString formatDateTime(const QString& fieldName, const QString& format = "YYYY-MM-DD HH:MI:SS");
    static QString getCurrentTimestamp();

    // ===== ФУНКЦИИ СТРОК =====
    // GROUP_CONCAT для SQLite, STRING_AGG для PostgreSQL
    static QString groupConcat(const QString& field, const QString& separator = ",");

    // substr для SQLite, substring для PostgreSQL
    static QString substring(const QString& field, int start, int length = -1);

    // length для SQLite, char_length для PostgreSQL (в PostgreSQL длина в символах, не байтах)
    static QString charLength(const QString& field);

    // ===== ОБСЛУЖИВАНИЕ БД =====
    // VACUUM - оптимизация БД
    static QString vacuum();

    // REINDEX - переиндексирование
    static QString reindex(const QString& tableName = "");

    // Проверка целостности
    static QString integrityCheck();

    // ===== PRAGMA КОМАНДЫ =====
    // Выполнить PRAGMA команду (для SQLite), для PostgreSQL вернуть пустую строку
    static QString pragma(const QString& pragmaCommand);

    // ===== СЛУЖЕБНЫЕ МЕТОДЫ =====
    static QString databaseTypeName();
    static bool isSQLite();
    static bool isPostgreSQL();

private:
    static DatabaseType s_type;
    static QString buildPlaceholders(int count);
    static QString escapeIdentifier(const QString& identifier);
};

#endif // DATABASEABSTRACTION_H
