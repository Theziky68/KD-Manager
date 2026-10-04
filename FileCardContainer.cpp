#include "FileCardContainer.h"
#include "FileCardWidget.h"
#include "DatabaseManager.h"
#include "FolderData.h"
#include "ThemeManager.h"
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QAction>
#include <QMenu>
#include <QScrollBar>
#include <QResizeEvent>

FileCardContainer::FileCardContainer(QWidget* parent)
    : QWidget(parent), m_selectedFile(""), m_db(nullptr), m_folderPath("")
{
    setupUI();
}

FileCardContainer::~FileCardContainer()
{
    clear();
}

void FileCardContainer::setupUI()
{
    ThemeManager& tm = ThemeManager::instance();

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet(QString(
        "QScrollArea {"
        "   background: %1;"
        "   border: none;"
        "}"
        "QScrollBar:vertical {"
        "   background: %1;"
        "   width: 6px;"
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: %2;"
        "   border-radius: 3px;"
        "   min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background: %3;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
    ).arg(tm.backgroundColor(),
          tm.currentTheme() == ThemeManager::Dark ? "#4B5563" : "#d0d0d0",
          tm.currentTheme() == ThemeManager::Dark ? "#6B7280" : "#b0b0b0"));

    m_containerWidget = new QWidget(m_scrollArea);
    m_containerWidget->setStyleSheet(QString("background: %1;").arg(tm.backgroundColor()));

    m_layout = new QVBoxLayout(m_containerWidget);
    m_layout->setContentsMargins(10, 8, 10, 8);
    m_layout->setSpacing(4);
    m_layout->setAlignment(Qt::AlignTop);

    m_scrollArea->setWidget(m_containerWidget);
    mainLayout->addWidget(m_scrollArea);
}

void FileCardContainer::setFiles(const QStringList& files)
{
    QString selectedFile = m_selectedFile;
    clear();

    if (files.isEmpty()) {
        QLabel* emptyLabel = new QLabel("📭 Нет файлов в этой папке", m_containerWidget);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QString(
            "QLabel {"
            "   color: %1;"
            "   font-size: 15px;"
            "   font-family: -apple-system, 'Segoe UI', 'Arial', sans-serif;"
            "   background: transparent;"
            "   padding: 40px;"
            "}"
        ).arg(ThemeManager::instance().secondaryTextColor()));
        m_layout->addWidget(emptyLabel);
        return;
    }

    QStringList modelExts = {".m3d", ".a3d", ".sldprt", ".sldasm"};
    QStringList drawingExts = {".cdw", ".frw", ".dxf", ".dwg", ".spw"};

    for (const QString& filePath : files) {
        QFileInfo fi(filePath);
        QString ext = "." + fi.suffix().toLower();

        if (modelExts.contains(ext)) {
            m_modelFiles.append(filePath);
        } else if (drawingExts.contains(ext)) {
            m_drawingFiles.append(filePath);
        } else {
            m_otherFiles.append(filePath);
        }
    }

    m_modelFiles.sort();
    m_drawingFiles.sort();
    m_otherFiles.sort();

    m_filePaths = files;

    for (const QString& filePath : files) {
        const QString docKey = m_folderPath + "|" + filePath;

        QString status = "Новый";
        QString responsible = "";

        if (m_db && m_db->documents().contains(docKey)) {
            const DocInfo& doc = m_db->documents()[docKey];
            status = doc.status.isEmpty() ? "Новый" : doc.status;
            responsible = doc.responsibleUser;
        }

        FileCardWidget* card = new FileCardWidget(
            filePath,
            m_folderPath,
            status,
            responsible,
            this
        );
        connect(card, &FileCardWidget::clicked, this, &FileCardContainer::onFileClicked);
        connect(card, &FileCardWidget::doubleClicked, this, &FileCardContainer::onFileDoubleClicked);
        connect(card, &FileCardWidget::rightClicked, this, &FileCardContainer::onFileRightClicked);
        m_cardWidgets[filePath] = card;
    }

    rebuildLayout();

    if (!selectedFile.isEmpty() && m_cardWidgets.contains(selectedFile)) {
        selectFile(selectedFile);
    }
}

