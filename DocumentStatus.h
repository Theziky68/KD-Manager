#ifndef DOCUMENTSTATUS_H
#define DOCUMENTSTATUS_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QMap>

// ==================== СТАТУСЫ ДОКУМЕНТОВ ====================
enum DocumentStatus {
    STATUS_NEW = 0,             // ⚪ Новый (не назначено)
    STATUS_APPROVED,            // ✅ Утвержден
    STATUS_IN_PROGRESS,         // 🔄 В работе
    STATUS_REVIEW,              // 👀 На проверке
    STATUS_NEEDS_CHANGE,        // 🔧 Требует доработки
    STATUS_OBSOLETE,            // ⚠️ Устарел
    STATUS_ARCHIVED             // 📦 В архиве
};

// ==================== СТРУКТУРА ЗАПИСИ ОБ ИЗМЕНЕНИИ ====================
struct ChangeRecord {
    QDateTime timestamp;
    QString user;
    QString action;              // "Добавлен", "Изменен", "Удален", "Переименован"
    QString description;         // Что именно изменилось
    QString filePath;
    QString oldValue;
    QString newValue;

    ChangeRecord() {}

    ChangeRecord(const QString& user, const QString& action, const QString& description,
                 const QString& filePath = QString())
        : timestamp(QDateTime::currentDateTime())
        , user(user)
        , action(action)
        , description(description)
        , filePath(filePath)
    {}
};

// ==================== СТРУКТУРА ДОКУМЕНТА С СОСТОЯНИЕМ ====================
struct DocumentState {
    QString filePath;
    DocumentStatus status;
    QString responsibleUser;      // Ответственный
    QString reviewerUser;         // Проверяющий
    QDateTime lastModified;
    QString lastModifiedBy;
    QString changeComment;        // Комментарий об изменении
    QList<ChangeRecord> changeHistory;

    DocumentState() : status(STATUS_NEW) {}

    void addChange(const ChangeRecord& record) {
        changeHistory.append(record);
        lastModified = record.timestamp;
        lastModifiedBy = record.user;
        changeComment = record.description;
    }

    QString statusToString() const {
        switch(status) {
        case STATUS_NEW: return "⚪ Новый";
        case STATUS_APPROVED: return "✅ Утвержден";
        case STATUS_IN_PROGRESS: return "🔄 В работе";
        case STATUS_REVIEW: return "👀 На проверке";
        case STATUS_NEEDS_CHANGE: return "🔧 Требует доработки";
        case STATUS_OBSOLETE: return "⚠️ Устарел";
        case STATUS_ARCHIVED: return "📦 В архиве";
        default: return "❓ Неизвестно";
        }
    }

    QString statusToColor() const {
        switch(status) {
        case STATUS_NEW: return "#9E9E9E";           // Серый (не назначено)
        case STATUS_APPROVED: return "#4CAF50";      // Зеленый
        case STATUS_IN_PROGRESS: return "#FF9800";   // Оранжевый
        case STATUS_REVIEW: return "#2196F3";        // Синий
        case STATUS_NEEDS_CHANGE: return "#F44336";  // Красный
        case STATUS_OBSOLETE: return "#9E9E9E";      // Серый
        case STATUS_ARCHIVED: return "#607D8B";      // Сине-серый
        default: return "#000000";
        }
    }

    static QStringList getAllStatuses() {
        return QStringList()
        << "Новый"
        << "Утвержден"
        << "В работе"
        << "На проверке"
        << "Требует доработки"
        << "Устарел"
        << "В архиве";
    }

    static DocumentStatus stringToStatus(const QString& str) {
        if (str == "Новый") return STATUS_NEW;
        if (str == "Утвержден") return STATUS_APPROVED;
        if (str == "В работе") return STATUS_IN_PROGRESS;
        if (str == "На проверке") return STATUS_REVIEW;
        if (str == "Требует доработки") return STATUS_NEEDS_CHANGE;
        if (str == "Устарел") return STATUS_OBSOLETE;
        if (str == "В архиве") return STATUS_ARCHIVED;
        return STATUS_NEW;
    }
};

#endif // DOCUMENTSTATUS_H