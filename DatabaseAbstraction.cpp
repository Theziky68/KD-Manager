// DatabaseAbstraction.cpp
#include "DatabaseAbstraction.h"
#include <QDebug>
#include <QSqlDriver>

DatabaseAbstraction::DatabaseType DatabaseAbstraction::s_type = DatabaseAbstraction::Unknown;

void DatabaseAbstraction::setDatabaseType(DatabaseType type)
{
    s_type = type;
    QString typeName = (type == SQLite) ? "SQLite" : (type == PostgreSQL) ? "PostgreSQL" : "Unknown";
    qDebug() << "🔧 DatabaseAbstraction установлен на:" << typeName;
}

DatabaseAbstraction::DatabaseType DatabaseAbstraction::getDatabaseType()
{
    return s_type;
}

void DatabaseAbstraction::detectDatabaseType(const QSqlDatabase& db)
{
    if (!db.isOpen()) {
        s_type = Unknown;
        return;
    }

    // Используем QSqlDatabase::driverName() вместо db.driver()->driverName()
    QString driverName = db.driverName();

    if (driverName == "QSQLITE") {
        s_type = SQLite;
    } else if (driverName == "QPSQL") {
        s_type = PostgreSQL;
    } else {
        s_type = Unknown;
    }

    qDebug() << "🔍 Автоопределен тип БД:" << databaseTypeName();
}

// ============================================================
// INSERT ОПЕРАЦИИ
// ============================================================

QString DatabaseAbstraction::insertOrReplace(
    const QString& table,
    const QMap<QString, QVariant>& values,
    const QStringList& conflictKeys)
{
    if (values.isEmpty()) {
        return QString();
    }

    QStringList fields;
    QStringList placeholders;
    int i = 0;
    for (auto it = values.begin(); it != values.end(); ++it) {
        fields << it.key();
        placeholders << ":" + it.key();
        i++;
    }

    if (s_type == PostgreSQL) {
        // PostgreSQL: INSERT ... ON CONFLICT DO UPDATE
        QString conflictClause;
        if (!conflictKeys.isEmpty()) {
            conflictClause = "(" + conflictKeys.join(", ") + ")";
        } else {
            // Предполагаем, что первое поле - это ID
            conflictClause = "(id)";
        }

        QString updateClause;
        for (const QString& field : fields) {
            if (!conflictKeys.contains(field, Qt::CaseInsensitive)) {
                updateClause += field + " = EXCLUDED." + field + ", ";
            }
        }
        if (updateClause.endsWith(", ")) {
            updateClause.chop(2);
        }

        return QString(
            "INSERT INTO %1 (%2) VALUES (%3) "
            "ON CONFLICT %4 DO UPDATE SET %5"
        ).arg(table, fields.join(", "), placeholders.join(", "), conflictClause, updateClause);
    } else {
        // SQLite: INSERT OR REPLACE
        return QString(
            "INSERT OR REPLACE INTO %1 (%2) VALUES (%3)"
        ).arg(table, fields.join(", "), placeholders.join(", "));
    }
}

QString DatabaseAbstraction::insertOrIgnore(
    const QString& table,
    const QMap<QString, QVariant>& values,
    const QStringList& conflictKeys)
{
    if (values.isEmpty()) {
        return QString();
    }

    QStringList fields;
    QStringList placeholders;
    for (auto it = values.begin(); it != values.end(); ++it) {
        fields << it.key();
        placeholders << ":" + it.key();
    }

    if (s_type == PostgreSQL) {
        // PostgreSQL: INSERT ... ON CONFLICT DO NOTHING
        QString conflictClause;
        if (!conflictKeys.isEmpty()) {
            conflictClause = "(" + conflictKeys.join(", ") + ")";
        } else {
            conflictClause = "(id)";
        }

        return QString(
            "INSERT INTO %1 (%2) VALUES (%3) "
            "ON CONFLICT %4 DO NOTHING"
        ).arg(table, fields.join(", "), placeholders.join(", "), conflictClause);
    } else {
        // SQLite: INSERT OR IGNORE
        return QString(
            "INSERT OR IGNORE INTO %1 (%2) VALUES (%3)"
        ).arg(table, fields.join(", "), placeholders.join(", "));
    }
}

// ============================================================
// УСЛОВИЯ ПОИСКА
// ============================================================

QString DatabaseAbstraction::makeLikeClause(const QString& field, const QString& pattern)
{
    // Регистронезависимый поиск для обеих БД
    if (s_type == PostgreSQL) {
        // PostgreSQL: ILIKE (case-insensitive)
        return QString("%1 ILIKE %2").arg(field, pattern);
    } else {
        // SQLite: LIKE уже регистронезависимый
        return QString("%1 LIKE %2").arg(field, pattern);
    }
}

QString DatabaseAbstraction::makeLikeCaseInsensitive(const QString& field, const QString& pattern)
{
    // Явное приведение к нижнему регистру для обеих БД
    if (s_type == PostgreSQL) {
        return QString("LOWER(%1) LIKE LOWER(%2)").arg(field, pattern);
    } else {
        return QString("LOWER(%1) LIKE LOWER(%2)").arg(field, pattern);
    }
}

// ============================================================
// ФУНКЦИИ ДАТ
// ============================================================