void FileCardContainer::rebuildLayout()
{
    clearLayout();

    if (!m_modelFiles.isEmpty()) {
        addGroupHeader("3D модели", "🖥️", "#2196F3");
        for (const QString& filePath : m_modelFiles) {
            FileCardWidget* card = m_cardWidgets.value(filePath);
            if (card) m_layout->addWidget(card);
        }
        addSeparator();
    }

    if (!m_drawingFiles.isEmpty()) {
        addGroupHeader("Чертежи", "📐", "#F44336");
        for (const QString& filePath : m_drawingFiles) {
            FileCardWidget* card = m_cardWidgets.value(filePath);
            if (card) m_layout->addWidget(card);
        }
        addSeparator();
    }

    if (!m_otherFiles.isEmpty()) {
        addGroupHeader("Прочие файлы", "📎", "#8e8e93");
        for (const QString& filePath : m_otherFiles) {
            FileCardWidget* card = m_cardWidgets.value(filePath);
            if (card) m_layout->addWidget(card);
        }
    }
}

void FileCardContainer::clearLayout()
{
    QLayoutItem* child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
}

void FileCardContainer::addGroupHeader(const QString& title, const QString& icon, const QString& color)
{
    QWidget* headerWidget = new QWidget(m_containerWidget);
    headerWidget->setStyleSheet("background: transparent;");
    headerWidget->setFixedHeight(32);

    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(8);

    QLabel* iconLabel = new QLabel(icon, headerWidget);
    iconLabel->setStyleSheet("QLabel { font-size: 14px; background: transparent; border: none; }");
    headerLayout->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel(title, headerWidget);
    titleLabel->setStyleSheet(QString(
        "QLabel { font-size: 13px; font-weight: 600; color: %1; "
        "font-family: -apple-system, 'Segoe UI', 'Arial', sans-serif; "
        "background: transparent; border: none; }"
    ).arg(color));
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    int count = 0;
    if (title == "3D модели") count = m_modelFiles.size();
    else if (title == "Чертежи") count = m_drawingFiles.size();
    else if (title == "Прочие файлы") count = m_otherFiles.size();

    QLabel* countLabel = new QLabel(QString::number(count), headerWidget);
    countLabel->setStyleSheet("QLabel { font-size: 11px; color: #8e8e93; "
        "font-family: -apple-system, 'Segoe UI', 'Arial', sans-serif; background: transparent; border: none; }");
    headerLayout->addWidget(countLabel);

    m_layout->addWidget(headerWidget);
}

void FileCardContainer::addSeparator()
{
    QFrame* separator = new QFrame(m_containerWidget);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("background: #e8e8e8; max-height: 1px; margin: 6px 4px;");
    m_layout->addWidget(separator);
}

void FileCardContainer::addFile(const QString& filePath)
{
    if (m_cardWidgets.contains(filePath)) return;

    const QString docKey = m_folderPath + "|" + filePath;

    QString status = "Новый";
    QString responsible = "";

    if (m_db && m_db->documents().contains(docKey)) {
        const DocInfo& doc = m_db->documents()[docKey];
        status = doc.status.isEmpty() ? "Новый" : doc.status;
        responsible = doc.responsibleUser;
    }

    FileCardWidget* card = new FileCardWidget(
        filePath,
        m_folderPath,
        status,
        responsible,
        this
    );
    connect(card, &FileCardWidget::clicked, this, &FileCardContainer::onFileClicked);
    connect(card, &FileCardWidget::doubleClicked, this, &FileCardContainer::onFileDoubleClicked);
    connect(card, &FileCardWidget::rightClicked, this, &FileCardContainer::onFileRightClicked);

    m_cardWidgets[filePath] = card;
    m_filePaths.append(filePath);

    QFileInfo fi(filePath);
    QString ext = "." + fi.suffix().toLower();

    QStringList modelExts = {".m3d", ".a3d", ".sldprt", ".sldasm"};
    QStringList drawingExts = {".cdw", ".frw", ".dxf", ".dwg", ".spw"};

    if (modelExts.contains(ext)) {
        m_modelFiles.append(filePath);
        m_modelFiles.sort();
    } else if (drawingExts.contains(ext)) {
        m_drawingFiles.append(filePath);
        m_drawingFiles.sort();
    } else {
        m_otherFiles.append(filePath);
        m_otherFiles.sort();
    }

    rebuildLayout();
}

