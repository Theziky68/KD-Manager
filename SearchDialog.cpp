#include "SearchDialog.h"
#include "MainWindow.h"
#include "TreeItemWidget.h"
#include "ThemeManager.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QFileInfo>
#include <QApplication>
#include <QPalette>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QScrollArea>

SearchDialog::SearchDialog(QWidget *parent)
    : QDialog(parent)
    , m_db(nullptr)
    , m_mainWindow(nullptr)
    , m_searchTimer(nullptr)
{
    setupUI();
    setWindowTitle("🔍 Поиск файлов");
    resize(650, 500);
    setModal(false);

    // Создаём таймер для debounce поиска
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    connect(m_searchTimer, &QTimer::timeout, this, &SearchDialog::onSearch);

    applyTheme();
}

void SearchDialog::applyTheme()
{
    ThemeManager& tm = ThemeManager::instance();
    bool isDark = (tm.currentTheme() == ThemeManager::Dark);

    QString bgColor = isDark ? "#1E1E1E" : "#f5f5f5";
    QString textColor = isDark ? "#E0E0E0" : "#37474F";
    QString cardBg = isDark ? "#2A2A2A" : "white";
    QString borderColor = isDark ? "#3A3A3A" : "#e0e0e0";
    QString inputBg = isDark ? "#333333" : "white";
    QString hoverBg = isDark ? "#3A3A3A" : "#e3f2fd";
    QString selectedBg = isDark ? "#0084ff" : "#d5dce0";
    QString placeholderColor = isDark ? "#666666" : "#90A4AE";
    QString btnHoverBg = isDark ? "rgba(100,100,100,0.2)" : "rgba(96,125,139,0.1)";

    setStyleSheet(QString(
        "QDialog {"
        "   background-color: %1;"
        "}"
        "QLabel {"
        "   color: %2;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   background: transparent;"
        "}"
        "QLineEdit {"
        "   background-color: %3;"
        "   border: 1px solid %4;"
        "   border-radius: 6px;"
        "   padding: 8px 12px;"
        "   color: %2;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "   border: 2px solid #0084ff;"
        "}"
        "QLineEdit::placeholder {"
        "   color: %5;"
        "}"
        "QPushButton {"
        "   background: #0084ff;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 6px;"
        "   padding: 8px 20px;"
        "   font-size: 13px;"
        "   font-weight: 500;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QPushButton:hover {"
        "   background: #0073e6;"
        "}"
        "QPushButton:pressed {"
        "   background: #0063cc;"
        "}"
        "QPushButton#closeBtn {"
        "   background: transparent;"
        "   color: %2;"
        "   border: 1px solid %4;"
        "}"
        "QPushButton#closeBtn:hover {"
        "   background: %6;"
        "}"
        "QListWidget {"
        "   background-color: %3;"
        "   border: 1px solid %4;"
        "   border-radius: 8px;"
        "   font-size: 13px;"
        "   color: %2;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   outline: none;"
        "}"
        "QListWidget::item {"
        "   padding: 10px 14px;"
        "   border-bottom: 1px solid %4;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: %7;"
        "}"
        "QListWidget::item:selected {"
        "   background: %8;"
        "   color: white;"
        "}"
        "QListWidget::item:selected:hover {"
        "   background: %8;"
        "}"
        "QScrollBar:vertical {"
        "   background: transparent;"
        "   width: 6px;"
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: %4;"
        "   border-radius: 3px;"
        "   min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background: %9;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
    ).arg(bgColor, textColor, inputBg, borderColor, placeholderColor, btnHoverBg,
          hoverBg, selectedBg, isDark ? "#555555" : "#b0b0b0"));
}

void SearchDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QLabel* titleLabel = new QLabel("🔍 Поиск файлов", this);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 18px;"
        "   font-weight: 600;"
        "   color: #263238;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   background: transparent;"
        "   padding-bottom: 4px;"
        "}"
        );
    mainLayout->addWidget(titleLabel);

    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->setSpacing(10);

    QLabel* searchIcon = new QLabel("🔍", this);
    searchIcon->setStyleSheet("background: transparent; font-size: 16px;");
    searchLayout->addWidget(searchIcon);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Введите имя файла для поиска...");
    m_searchEdit->setMinimumHeight(38);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &SearchDialog::onSearch);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SearchDialog::onSearchTextChanged);
    searchLayout->addWidget(m_searchEdit);

    m_searchBtn = new QPushButton("Найти", this);
    m_searchBtn->setMinimumWidth(80);
    m_searchBtn->setMinimumHeight(38);
    connect(m_searchBtn, &QPushButton::clicked, this, &SearchDialog::onSearch);
    searchLayout->addWidget(m_searchBtn);

    m_clearBtn = new QPushButton("✖", this);
    m_clearBtn->setFixedSize(38, 38);
    m_clearBtn->setStyleSheet(
        "QPushButton {"
        "   background: transparent;"
        "   color: #607D8B;"
        "   border: 1px solid #d0d0d0;"
        "   border-radius: 6px;"
        "   font-size: 14px;"
        "   padding: 0px;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(96,125,139,0.1);"
        "}"
        );
    connect(m_clearBtn, &QPushButton::clicked, this, &SearchDialog::onClear);
    searchLayout->addWidget(m_clearBtn);

    mainLayout->addLayout(searchLayout);

    m_resultsList = new QListWidget(this);
    m_resultsList->setMinimumHeight(250);
    m_resultsList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_resultsList, &QListWidget::itemDoubleClicked,
            this, &SearchDialog::onItemDoubleClicked);
    connect(m_resultsList, &QListWidget::itemClicked,
            this, &SearchDialog::onItemClicked);
    connect(m_resultsList, &QListWidget::customContextMenuRequested,
            this, &SearchDialog::showContextMenu);
    mainLayout->addWidget(m_resultsList);

    m_statusLabel = new QLabel("💡 Введите текст для поиска", this);
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "   color: #90A4AE;"
        "   padding: 4px 8px;"
        "   font-size: 12px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   background: transparent;"
        "}"
        );
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    m_closeBtn = new QPushButton("Закрыть", this);
    m_closeBtn->setObjectName("closeBtn");
    m_closeBtn->setMinimumWidth(100);
    m_closeBtn->setMinimumHeight(36);
    connect(m_closeBtn, &QPushButton::clicked, this, &SearchDialog::onClose);
    buttonLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(buttonLayout);

    m_searchEdit->setFocus();
}

void SearchDialog::setMainWindow(MainWindow* mainWindow)
{
    m_mainWindow = mainWindow;
}

void SearchDialog::openFile(const QString& filePath, const QString& folderPath)
{
    if (!filePath.isEmpty() && !folderPath.isEmpty()) {
        emit fileSelected(filePath, folderPath);

        if (m_mainWindow) {
            QMetaObject::invokeMethod(m_mainWindow, "showWorkPageAndSelectFile",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, filePath),
                                      Q_ARG(QString, folderPath));
        }
    }
}

void SearchDialog::showContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_resultsList->itemAt(pos);
    if (!item) return;

    QString filePath = item->data(Qt::UserRole).toString();
    QString folderPath = item->data(Qt::UserRole + 1).toString();

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "   background-color: white;"
        "   border: 1px solid #e0e0e0;"
        "   border-radius: 8px;"
        "   padding: 4px;"
        "}"
        "QMenu::item {"
        "   padding: 8px 24px;"
        "   border-radius: 4px;"
        "   color: #37474F;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   font-size: 13px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: #e8edf0;"
        "}"
        "QMenu::separator {"
        "   height: 1px;"
        "   background: #e0e0e0;"
        "   margin: 4px 8px;"
        "}"
        );

    QAction* actionOpen = new QAction("📂 Открыть файл", this);
    connect(actionOpen, &QAction::triggered, [this, filePath, folderPath]() {
        openFile(filePath, folderPath);
    });
    menu.addAction(actionOpen);

    QAction* actionOpenFolder = new QAction("📁 Открыть папку", this);
    connect(actionOpenFolder, &QAction::triggered, [folderPath]() {
        if (!folderPath.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
        }
    });
    menu.addAction(actionOpenFolder);

    if (QFile::exists(filePath)) {
        menu.addSeparator();
        QAction* actionOpenExternal = new QAction("🚀 Открыть внешней программой", this);
        connect(actionOpenExternal, &QAction::triggered, [filePath]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        });
        menu.addAction(actionOpenExternal);
    }

    menu.exec(m_resultsList->mapToGlobal(pos));
}