QString DatabaseAbstraction::formatDate(const QString& fieldName, const QString& format)
{
    if (s_type == PostgreSQL) {
        // PostgreSQL: TO_CHAR(date, format)
        // Конвертируем формат из strftime в PostgreSQL
        // YYYY-MM-DD HH:MI:SS → YYYY-MM-DD HH24:MI:SS
        QString pgFormat = format;
        pgFormat.replace("%Y", "YYYY");
        pgFormat.replace("%m", "MM");
        pgFormat.replace("%d", "DD");
        pgFormat.replace("%H", "HH24");
        pgFormat.replace("%M", "MI");
        pgFormat.replace("%S", "SS");
        return QString("TO_CHAR(%1, '%2')").arg(fieldName, pgFormat);
    } else {
        // SQLite: strftime(format, date)
        return QString("strftime('%1', %2)").arg(format, fieldName);
    }
}

QString DatabaseAbstraction::formatDateTime(const QString& fieldName, const QString& format)
{
    return formatDate(fieldName, format);
}

QString DatabaseAbstraction::getCurrentTimestamp()
{
    if (s_type == PostgreSQL) {
        return "CURRENT_TIMESTAMP";
    } else {
        return "CURRENT_TIMESTAMP";  // Работает в обеих
    }
}

// ============================================================
// ФУНКЦИИ СТРОК
// ============================================================

QString DatabaseAbstraction::groupConcat(const QString& field, const QString& separator)
{
    if (s_type == PostgreSQL) {
        // PostgreSQL: STRING_AGG(field, separator)
        return QString("STRING_AGG(%1, '%2')").arg(field, separator);
    } else {
        // SQLite: GROUP_CONCAT(field, separator)
        return QString("GROUP_CONCAT(%1, '%2')").arg(field, separator);
    }
}

QString DatabaseAbstraction::substring(const QString& field, int start, int length)
{
    if (s_type == PostgreSQL) {
        // PostgreSQL: SUBSTRING(field FROM start FOR length)
        if (length > 0) {
            return QString("SUBSTRING(%1 FROM %2 FOR %3)").arg(field).arg(start).arg(length);
        } else {
            return QString("SUBSTRING(%1 FROM %2)").arg(field).arg(start);
        }
    } else {
        // SQLite: substr(field, start, length)
        if (length > 0) {
            return QString("substr(%1, %2, %3)").arg(field).arg(start).arg(length);
        } else {
            return QString("substr(%1, %2)").arg(field).arg(start);
        }
    }
}

QString DatabaseAbstraction::charLength(const QString& field)
{
    if (s_type == PostgreSQL) {
        // PostgreSQL: CHAR_LENGTH (для символов, не байтов)
        return QString("CHAR_LENGTH(%1)").arg(field);
    } else {
        // SQLite: LENGTH (считает символы, не байты)
        return QString("LENGTH(%1)").arg(field);
    }
}

// ============================================================
// ОБСЛУЖИВАНИЕ БД
// ============================================================

QString DatabaseAbstraction::vacuum()
{
    if (s_type == PostgreSQL) {
        // PostgreSQL: VACUUM ANALYZE
        return "VACUUM ANALYZE;";
    } else {
        // SQLite: VACUUM
        return "VACUUM;";
    }
}

QString DatabaseAbstraction::reindex(const QString& tableName)
{
    if (s_type == PostgreSQL) {
        // PostgreSQL: REINDEX TABLE table_name
        if (!tableName.isEmpty()) {
            return QString("REINDEX TABLE %1;").arg(tableName);
        } else {
            return "REINDEX;";  // Reindex all
        }
    } else {
        // SQLite: REINDEX
        if (!tableName.isEmpty()) {
            return QString("REINDEX %1;").arg(tableName);
        } else {
            return "REINDEX;";
        }
    }
}

QString DatabaseAbstraction::integrityCheck()
{
    if (s_type == PostgreSQL) {
        // PostgreSQL: запрос к системным таблицам
        return "SELECT COUNT(*) FROM pg_class WHERE relkind = 'r';";
    } else {
        // SQLite: PRAGMA integrity_check
        return "PRAGMA integrity_check;";
    }
}

// ============================================================
// PRAGMA КОМАНДЫ
// ============================================================

QString DatabaseAbstraction::pragma(const QString& pragmaCommand)
{
    if (s_type == SQLite) {
        return "PRAGMA " + pragmaCommand + ";";
    } else if (s_type == PostgreSQL) {
        // PostgreSQL не использует PRAGMA - возвращаем пустую строку
        qDebug() << "⚠️ PRAGMA команда игнорирована для PostgreSQL:" << pragmaCommand;
        return QString();
    } else {
        return QString();
    }
}

// ============================================================
// СЛУЖЕБНЫЕ МЕТОДЫ
// ============================================================

QString DatabaseAbstraction::databaseTypeName()
{
    switch (s_type) {
        case SQLite:
            return "SQLite";
        case PostgreSQL:
            return "PostgreSQL";
        default:
            return "Unknown";
    }
}

bool DatabaseAbstraction::isSQLite()
{
    return s_type == SQLite;
}

bool DatabaseAbstraction::isPostgreSQL()
{
    return s_type == PostgreSQL;
}

QString DatabaseAbstraction::buildPlaceholders(int count)
{
    QStringList placeholders;
    for (int i = 0; i < count; ++i) {
        if (s_type == PostgreSQL) {
            placeholders << "$" + QString::number(i + 1);
        } else {
            placeholders << "?";
        }
    }
    return placeholders.join(", ");
}

QString DatabaseAbstraction::escapeIdentifier(const QString& identifier)
{
    if (s_type == PostgreSQL) {
        // PostgreSQL: двойные кавычки, но они делают идентификаторы регистрозависимыми
        // Лучше не использовать кавычки в PostgreSQL
        return identifier;
    } else {
        // SQLite: можно использовать [identifier] или "identifier"
        return identifier;
    }
}
