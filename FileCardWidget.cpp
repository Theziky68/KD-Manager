#include "FileCardWidget.h"
#include "ThemeManager.h"
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QPainter>
#include <QStyle>

FileCardWidget::FileCardWidget(const QString& filePath,
                               const QString& folderPath,
                               const QString& docStatus,
                               const QString& responsible,
                               QWidget* parent)
    : QFrame(parent)
    , m_filePath(filePath)
    , m_folderPath(folderPath)
    , m_docStatus(docStatus)
    , m_responsibleUser(responsible)
    , m_selected(false)
    , m_hovered(false)
{
    QFileInfo fi(filePath);
    m_fileName = fi.fileName();
    m_fileExt = fi.suffix().toLower();
    m_fileSize = fi.size();
    m_modified = fi.lastModified();

    setupUI();
    updateStyle();

    setFixedHeight(52);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

FileCardWidget::~FileCardWidget()
{
}


void FileCardWidget::setupUI()
{
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 6, 12, 6);
    mainLayout->setSpacing(12);

    // ===== ИКОНКА ФАЙЛА =====
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(32, 32);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 18px;"
        "   background: transparent;"
        "   border: none;"
        "}"
        );
    m_iconLabel->setText(getFileIcon(m_fileExt));
    mainLayout->addWidget(m_iconLabel);

    // ===== ИМЯ ФАЙЛА =====
    m_nameLabel = new QLabel(this);
    m_nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_nameLabel->setStyleSheet(
        QString(
            "QLabel {"
            "   font-size: 13px;"
            "   color: %1;"
            "   font-weight: 500;"
            "   font-family: -apple-system, 'Segoe UI', 'Arial', sans-serif;"
            "   background: transparent;"
            "   border: none;"
            "}"
        ).arg(ThemeManager::instance().textColor())
        );
    m_nameLabel->setText(m_fileName);
    m_nameLabel->setToolTip(m_fileName);
    mainLayout->addWidget(m_nameLabel, 1);

    // ===== ИНФОРМАЦИЯ: РАЗМЕР + ДАТА =====
    m_infoLabel = new QLabel(this);
    m_infoLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_infoLabel->setStyleSheet(
        QString(
            "QLabel {"
            "   font-size: 11px;"
            "   color: %1;"
            "   font-family: -apple-system, 'Segoe UI', 'Arial', sans-serif;"
            "   background: transparent;"
            "   border: none;"
            "}"
        ).arg(ThemeManager::instance().secondaryTextColor())
        );
    m_infoLabel->setText(formatFileSize(m_fileSize) + "  ·  " + m_modified.toString("dd.MM.yyyy"));
    mainLayout->addWidget(m_infoLabel);

    // ===== СТАТУС (цветной кружок) =====
    m_statusIndicator = new QLabel(this);
    m_statusIndicator->setFixedSize(12, 12);
    m_statusIndicator->setAlignment(Qt::AlignCenter);
    m_statusIndicator->setStyleSheet(getStatusStyle(m_docStatus));
    m_statusIndicator->setToolTip(m_docStatus);
    mainLayout->addWidget(m_statusIndicator);

    // ===== ОТВЕТСТВЕННЫЙ (если назначен) =====
    m_responsibleLabel = new QLabel(this);
    m_responsibleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_responsibleLabel->setStyleSheet(
        QString("QLabel { font-size: 10px; color: %1; border: none; }")
        .arg(ThemeManager::instance().secondaryTextColor())
    );
    if (!m_responsibleUser.isEmpty()) {
        m_responsibleLabel->setText("👤 " + m_responsibleUser.left(15));
        m_responsibleLabel->setToolTip(m_responsibleUser);
    } else {
        m_responsibleLabel->setText("");
    }
    mainLayout->addWidget(m_responsibleLabel);

    // ===== ИНДИКАТОР ВЫБОРА (как в Telegram галочка) =====
    m_checkLabel = new QLabel(this);
    m_checkLabel->setFixedSize(20, 20);
    m_checkLabel->setAlignment(Qt::AlignCenter);
    m_checkLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 14px;"
        "   color: #34c759;"
        "   background: transparent;"
        "   border: none;"
        "}"
        );
    m_checkLabel->setText("✓");
    m_checkLabel->setVisible(false);
    mainLayout->addWidget(m_checkLabel);
}