void FileCardContainer::removeFile(const QString& filePath)
{
    if (!m_cardWidgets.contains(filePath)) return;

    FileCardWidget* card = m_cardWidgets[filePath];
    m_cardWidgets.remove(filePath);
    m_filePaths.removeAll(filePath);
    m_modelFiles.removeAll(filePath);
    m_drawingFiles.removeAll(filePath);
    m_otherFiles.removeAll(filePath);

    card->deleteLater();

    if (m_selectedFile == filePath) m_selectedFile.clear();

    rebuildLayout();
}

void FileCardContainer::clear()
{
    for (FileCardWidget* card : m_cardWidgets.values()) card->deleteLater();
    m_cardWidgets.clear();
    m_filePaths.clear();
    m_modelFiles.clear();
    m_drawingFiles.clear();
    m_otherFiles.clear();
    m_selectedFile.clear();
    clearLayout();
}

void FileCardContainer::selectFile(const QString& filePath)
{
    deselectAll();
    if (m_cardWidgets.contains(filePath)) {
        m_cardWidgets[filePath]->setSelected(true);
        m_selectedFile = filePath;
        emit fileSelected(filePath);
    }
}

void FileCardContainer::deselectAll()
{
    for (FileCardWidget* card : m_cardWidgets.values()) card->setSelected(false);
    m_selectedFile.clear();
}

QStringList FileCardContainer::getSelectedFiles() const
{
    QStringList selected;
    for (FileCardWidget* card : m_cardWidgets.values()) {
        if (card->isSelected()) selected.append(card->getFilePath());
    }
    return selected;
}

void FileCardContainer::selectAll()
{
    for (FileCardWidget* card : m_cardWidgets.values()) card->setSelected(true);
}

void FileCardContainer::onFileClicked(const QString& filePath)
{
    selectFile(filePath);
}

void FileCardContainer::onFileDoubleClicked(const QString& filePath)
{
    emit fileDoubleClicked(filePath);
}

void FileCardContainer::onFileRightClicked(const QPoint& pos, const QString& filePath)
{
    selectFile(filePath);

    QMenu menu(this);
    menu.setStyleSheet("QMenu { background: #ffffff; border: 1px solid #e0e0e0; border-radius: 12px; padding: 4px; }"
        "QMenu::item { padding: 8px 24px; border-radius: 8px; color: #1a1a1a; font-family: -apple-system, 'Segoe UI', 'Arial', sans-serif; font-size: 13px; }"
        "QMenu::item:selected { background: #e8f5e9; color: #1a1a1a; }"
        "QMenu::separator { height: 1px; background: #e8e8e8; margin: 4px 8px; }");

    QAction* actionOpen = new QAction("📂 Открыть", this);
    connect(actionOpen, &QAction::triggered, [filePath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    });
    menu.addAction(actionOpen);

    QAction* actionComment = new QAction("💬 Комментарий", this);
    connect(actionComment, &QAction::triggered, [this, filePath]() {
        emit fileCommentRequested(filePath);
    });
    menu.addAction(actionComment);

    menu.addSeparator();

    QAction* actionDelete = new QAction("🗑️ Удалить", this);
    connect(actionDelete, &QAction::triggered, [this, filePath]() {
        if (QMessageBox::question(this, "Подтверждение",
            "Удалить файл '" + QFileInfo(filePath).fileName() + "'?",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            emit fileDeleted(filePath);
        }
    });
    menu.addAction(actionDelete);

    menu.exec(pos);
}

void FileCardContainer::scrollToFile(const QString& filePath)
{
    if (!m_cardWidgets.contains(filePath)) return;

    FileCardWidget* card = m_cardWidgets[filePath];
    if (!card) return;

    QPoint pos = card->mapTo(m_containerWidget, QPoint(0, 0));
    m_scrollArea->ensureVisible(pos.x(), pos.y(), 0, 50);
}