void SearchDialog::onSearchTextChanged(const QString& text)
{
    if (m_searchTimer) {
        m_searchTimer->start(300);
    }
}

void SearchDialog::onSearch()
{
    if (!m_db) {
        QMessageBox::warning(this, "Ошибка", "База данных не инициализирована!");
        return;
    }

    QString searchText = m_searchEdit->text().trimmed();
    if (searchText.isEmpty()) {
        m_resultsList->clear();
        m_statusLabel->setText("💡 Введите текст для поиска");
        return;
    }

    m_resultsList->clear();
    m_statusLabel->setText("⏳ Поиск...");

    int foundCount = 0;
    QString searchLower = searchText.toLower();

    QApplication::processEvents();

    for (auto it = m_db->folders().begin(); it != m_db->folders().end(); ++it) {
        if (it.key().isEmpty()) continue;

        for (const QString& filePath : it.value().files) {
            QFileInfo fi(filePath);
            QString fileName = fi.fileName().toLower();

            if (fileName.contains(searchLower)) {
                QListWidgetItem* item = new QListWidgetItem(m_resultsList);

                QString displayText = "📄 " + fi.fileName();
                item->setText(displayText);
                item->setData(Qt::UserRole, filePath);
                item->setData(Qt::UserRole + 1, it.key());

                QString folderDisplay = it.value().name;
                item->setToolTip("Папка: " + folderDisplay + "\nПуть: " + filePath);

                item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));

                foundCount++;
            }
        }
    }

    if (foundCount == 0) {
        m_statusLabel->setText("❌ Файлы не найдены по запросу: \"" + searchText + "\"");
        QListWidgetItem* emptyItem = new QListWidgetItem(m_resultsList);
        emptyItem->setText("📭 Нет результатов");
        emptyItem->setTextAlignment(Qt::AlignCenter);
        emptyItem->setForeground(QColor("#90A4AE"));
        m_resultsList->addItem(emptyItem);
    } else {
        m_statusLabel->setText(QString("✅ Найдено файлов: %1").arg(foundCount));
    }
}

void SearchDialog::onItemClicked(QListWidgetItem* item)
{
    if (!item) return;

    QString folderPath = item->data(Qt::UserRole + 1).toString();
    if (!folderPath.isEmpty()) {
        highlightInTree(folderPath);
    }
}

void SearchDialog::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;

    QString filePath = item->data(Qt::UserRole).toString();
    QString folderPath = item->data(Qt::UserRole + 1).toString();

    if (!filePath.isEmpty() && !folderPath.isEmpty()) {
        openFile(filePath, folderPath);
    }
}

void SearchDialog::highlightInTree(const QString& path)
{
    if (!m_mainWindow) return;

    QList<TreeItemWidget*> widgets = m_mainWindow->findChildren<TreeItemWidget*>();
    for (TreeItemWidget* widget : widgets) {
        QString itemPath = widget->getFolderPath();
        if (itemPath == path) {
            widget->setSelected(true);

            QScrollArea* scrollArea = m_mainWindow->findChild<QScrollArea*>("treeScrollArea");
            if (scrollArea) {
                scrollArea->ensureWidgetVisible(widget);
            }
            break;
        }
    }
}

void SearchDialog::onClear()
{
    m_searchEdit->clear();
    m_resultsList->clear();
    m_statusLabel->setText("💡 Введите текст для поиска");
    m_searchEdit->setFocus();
}

void SearchDialog::onClose()
{
    hide();
}