void FileCardWidget::updateStyle()
{
    ThemeManager& tm = ThemeManager::instance();
    QString style;

    if (m_selected) {
        style = QString(
            "QFrame {"
            "   background: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 10px;"
            "}"
            "QFrame:hover {"
            "   background: %1;"
            "   border: 1px solid %2;"
            "}"
        ).arg(tm.currentTheme() == ThemeManager::Dark ? "#1F4620" : "#e8f5e9",
              tm.currentTheme() == ThemeManager::Dark ? "#4CAF50" : "#66bb6a");
        m_checkLabel->setVisible(true);
    } else if (m_hovered) {
        style = QString(
            "QFrame {"
            "   background: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 10px;"
            "}"
            "QFrame:hover {"
            "   background: %1;"
            "   border: 1px solid %2;"
            "}"
        ).arg(tm.hoverColor(), tm.borderColor());
        m_checkLabel->setVisible(false);
    } else {
        style = QString(
            "QFrame {"
            "   background: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 10px;"
            "}"
            "QFrame:hover {"
            "   background: %3;"
            "   border: 1px solid %4;"
            "}"
        ).arg(tm.cardBackground(), tm.borderColor(),
              tm.hoverColor(), tm.borderColor());
        m_checkLabel->setVisible(false);
    }

    setStyleSheet(style);
}

QString FileCardWidget::getFileIcon(const QString& ext)
{
    if (ext == "m3d" || ext == "a3d") return "📦";
    if (ext == "sldprt") return "⚙️";
    if (ext == "sldasm") return "🔧";
    if (ext == "cdw" || ext == "frw") return "📐";
    if (ext == "dxf" || ext == "dwg") return "📏";
    if (ext == "spw") return "📋";
    if (ext == "step" || ext == "stp") return "🔄";
    if (ext == "pdf") return "📄";
    if (ext == "doc" || ext == "docx") return "📝";
    if (ext == "xls" || ext == "xlsx") return "📊";
    if (ext == "jpg" || ext == "jpeg" || ext == "png") return "🖼️";
    return "📎";
}

QString FileCardWidget::getStatusStyle(const QString& status)
{
    QString color = "#888888";

    if (status.contains("Утвержден", Qt::CaseInsensitive) || status.contains("Approved")) {
        color = "#34c759";
    } else if (status.contains("работе", Qt::CaseInsensitive) || status.contains("Progress")) {
        color = "#ff9500";
    } else if (status.contains("проверке", Qt::CaseInsensitive) || status.contains("Review")) {
        color = "#007aff";
    } else if (status.contains("доработки", Qt::CaseInsensitive) || status.contains("Rework")) {
        color = "#ff3b30";
    }

    return QString(
        "QLabel {"
        "   border-radius: 6px;"
        "   background-color: %1;"
        "   border: 1px solid %1;"
        "}"
    ).arg(color);
}

QString FileCardWidget::formatFileSize(qint64 size)
{
    if (size < 1024) return QString::number(size) + " B";
    if (size < 1048576) return QString::number(size / 1024) + " KB";
    if (size < 1073741824) return QString::number(size / 1048576) + " MB";
    return QString::number(size / 1073741824) + " GB";
}

void FileCardWidget::setSelected(bool selected)
{
    m_selected = selected;
    updateStyle();
}

void FileCardWidget::updateFileInfo()
{
    QFileInfo fi(m_filePath);
    m_fileSize = fi.size();
    m_modified = fi.lastModified();
    m_infoLabel->setText(formatFileSize(m_fileSize) + "  ·  " + m_modified.toString("dd.MM.yyyy"));
}

void FileCardWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_filePath);
    }
    QFrame::mousePressEvent(event);
}

void FileCardWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_filePath);
    }
    QFrame::mouseDoubleClickEvent(event);
}

void FileCardWidget::contextMenuEvent(QContextMenuEvent* event)
{
    emit rightClicked(event->globalPos(), m_filePath);
}

void FileCardWidget::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    updateStyle();
    emit mouseEntered(m_filePath);
    QFrame::enterEvent(event);
}

void FileCardWidget::leaveEvent(QEvent* event)
{
    m_hovered = false;
    updateStyle();
    emit mouseLeft();
    QFrame::leaveEvent(event);
}

void FileCardWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_hovered) {
        emit previewRequested(m_filePath, mapToGlobal(event->pos()));
    }
    QFrame::mouseMoveEvent(event);
}