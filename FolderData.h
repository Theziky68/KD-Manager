// FolderData.h
#ifndef FOLDERDATA_H
#define FOLDERDATA_H

#include <QString>
#include <QDateTime>
#include <QVector>
#include <QMap>
#include <QList>

enum FolderType {
    TYPE_ROOT,
    TYPE_PRODUCT,
    TYPE_ASSEMBLY,
    TYPE_DETAIL,
    TYPE_FOLDER,
    TYPE_SPEC,
    TYPE_3D,
    TYPE_DRAWING,
    TYPE_ROOT_FOLDER,
    TYPE_REFERENCE,
    TYPE_ARCHIVE,       // <-- УЖЕ ЕСТЬ
    TYPE_TECH_PROCESS,  // <-- УЖЕ ЕСТЬ
    TYPE_OTHER          // <-- УЖЕ ЕСТЬ
};

struct FolderData {
    QString name;
    FolderType type;
    QVector<QString> files;

    FolderData() : type(TYPE_FOLDER) {}
};

struct Assignment {
    QString assignedTo;          // Кому назначено
    QString assignedBy;          // Кто назначил
    QDateTime assignedDate;      // Когда назначено
    QString previousAssignee;    // Предыдущий ответственный
    QString comment;             // Комментарий к назначению

    Assignment() {}
    Assignment(const QString& to, const QString& by, const QDateTime& date,
               const QString& prev = QString(), const QString& cmt = QString())
        : assignedTo(to), assignedBy(by), assignedDate(date),
          previousAssignee(prev), comment(cmt) {}
};

struct StatusChange {
    QString oldStatus;           // Старый статус
    QString newStatus;           // Новый статус
    QString changedBy;           // Кто изменил
    QDateTime changedDate;       // Когда изменено
    QString reason;              // Причина изменения

    StatusChange() {}
    StatusChange(const QString& old, const QString& newS, const QString& by,
                 const QDateTime& date, const QString& rsn = QString())
        : oldStatus(old), newStatus(newS), changedBy(by), changedDate(date), reason(rsn) {}
};

struct DocInfo {
    QString path;
    QString name;
    QString folderPath;                       // НОВОЕ: путь папки (для правильной идентификации)
    QString designation;
    QString material;
    QString mass;
    QString scale;
    QString format;
    QString status;
    QString comment;
    QString responsibleUser;
    qint64 size;
    QDateTime modified;
    bool isOpen;
    QList<Assignment> assignmentHistory;      // История назначений
    QList<StatusChange> statusHistory;        // История изменений статуса

    DocInfo() : size(0), isOpen(false) {}
};

struct SearchResultItem {
    QString fileName;
    QString folderPath;
    QString fullPath;
    int fileIndex;

    SearchResultItem() : fileIndex(-1) {}
};

// Объявления функций
QString folderTypeName(FolderType type);
QString fileSizeStr(qint64 size);
QString fileExt(const QString& fileName);
bool isFileAllowedForFolder(const QString& filePath, FolderType folderType);
QString getSectionName(FolderType type);

#endif // FOLDERDATA_H