#include "FolderData.h"
#include <QFileInfo>

QString folderTypeName(FolderType type)
{
    switch (type) {
    case TYPE_PRODUCT: return "📦 [Изделие]";
    case TYPE_ASSEMBLY: return "🔧 [СБ]";
    case TYPE_DETAIL: return "⚙️ [Деталь]";
    case TYPE_SPEC: return "📋 [Спец]";
    case TYPE_3D: return "🖥️ [3D]";
    case TYPE_DRAWING: return "📐 [Чертеж]";
    case TYPE_ROOT_FOLDER: return "📁 [Раздел]";
    case TYPE_REFERENCE: return "📚 [Справочник]";
    case TYPE_ARCHIVE: return "📦 [Архив]";
    case TYPE_TECH_PROCESS: return "⚙️ [Техпроцесс]";
    case TYPE_OTHER: return "📎 [Другое]";
    default: return "📁 [Папка]";
    }
}

QString getSectionName(FolderType type)
{
    switch (type) {
    case TYPE_PRODUCT: return "Изделия";
    case TYPE_REFERENCE: return "Справочники";
    case TYPE_ARCHIVE: return "Архив";
    case TYPE_TECH_PROCESS: return "Техпроцессы";
    case TYPE_OTHER: return "Другое";
    default: return "";
    }
}

QString fileSizeStr(qint64 size)
{
    if (size < 1024) return QString::number(size) + " B";
    if (size < 1048576) return QString::number(size / 1024) + " KB";
    return QString::number(size / 1048576) + " MB";
}

QString fileExt(const QString& fileName)
{
    int pos = fileName.lastIndexOf('.');
    return (pos != -1) ? fileName.mid(pos).toLower() : "";
}

// FolderData.cpp

bool isFileAllowedForFolder(const QString& filePath, FolderType folderType)
{
    // ===== НЕ ПРОПУСКАЕМ .bak ФАЙЛЫ =====
    if (filePath.endsWith(".bak", Qt::CaseInsensitive)) {
        return false;  // ❌ Бэкапы Компаса не добавляем
    }

    // ===== ДЛЯ СПЕЦИАЛЬНЫХ ПАПОК РАЗРЕШАЕМ ВСЕ ФАЙЛЫ (КРОМЕ .bak) =====
    if (folderType == TYPE_REFERENCE ||
        folderType == TYPE_ARCHIVE ||
        folderType == TYPE_TECH_PROCESS ||
        folderType == TYPE_OTHER ||
        folderType == TYPE_ROOT_FOLDER) {
        return true;
    }

    QString ext = fileExt(filePath);

    bool isDrawing = (ext == ".cdw" || ext == ".frw");
    bool isSpec = (ext == ".spw");
    bool is3D = (ext == ".m3d" || ext == ".a3d" || ext == ".sldprt" ||
                 ext == ".sldasm" || ext == ".step" || ext == ".stp");
    bool isCAD = isDrawing || isSpec || is3D || (ext == ".dxf") || (ext == ".dwg");

    if (!isCAD) return false;

    switch (folderType) {
    case TYPE_PRODUCT:
        return isCAD;

    case TYPE_ASSEMBLY:
        if (isDrawing) return true;
        if (isSpec) return true;
        if (ext == ".a3d" || ext == ".sldasm") return true;
        if (ext == ".m3d" || ext == ".sldprt") return false;
        return false;

    case TYPE_DETAIL:
        if (isDrawing) return true;
        if (isSpec) return false;
        if (ext == ".m3d" || ext == ".sldprt") return true;
        if (ext == ".a3d" || ext == ".sldasm") return false;
        return false;

    case TYPE_SPEC:
        return isSpec;

    case TYPE_3D:
        return is3D;

    default:
        return isCAD;
    }
}