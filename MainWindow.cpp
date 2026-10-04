#include "MainWindow.h"
#include "SettingsDialog.h"
#include "ChangesPanel.h"
#include "CommentDialog.h"
#include "SearchDialog.h"
#include "TasksDialog.h"
#include "PreviewWidget.h"
#include "NotificationManager.h"
#include "FolderData.h"
#include "StartupPage.h"
#include "ProductTreePage.h"
#include "PinDialog.h"
#include "AddFilesDialog.h"
#include "AboutDialog.h"
#include "TreeItemWidget.h"
#include "ThemeManager.h"
#include "UserConfig.h"
#include "OnlineUsersPanel.h"
#include "OnlineUsersManager.h"
#include <QGraphicsDropShadowEffect>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QSplitter>
#include <QRegularExpression>
#include <QMessageBox>
#include <QInputDialog>
#include <QStatusBar>
#include <QLineEdit>
#include <QMenuBar>
#include <QStyle>
#include <QHeaderView>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDockWidget>
#include <QApplication>
#include <QToolBar>
#include <QToolButton>
#include <QIcon>
#include <QStackedWidget>
#include <QLabel>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QElapsedTimer>
#include <QSettings>
#include <QPainter>
#include <QScreen>

// ============================================================
// КОНСТРУКТОР / ДЕСТРУКТОР
// ============================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_db(DatabaseManager::instance())
    , m_changesPanel(nullptr)
    , m_searchDialog(nullptr)
    , m_isOnStartupPage(true)
    , m_aboutDialog(nullptr)
    , m_notificationPanel(nullptr)
    , m_notificationDock(nullptr)
    , m_onlineUsersPanel(nullptr)
    , m_productTreePage(nullptr)
    , m_isRefreshing(false)
    , m_isPanelVisible(false)
    , m_panelAnimation(nullptr)
    , m_saveAnimationDots(0)
    , m_statusIndicator(nullptr)
    , m_batchUpdate(false)
    , m_saveAnimationTimer(nullptr)
    , m_isUpdatingTree(false)
{
    qDebug() << "🔍 MainWindow::MainWindow - начало конструктора";

    // Применяем тему при старте
    qDebug() << "🔍 Применяем тему...";
    ThemeManager::instance().setTheme(ThemeManager::instance().currentTheme());
    qDebug() << "✅ Тема применена";

    qDebug() << "🔍 Вызываем setupUI()...";
    setupUI();
    qDebug() << "✅ setupUI() завершён";

    qDebug() << "🔍 Вызываем refreshAll()...";
    refreshAll();
    qDebug() << "✅ refreshAll() завершён";

    qDebug() << "🔍 Вызываем updateStatus()...";
    updateStatus();
    qDebug() << "✅ updateStatus() завершён";

    qDebug() << "🔍 Вызываем loadStartupPage()...";
    loadStartupPage();
    qDebug() << "✅ loadStartupPage() завершён";

    // ===== ВОССТАНОВЛЕНИЕ ГЕОМЕТРИИ ОКНА =====
    qDebug() << "🔍 Восстанавливаем геометрию окна...";
    UserConfig& config = UserConfig::instance();
    QRect geometry = config.getWindowGeometry();
    qDebug() << "📐 Сохранённая геометрия:" << geometry;

    // Проверяем, не за границами ли экрана геометрия
    QRect screenGeometry = QApplication::primaryScreen()->availableGeometry();
    qDebug() << "📺 Доступная геометрия экрана:" << screenGeometry;

    bool wasMaximized = config.isWindowMaximized();
    qDebug() << "🔍 Было ли окно развёрнуто:" << wasMaximized;

    // СНАЧАЛА показываем окно
    if (wasMaximized) {
        qDebug() << "✅ Развёртываем окно (showMaximized)";
        showMaximized();
    } else {
        qDebug() << "✅ Показываем окно через show()";
        show();
    }

    // ПОТОМ восстанавливаем геометрию (если не максимизировано)
    if (!wasMaximized && geometry.width() > 0 && geometry.height() > 0 && screenGeometry.intersects(geometry)) {
        qDebug() << "✅ Восстанавливаем геометрию:" << geometry;
        setGeometry(geometry);
    } else if (!wasMaximized) {
        qDebug() << "⚠️ Геометрия за границами экрана, используем размеры по умолчанию";
        int width = std::min(1400, screenGeometry.width() - 50);
        int height = std::min(800, screenGeometry.height() - 50);
        int x = (screenGeometry.width() - width) / 2;
        int y = (screenGeometry.height() - height) / 2;
        setGeometry(x, y, width, height);
    }

    QByteArray splitterState = config.getSplitterState();
    if (!splitterState.isEmpty()) {
        // Восстанавливаем состояние сплиттера (если он используется)
    }
    qDebug() << "✅ Геометрия восстановлена";

    connect(m_searchBtn, &QToolButton::clicked, this, &MainWindow::onSearch);
    connect(&m_db, &DatabaseManager::databaseChanged, this, &MainWindow::refreshAll);

    connect(&m_db, &DatabaseManager::databaseReloaded, this, [this]() {
        refreshAll();
        updateStatusIndicator("🟢 БД: обновлена", "#4CAF50");
        statusBar()->showMessage("✅ База данных автоматически обновлена", 3000);
    });

    connect(&m_db, &DatabaseManager::databaseReloadFailed, this, [this](const QString& error) {
        updateStatusIndicator("🔴 Ошибка БД: " + error, "#f44336");
        statusBar()->showMessage("❌ Ошибка обновления БД: " + error, 5000);

        NotificationManager::Notification notif;
        notif.type = NotificationManager::TYPE_WARNING;
        notif.title = "⚠️ Ошибка обновления БД";
        notif.message = "Ошибка: " + error;
        notif.timestamp = QDateTime::currentDateTime();
        notif.isRead = false;
        notif.isImportant = true;
        notif.sourceUser = "System";
        NotificationManager::instance().addNotification("", notif);
    });

    connect(&m_db, &DatabaseManager::saveFinished, this, [this](bool success) {
        m_saveAnimationTimer->stop();
        if (success) {
            updateStatusIndicator("🟢 БД: сохранена", "#4CAF50");
            statusBar()->showMessage("✅ База данных сохранена", 2000);
        } else {
            updateStatusIndicator("🔴 Ошибка сохранения!", "#f44336");
            statusBar()->showMessage("❌ Ошибка сохранения базы данных!", 5000);
        }
    });

    connect(&m_db, &DatabaseManager::reloadStatusChanged, this, [this](bool enabled) {
        if (enabled) {
            updateStatusIndicator("🟢 БД: онлайн | Автообновление: вкл", "#4CAF50");
        } else {
            updateStatusIndicator("🟡 БД: онлайн | Автообновление: выкл", "#FF9800");
        }
    });

    m_db.enableAutoReload(true);

    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::onAutoSave);
    m_autoSaveTimer->start(300000);

    m_autoRefreshTimer = new QTimer(this);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &MainWindow::onAutoRefresh);
    m_autoRefreshTimer->start(300000);

    m_saveAnimationTimer = new QTimer(this);
    connect(m_saveAnimationTimer, &QTimer::timeout, this, [this]() {
        m_saveAnimationDots = (m_saveAnimationDots % 3) + 1;
        QString dots = QString(".").repeated(m_saveAnimationDots);
        statusBar()->showMessage("💾 Сохранение" + dots, 3000);
        updateStatusIndicator("🟡 Сохранение..." + dots, "#FF9800");
    });

    // ===== ПОДКЛЮЧАЕМ ТОЧЕЧНЫЕ СИГНАЛЫ =====
    connect(&m_db, &DatabaseManager::folderRemoved,
            this, &MainWindow::onFolderRemoved);

    restoreLastFolder();
    if (m_startupPage) {
        connect(m_startupPage, &StartupPage::showAbout, this, [this]() {
            if (m_aboutDialog) {
                if (!m_aboutDialog->isVisible()) {
                    m_aboutDialog->show();
                }
                m_aboutDialog->raise();
                m_aboutDialog->activateWindow();
                return;
            }

            m_aboutDialog = new AboutDialog(this);
            connect(m_aboutDialog, &AboutDialog::finished, [this]() {
                if (m_aboutDialog) {
                    m_aboutDialog->hide();
                }
            });
            connect(m_aboutDialog, &QObject::destroyed, [this]() {
                m_aboutDialog = nullptr;
            });

            m_aboutDialog->show();
        });
    }
    showStartupPage();

    setupShortcuts();
    updateNotificationBadge(0);
    setAcceptDrops(true);

    // Подключаемся к сигналу смены темы
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::onThemeChanged);
    qDebug() << "✅ MainWindow::MainWindow - конструктор завершён успешно";
}

MainWindow::~MainWindow()
{
    m_db.enableAutoReload(false);

    if (m_aboutDialog) {
        delete m_aboutDialog;
        m_aboutDialog = nullptr;
    }
    if (m_notificationPanel) {
        delete m_notificationPanel;
        m_notificationPanel = nullptr;
    }
    if (m_onlineUsersPanel) {
        delete m_onlineUsersPanel;
        m_onlineUsersPanel = nullptr;
    }
}
// ============================================================
// ИНИЦИАЛИЗАЦИЯ ИНДИКАТОРА СТАТУСА
// ============================================================

void MainWindow::setupStatusIndicator()
{
    m_statusIndicator = new QLabel(this);
    m_statusIndicator->setText("🟢 БД: онлайн | Автообновление: вкл");
    m_statusIndicator->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   font-weight: bold;"
        "   padding: 2px 10px;"
        "   font-size: 11px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   background: transparent;"
        "}"
    ).arg(ThemeManager::instance().successColor()));
    m_statusIndicator->setAlignment(Qt::AlignCenter);
    m_statusBar->addPermanentWidget(m_statusIndicator);

    // Индикатор пользователя
    m_userIndicator = new QLabel(this);
    QString userFIO = UserConfig::instance().getUserFIO();
    m_userIndicator->setText(QString("👤 %1").arg(userFIO.isEmpty() ? "Гость" : userFIO));
    m_userIndicator->setStyleSheet(QString(
        "QLabel {"
        "   color: %1;"
        "   font-weight: bold;"
        "   padding: 2px 10px;"
        "   font-size: 11px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   background: transparent;"
        "}"
    ).arg(ThemeManager::instance().accentColor()));
    m_userIndicator->setAlignment(Qt::AlignCenter);
    m_statusBar->addPermanentWidget(m_userIndicator);
}

void MainWindow::updateStatusIndicator(const QString& text, const QString& color)
{
    if (m_statusIndicator) {
        m_statusIndicator->setText(text);
        m_statusIndicator->setStyleSheet(
            "QLabel {"
            "   color: " + color + ";"
                      "   font-weight: bold;"
                      "   padding: 2px 10px;"
                      "   font-size: 11px;"
                      "   font-family: 'Segoe UI', 'Arial', sans-serif;"
                      "   background: transparent;"
                      "}"
            );
    }
}

// ============================================================
// ГОРЯЧИЕ КЛАВИШИ
// ============================================================

void MainWindow::setupShortcuts()
{
    QAction* actionSave = new QAction(this);
    actionSave->setShortcut(QKeySequence::Save);
    connect(actionSave, &QAction::triggered, [this]() {
        if (m_db.isDirty()) {
            m_saveAnimationTimer->start(300);
            m_db.forceSave();
            statusBar()->showMessage("💾 Сохранение базы данных...", 2000);
        } else {
            statusBar()->showMessage("ℹ️ Нет изменений для сохранения", 2000);
        }
    });
    addAction(actionSave);

    QAction* actionBackup = new QAction(this);
    actionBackup->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(actionBackup, &QAction::triggered, this, &MainWindow::onBackup);
    addAction(actionBackup);

    QAction* actionHelp = new QAction(this);
    actionHelp->setShortcut(QKeySequence::HelpContents);
    connect(actionHelp, &QAction::triggered, [this]() {
        QMessageBox::information(this, "Помощь",
                                 "📖 КД Менеджер\n\n"
                                 "Горячие клавиши:\n"
                                 "• Ctrl+N - Новое изделие\n"
                                 "• Ctrl+Shift+N - Новая папка\n"
                                 "• Ctrl+F - Поиск\n"
                                 "• Ctrl+S - Сохранить БД\n"
                                 "• Ctrl+Shift+S - Создать бэкап\n"
                                 "• Ctrl+M - Комментарии\n"
                                 "• F5 - Обновить\n"
                                 "• Delete - Удалить файл\n"
                                 "• Esc - Закрыть превью/уведомления\n"
                                 "• F1 - Эта справка");
    });
    addAction(actionHelp);

    QAction* actionHome = new QAction(this);
    actionHome->setShortcut(QKeySequence("Home"));
    connect(actionHome, &QAction::triggered, this, &MainWindow::togglePage);
    addAction(actionHome);

    QAction* actionClose = new QAction(this);
    actionClose->setShortcut(QKeySequence::Close);
    connect(actionClose, &QAction::triggered, this, &MainWindow::close);
    addAction(actionClose);
}

// ============================================================
// ВОССТАНОВЛЕНИЕ ПОСЛЕДНЕЙ ПАПКИ
// ============================================================

void MainWindow::restoreLastFolder()
{
    QSettings settings("KDManager", "MainWindow");
    QString lastPath = settings.value("lastFolderPath").toString();
    QString lastProduct = settings.value("lastProductPath").toString();

    if (!lastPath.isEmpty() && m_db.folders().contains(lastPath)) {
        m_currentPath = lastPath;
        findAndSelectFolder(lastPath);
        statusBar()->showMessage("📂 Восстановлена папка: " + m_db.folders()[lastPath].name, 3000);
    } else if (!lastProduct.isEmpty() && m_db.folders().contains(lastProduct)) {
        m_currentPath = lastProduct;
        findAndSelectFolder(lastProduct);
        statusBar()->showMessage("📂 Восстановлено изделие: " + m_db.folders()[lastProduct].name, 3000);
    }
}

// ============================================================
// ТАЙМЕРЫ
// ============================================================

void MainWindow::onAutoSave()
{
    if (m_db.isDirty()) {
        m_saveAnimationTimer->start(300);
        m_db.forceSave();
    } else {
        m_db.checkFileChanged();
    }
}

void MainWindow::onAutoRefresh()
{
    m_db.checkFileChanged();
}
// ============================================================
// ОБНОВЛЕНИЕ ВСЕГО СРАЗУ
// ============================================================

void MainWindow::refreshAll()
{
    qDebug() << "=== refreshAll ===";

    // Сохраняем выделенный файл
    QString selectedFilePath;
    if (m_fileContainer) {
        QStringList selected = m_fileContainer->getSelectedFiles();
        if (!selected.isEmpty()) {
            selectedFilePath = selected.first();
        }
    }

    // Обновляем дерево
    refreshTree();

    // Обновляем файлы
    if (!m_currentPath.isEmpty() && m_db.folders().contains(m_currentPath)) {
        updateFileTable();
    } else {
        // Если текущий путь не существует - ищем первый доступный раздел
        for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
            if (!it.key().isEmpty()) {
                m_currentPath = it.key();
                updateFileTable();
                break;
            }
        }
    }

    // Обновляем стартовую страницу
    loadStartupPage();
    if (m_startupPage) {
        m_startupPage->updateStatistics();
    }

    // Восстанавливаем выделение
    if (!selectedFilePath.isEmpty() && m_fileContainer) {
        m_fileContainer->selectFile(selectedFilePath);
    }
}

// ============================================================
// ОБНОВЛЕНИЕ ДЕРЕВА (НОВОЕ КАСТОМНОЕ ДЕРЕВО)
// ============================================================

void MainWindow::refreshTree()
{
    if (m_isRefreshing) return;
    m_isRefreshing = true;

    qDebug() << "=== refreshTree ===";
    qDebug() << "Количество папок в m_folders:" << m_db.folders().size();

    clearTreeContainer();
    m_treeItemWidgets.clear();

    const auto& folders = m_db.folders();

    // ===== НАХОДИМ РАЗДЕЛ "ИЗДЕЛИЯ" =====
    QString productSectionPath;
    for (auto it = folders.begin(); it != folders.end(); ++it) {
        if (it.key().isEmpty()) continue;
        int lastSlash = it.key().lastIndexOf('/');
        QString parentPath = (lastSlash != -1) ? it.key().left(lastSlash) : "";
        if (parentPath.isEmpty() && it.value().name == "Изделия" && it.value().type == TYPE_PRODUCT) {
            productSectionPath = it.key();
            break;
        }
    }

    // ============================================================
    // 1. ЗАГОЛОВОК "ИЗДЕЛИЯ"
    // ============================================================
    QLabel* productsHeader = new QLabel("  🏢  ИЗДЕЛИЯ", m_treeContainer);
    productsHeader->setStyleSheet(getHeaderStyle());
    productsHeader->setFixedHeight(56);
    productsHeader->setWordWrap(false);
    productsHeader->setMinimumWidth(200);
    productsHeader->setAlignment(Qt::AlignCenter);
    m_treeLayout->addWidget(productsHeader);

    // Изделия
    if (!productSectionPath.isEmpty()) {
        QList<QPair<QString, FolderData>> productFolders;
        for (auto it = folders.begin(); it != folders.end(); ++it) {
            if (it.key().isEmpty()) continue;
            if (it.key().startsWith(productSectionPath + "/") && it.value().type == TYPE_PRODUCT) {
                productFolders.append(qMakePair(it.key(), it.value()));
            }
        }

        std::sort(productFolders.begin(), productFolders.end(),
                  [](const QPair<QString, FolderData>& a, const QPair<QString, FolderData>& b) {
                      return a.second.name < b.second.name;
                  });

        for (const auto& pair : productFolders) {
            bool hasChildren = false;
            for (auto it = folders.begin(); it != folders.end(); ++it) {
                if (it.key().startsWith(pair.first + "/")) {
                    hasChildren = true;
                    break;
                }
            }
            bool isExpanded = m_expandedState.value(pair.first, false);
            addTreeItem(pair.first, pair.second, 0, isExpanded);

            if (isExpanded) {
                buildTreeRecursiveWithGroups(pair.first, 1);
            }
        }
    }

    // ============================================================
    // 2. ОСТАЛЬНЫЕ РАЗДЕЛЫ (добавленные вручную)
    // ============================================================
    QList<QPair<QString, FolderData>> userSectionsList;
    for (auto it = folders.begin(); it != folders.end(); ++it) {
        if (it.key().isEmpty()) continue;
        int lastSlash = it.key().lastIndexOf('/');
        QString parentPath = (lastSlash != -1) ? it.key().left(lastSlash) : "";
        // Берём только корневые папки, которые НЕ "Изделия"
        if (parentPath.isEmpty() && it.value().name != "Изделия") {
            userSectionsList.append(qMakePair(it.key(), it.value()));
        }
    }

    // Сортируем
    std::sort(userSectionsList.begin(), userSectionsList.end(),
              [](const QPair<QString, FolderData>& a, const QPair<QString, FolderData>& b) {
                  return a.second.name < b.second.name;
              });

    // Добавляем заголовок "РАЗДЕЛЫ", если есть пользовательские разделы
    if (!userSectionsList.isEmpty()) {
        // ===== РАЗДЕЛИТЕЛЬ =====
        QFrame* separator = new QFrame(m_treeContainer);
        separator->setFrameShape(QFrame::HLine);
        separator->setStyleSheet(getSeparatorStyle());
        m_treeLayout->addWidget(separator);

        QLabel* sectionsHeader = new QLabel("  📁  РАЗДЕЛЫ", m_treeContainer);
        sectionsHeader->setStyleSheet(getSectionHeaderStyle());
        sectionsHeader->setFixedHeight(56);
        sectionsHeader->setWordWrap(false);
        sectionsHeader->setMinimumWidth(200);
        sectionsHeader->setAlignment(Qt::AlignCenter);
        m_treeLayout->addWidget(sectionsHeader);
    }

    // Отображаем каждый раздел как отдельный блок
    for (const auto& pair : userSectionsList) {
        // ===== СОДЕРЖИМОЕ РАЗДЕЛА (БЕЗ ОТДЕЛЬНОГО БАРА) =====
        bool hasChildren = false;
        for (auto it = folders.begin(); it != folders.end(); ++it) {
            if (it.key().startsWith(pair.first + "/")) {
                hasChildren = true;
                break;
            }
        }

        bool isExpanded = m_expandedState.value(pair.first, false);
        addTreeItem(pair.first, pair.second, 0, isExpanded);

        if (isExpanded && hasChildren) {
            buildTreeRecursive(pair.first, 1);
        }
    }

    // ============================================================
    // 3. КНОПКА "ДОБАВИТЬ НОВЫЙ РАЗДЕЛ"
    // ============================================================
    QPushButton* addBtn = new QPushButton("➕ Добавить новый раздел", m_treeContainer);
    addBtn->setStyleSheet(getButtonStyle());
    addBtn->setMinimumHeight(36);
    addBtn->setMinimumWidth(150);
    addBtn->setCursor(Qt::PointingHandCursor);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::onNewRootFolder);
    m_treeLayout->addWidget(addBtn);

    m_treeLayout->addStretch();

    m_isRefreshing = false;
}

void MainWindow::buildTreeRecursiveWithGroups(const QString& parentPath, int level)
{
    const auto& folders = m_db.folders();

    // Собираем все дочерние папки по типам
    QList<QPair<QString, FolderData>> assemblyFolders;
    QList<QPair<QString, FolderData>> otherFolders;
    QList<QPair<QString, FolderData>> systemFolders; // Главный вид 3д модель, Чертеж, Спецификация

    for (auto it = folders.begin(); it != folders.end(); ++it) {
        if (it.key().isEmpty()) continue;
        int lastSlash = it.key().lastIndexOf('/');
        QString folderParent = (lastSlash != -1) ? it.key().left(lastSlash) : "";
        if (folderParent == parentPath) {
            if (it.value().type == TYPE_ASSEMBLY) {
                assemblyFolders.append(qMakePair(it.key(), it.value()));
            } else if (it.value().type == TYPE_3D || it.value().type == TYPE_DRAWING || it.value().type == TYPE_SPEC) {
                systemFolders.append(qMakePair(it.key(), it.value()));
            } else {
                otherFolders.append(qMakePair(it.key(), it.value()));
            }
        }
    }

    // Сортируем
    auto sortFunc = [](const QPair<QString, FolderData>& a, const QPair<QString, FolderData>& b) {
        return a.second.name < b.second.name;
    };
    std::sort(assemblyFolders.begin(), assemblyFolders.end(), sortFunc);
    std::sort(systemFolders.begin(), systemFolders.end(), sortFunc);
    std::sort(otherFolders.begin(), otherFolders.end(), sortFunc);

    // ===== 1. СИСТЕМНЫЕ ПАПКИ (Главный вид, Чертеж, Спецификация) =====
    for (const auto& pair : systemFolders) {
        bool hasChildren = false;
        for (auto it = folders.begin(); it != folders.end(); ++it) {
            if (it.key().startsWith(pair.first + "/")) {
                hasChildren = true;
                break;
            }
        }
        bool isExpanded = m_expandedState.value(pair.first, false);
        addTreeItem(pair.first, pair.second, level, isExpanded);

        if (isExpanded) {
            buildTreeRecursiveWithGroups(pair.first, level + 1);
        }
    }

    // ===== 2. ОСТАЛЬНЫЕ ПАПКИ =====
    if (!otherFolders.isEmpty()) {
        // Разделитель
        QFrame* separator = new QFrame(m_treeContainer);
        separator->setFrameShape(QFrame::HLine);
        separator->setStyleSheet(QString(
            "QFrame {"
            "   background: %1;"
            "   max-height: 1px;"
            "   margin: 4px 4px;"
            "}"
            ).arg(ThemeManager::instance().separatorColor()));
        m_treeLayout->addWidget(separator);

        for (const auto& pair : otherFolders) {
            bool hasChildren = false;
            for (auto it = folders.begin(); it != folders.end(); ++it) {
                if (it.key().startsWith(pair.first + "/")) {
                    hasChildren = true;
                    break;
                }
            }
            bool isExpanded = m_expandedState.value(pair.first, false);
            addTreeItem(pair.first, pair.second, level, isExpanded);

            if (isExpanded) {
                buildTreeRecursiveWithGroups(pair.first, level + 1);
            }
        }
    }

    // ===== 3. СБОРКИ =====
    if (!assemblyFolders.isEmpty()) {
        // Разделитель
        QFrame* separator = new QFrame(m_treeContainer);
        separator->setFrameShape(QFrame::HLine);
        separator->setStyleSheet(QString(
            "QFrame {"
            "   background: %1;"
            "   max-height: 1px;"
            "   margin: 4px 4px;"
            "}"
            ).arg(ThemeManager::instance().separatorColor()));
        m_treeLayout->addWidget(separator);

        // Заголовок "СБОРКИ"
        ThemeManager& tm = ThemeManager::instance();
        QLabel* assemblyHeader = new QLabel("  🔧  СБОРКИ", m_treeContainer);
        assemblyHeader->setStyleSheet(QString(
            "QLabel {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "                               stop:0 %1, stop:1 %2);"
            "   border-radius: 8px;"
            "   padding: 4px 14px;"
            "   font-size: 11px;"
            "   font-weight: 700;"
            "   color: %3;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "   margin: 2px 4px;"
            "}"
        ).arg(tm.secondaryGradientStart(), tm.secondaryGradientEnd(), tm.textColor()));
        assemblyHeader->setFixedHeight(30);
        assemblyHeader->setWordWrap(false);
        assemblyHeader->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_treeLayout->addWidget(assemblyHeader);

        for (const auto& pair : assemblyFolders) {
            bool hasChildren = false;
            for (auto it = folders.begin(); it != folders.end(); ++it) {
                if (it.key().startsWith(pair.first + "/")) {
                    hasChildren = true;
                    break;
                }
            }
            bool isExpanded = m_expandedState.value(pair.first, false);
            addTreeItem(pair.first, pair.second, level, isExpanded);

            if (isExpanded) {
                buildTreeRecursiveWithGroups(pair.first, level + 1);
            }
        }
    }
}



void MainWindow::showTreeContextMenuForPath(const QString& path, TreeItemWidget* itemWidget, const QPoint& pos)
{
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Используем копию вместо прямого доступа к БД
    // Это предотвращает deadlock при одновременном сохранении БД
    QMap<QString, FolderData> foldersCopy = m_db.getFoldersCopy();

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "   background-color: #f5f5f5;"
        "   border: 1px solid #d0d0d0;"
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
        "   background: #d0d0d0;"
        "   margin: 4px 8px;"
        "}"
        );

    if (foldersCopy.contains(path)) {
        FolderType folderType = foldersCopy[path].type;
        bool isProductFolder = (folderType == TYPE_PRODUCT);
        bool isProductSection = (folderType == TYPE_PRODUCT && foldersCopy[path].name == "Изделия");
        bool isAssembly = (folderType == TYPE_ASSEMBLY);

        // Новая папка
        QAction* actionAddFolder = new QAction("📁 Новая папка", &menu);
        connect(actionAddFolder, &QAction::triggered, [this, path]() {
            onAddFolderToPath(path);
        });
        menu.addAction(actionAddFolder);

        // Добавить файлы
        QAction* actionAddFiles = new QAction("📄 Добавить файлы", &menu);
        connect(actionAddFiles, &QAction::triggered, [this, path]() {
            QString oldPath = m_currentPath;
            m_currentPath = path;
            onAddFiles();
            m_currentPath = oldPath;
        });
        menu.addAction(actionAddFiles);

        // Импорт
        QAction* actionImport = new QAction("📥 Импортировать папку", &menu);
        connect(actionImport, &QAction::triggered, [this, path]() {
            QString oldPath = m_currentPath;
            m_currentPath = path;
            onImportFolder();
            m_currentPath = oldPath;
        });
        menu.addAction(actionImport);

        menu.addSeparator();

        // Для папок-изделий - Открыть интерактивное дерево
        if (isProductFolder && !isProductSection) {
            QAction* actionOpenTree = new QAction("🌳 Открыть интерактивное дерево", &menu);
            connect(actionOpenTree, &QAction::triggered, [this, path]() {
                // Получаем имя изделия
                if (m_db.folders().contains(path)) {
                    QString productName = m_db.folders()[path].name;
                    m_productTreePage->showProduct(productName);
                    m_productTreePage->animateIn();
                }
            });
            menu.addAction(actionOpenTree);
            menu.addSeparator();
        }

        // Для папок-изделий - Создать СБ
        if (isProductFolder && !isProductSection) {
            QAction* actionCreateAssembly = new QAction("🏗️ Создать СБ", &menu);
            connect(actionCreateAssembly, &QAction::triggered, [this, path]() {
                bool ok;
                QString assemblyName = QInputDialog::getText(this, "Создание СБ",
                                                             "Введите название новой сборки:",
                                                             QLineEdit::Normal,
                                                             "Новая сборка", &ok);
                if (!ok || assemblyName.isEmpty()) return;

                for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
                    if (it.key().isEmpty()) continue;
                    int lastSlash = it.key().lastIndexOf('/');
                    QString folderParent = (lastSlash != -1) ? it.key().left(lastSlash) : "";
                    if (folderParent == path && it.value().name == assemblyName) {
                        QMessageBox::warning(this, "Ошибка",
                                             "Папка с именем '" + assemblyName + "' уже существует!");
                        return;
                    }
                }

                m_db.beginBatch();
                m_db.addFolder(path, assemblyName, TYPE_ASSEMBLY);
                QString assemblyPath = path + "/" + assemblyName;
                m_db.addFolder(assemblyPath, "Детали", TYPE_FOLDER);
                m_db.addFolder(assemblyPath, "Спецификация", TYPE_SPEC);
                m_db.endBatch();

                refreshTree();
                findAndSelectFolder(assemblyPath);
                statusBar()->showMessage("✅ Создана СБ: " + assemblyName, 3000);
            });
            menu.addAction(actionCreateAssembly);
            menu.addSeparator();
        }

        // Для сборок - добавить сборочную единицу
        if (isAssembly) {
            QAction* actionAddUnit = new QAction("➕ Добавить сборочную единицу", &menu);
            connect(actionAddUnit, &QAction::triggered, [this, path]() {
                bool ok;
                QString unitName = QInputDialog::getText(this, "Добавление сборочной единицы",
                                                         "Введите название сборочной единицы:",
                                                         QLineEdit::Normal,
                                                         "Новая сборочная единица", &ok);
                if (!ok || unitName.isEmpty()) return;

                for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
                    if (it.key().isEmpty()) continue;
                    int lastSlash = it.key().lastIndexOf('/');
                    QString folderParent = (lastSlash != -1) ? it.key().left(lastSlash) : "";
                    if (folderParent == path && it.value().name == unitName) {
                        QMessageBox::warning(this, "Ошибка",
                                             "Папка с именем '" + unitName + "' уже существует!");
                        return;
                    }
                }

                m_db.beginBatch();
                m_db.addFolder(path, unitName, TYPE_ASSEMBLY);
                QString unitPath = path + "/" + unitName;
                m_db.addFolder(unitPath, "Детали", TYPE_FOLDER);
                m_db.addFolder(unitPath, "Спецификация", TYPE_SPEC);
                m_db.endBatch();

                refreshTree();
                findAndSelectFolder(unitPath);
                statusBar()->showMessage("✅ Добавлена сборочная единица: " + unitName, 3000);
            });
            menu.addAction(actionAddUnit);
            menu.addSeparator();
        }

        // Переименовать
        QAction* actionRename = new QAction("✏️ Переименовать", &menu);
        connect(actionRename, &QAction::triggered, [this, path]() {
            onRenameFolder(path);
        });
        menu.addAction(actionRename);

        // Удалить
        QAction* actionDelete = new QAction("🗑️ Удалить", &menu);
        connect(actionDelete, &QAction::triggered, [this, path]() {
            onDeleteFolder(path);
        });
        menu.addAction(actionDelete);

        menu.addSeparator();

        // Комментарии
        QAction* actionComments = new QAction("💬 Комментарии", &menu);
        connect(actionComments, &QAction::triggered, [this, path]() {
            CommentDialog dialog(path, path, &m_db, this);
            dialog.exec();
        });
        menu.addAction(actionComments);
    }

    menu.exec(itemWidget->mapToGlobal(pos));
}

void MainWindow::clearTreeContainer()
{
    QLayoutItem* child;
    while ((child = m_treeLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
}

void MainWindow::addTreeItem(const QString& path, const FolderData& data, int level, bool isExpanded)
{
    QString icon = "📁";
    switch (data.type) {
    case TYPE_3D: icon = "🖥️"; break;
    case TYPE_DRAWING: icon = "📐"; break;
    case TYPE_SPEC: icon = "📋"; break;
    case TYPE_ASSEMBLY: icon = "🔧"; break;
    case TYPE_DETAIL: icon = "⚙️"; break;
    case TYPE_PRODUCT: icon = "📦"; break;
    case TYPE_REFERENCE: icon = "📚"; break;
    case TYPE_ARCHIVE: icon = "📦"; break;
    case TYPE_TECH_PROCESS: icon = "⚙️"; break;
    case TYPE_OTHER: icon = "📎"; break;
    default: icon = "📁"; break;
    }

    bool hasChildren = false;
    const auto& folders = m_db.folders();
    for (auto it = folders.begin(); it != folders.end(); ++it) {
        if (it.key().startsWith(path + "/")) {
            hasChildren = true;
            break;
        }
    }

    TreeItemWidget* itemWidget = new TreeItemWidget(data.name, icon, hasChildren, isExpanded, m_treeContainer);
    itemWidget->setIndentLevel(level);
    itemWidget->setFolderPath(path);

    if (path == m_currentPath) {
        itemWidget->setSelected(true);
    }

    connect(itemWidget, &TreeItemWidget::clicked, [this, path]() {
        onTreeItemClicked(path);
    });

    connect(itemWidget, &TreeItemWidget::expandToggled, [this, path](bool expanded) {
        m_expandedState[path] = expanded;
        refreshTree();
    });

    itemWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(itemWidget, &QWidget::customContextMenuRequested, [this, path, itemWidget](const QPoint& pos) {
        showTreeContextMenuForPath(path, itemWidget, pos);
    });

    m_treeItemWidgets[path] = itemWidget;
    m_treeLayout->addWidget(itemWidget);
}

void MainWindow::buildTreeRecursive(const QString& parentPath, int level)
{
    const auto& folders = m_db.folders();

    QList<QPair<QString, FolderData>> childFolders;
    for (auto it = folders.begin(); it != folders.end(); ++it) {
        if (it.key().isEmpty()) continue;
        int lastSlash = it.key().lastIndexOf('/');
        QString folderParent = (lastSlash != -1) ? it.key().left(lastSlash) : "";
        if (folderParent == parentPath) {
            childFolders.append(qMakePair(it.key(), it.value()));
        }
    }

    std::sort(childFolders.begin(), childFolders.end(),
              [](const QPair<QString, FolderData>& a, const QPair<QString, FolderData>& b) {
                  return a.second.name < b.second.name;
              });

    for (const auto& pair : childFolders) {
        bool hasChildren = false;
        for (auto it = folders.begin(); it != folders.end(); ++it) {
            if (it.key().startsWith(pair.first + "/")) {
                hasChildren = true;
                break;
            }
        }
        bool isExpanded = m_expandedState.value(pair.first, false);
        addTreeItem(pair.first, pair.second, level, isExpanded);

        if (isExpanded) {
            buildTreeRecursive(pair.first, level + 1);
        }
    }
}

// ============================================================
// МЕТОДЫ РАБОТЫ С ДЕРЕВОМ
// ============================================================

void MainWindow::findAndSelectFolder(const QString& folderPath)
{
    if (folderPath.isEmpty()) return;

    if (m_db.folders().contains(folderPath)) {
        m_currentPath = folderPath;

        // Обновляем выделение
        for (auto it = m_treeItemWidgets.begin(); it != m_treeItemWidgets.end(); ++it) {
            it.value()->setSelected(it.key() == folderPath);
        }

        updateFileTable();
        showWorkPage();
        updateStatus();
    }
}

void MainWindow::selectTreeItem(const QString& path)
{
    if (m_treeItemWidgets.contains(path)) {
        m_treeItemWidgets[path]->setSelected(true);
    }
}

void MainWindow::updateTreeSelection(const QString& path)
{
    for (auto it = m_treeItemWidgets.begin(); it != m_treeItemWidgets.end(); ++it) {
        it.value()->setSelected(it.key() == path);
    }
}

void MainWindow::onTreeItemClicked(const QString& path)
{
    if (path.isEmpty()) return;

    qDebug() << "=== onTreeItemClicked ===";
    qDebug() << "path:" << path;

    if (m_db.folders().contains(path)) {
        m_currentPath = path;
        qDebug() << "✅ Выбрана папка/раздел, m_currentPath =" << m_currentPath;
        updateFileTable();
        showWorkPage();
        updateStatus();
        updateTreeSelection(path);
    }
}

void MainWindow::onTreeItemExpandToggled(const QString& path, bool expanded)
{
    m_expandedState[path] = expanded;
    refreshTree();
}

void MainWindow::showTreeContextMenu(const QPoint& pos)
{
    // Находим виджет под курсором
    QWidget* widget = qApp->widgetAt(pos);
    if (!widget) return;

    // Находим TreeItemWidget
    TreeItemWidget* itemWidget = qobject_cast<TreeItemWidget*>(widget);
    if (!itemWidget) {
        // Может быть клик на дочернем виджете
        itemWidget = qobject_cast<TreeItemWidget*>(widget->parent());
        if (!itemWidget) return;
    }

    // Находим путь
    QString path;
    for (auto it = m_treeItemWidgets.begin(); it != m_treeItemWidgets.end(); ++it) {
        if (it.value() == itemWidget) {
            path = it.key();
            break;
        }
    }

    if (path.isEmpty()) return;

    // Создаем контекстное меню
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "   background-color: #f5f5f5;"
        "   border: 1px solid #d0d0d0;"
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
        "   background: #d0d0d0;"
        "   margin: 4px 8px;"
        "}"
        );

    if (m_db.folders().contains(path)) {
        FolderType folderType = m_db.folders()[path].type;
        bool isProductFolder = (folderType == TYPE_PRODUCT);
        bool isProductSection = (folderType == TYPE_PRODUCT && m_db.folders()[path].name == "Изделия");

        // Новая папка
        QAction* actionAddFolder = new QAction("📁 Новая папка", this);
        connect(actionAddFolder, &QAction::triggered, [this, path]() {
            onAddFolderToPath(path);
        });
        menu.addAction(actionAddFolder);

        // Добавить файлы
        QAction* actionAddFiles = new QAction("📄 Добавить файлы", this);
        connect(actionAddFiles, &QAction::triggered, [this, path]() {
            QString oldPath = m_currentPath;
            m_currentPath = path;
            onAddFiles();
            m_currentPath = oldPath;
        });
        menu.addAction(actionAddFiles);

        // Импорт
        QAction* actionImport = new QAction("📥 Импортировать папку", this);
        connect(actionImport, &QAction::triggered, [this, path]() {
            QString oldPath = m_currentPath;
            m_currentPath = path;
            onImportFolder();
            m_currentPath = oldPath;
        });
        menu.addAction(actionImport);

        menu.addSeparator();

        // Для папок-изделий - Создать СБ
        if (isProductFolder && !isProductSection) {
            QAction* actionCreateAssembly = new QAction("🏗️ Создать СБ", this);
            connect(actionCreateAssembly, &QAction::triggered, [this, path]() {
                bool ok;
                QString assemblyName = QInputDialog::getText(this, "Создание СБ",
                                                             "Введите название новой сборки:",
                                                             QLineEdit::Normal,
                                                             "Новая сборка", &ok);
                if (!ok || assemblyName.isEmpty()) return;

                for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
                    if (it.key().isEmpty()) continue;
                    int lastSlash = it.key().lastIndexOf('/');
                    QString folderParent = (lastSlash != -1) ? it.key().left(lastSlash) : "";
                    if (folderParent == path && it.value().name == assemblyName) {
                        QMessageBox::warning(this, "Ошибка",
                                             "Папка с именем '" + assemblyName + "' уже существует!");
                        return;
                    }
                }

                m_db.beginBatch();
                m_db.addFolder(path, assemblyName, TYPE_ASSEMBLY);
                QString assemblyPath = path + "/" + assemblyName;
                m_db.addFolder(assemblyPath, "Детали", TYPE_FOLDER);
                m_db.addFolder(assemblyPath, "Спецификация", TYPE_SPEC);
                m_db.endBatch();

                refreshTree();
                findAndSelectFolder(assemblyPath);
                statusBar()->showMessage("✅ Создана СБ: " + assemblyName, 3000);
            });
            menu.addAction(actionCreateAssembly);
            menu.addSeparator();
        }

        // Переименовать
        QAction* actionRename = new QAction("✏️ Переименовать", this);
        connect(actionRename, &QAction::triggered, [this, path]() {
            onRenameFolder(path);
        });
        menu.addAction(actionRename);

        // Удалить
        QAction* actionDelete = new QAction("🗑️ Удалить", this);
        connect(actionDelete, &QAction::triggered, [this, path]() {
            onDeleteFolder(path);
        });
        menu.addAction(actionDelete);

        menu.addSeparator();

        // Комментарии
        QAction* actionComments = new QAction("💬 Комментарии", this);
        connect(actionComments, &QAction::triggered, [this, path]() {
            // Комментарии к папке (пока не реализовано для папок)
            QMessageBox::information(this, "Информация",
                "Комментарии доступны только для файлов, а не для папок.\n"
                "Добавьте файлы в папку и откройте комментарии файла.");
        });
        menu.addAction(actionComments);
    }

    menu.exec(itemWidget->mapToGlobal(QPoint(0, itemWidget->height())));
}
// ============================================================
// ДЕЙСТВИЯ С БАЗОЙ
// ============================================================

void MainWindow::onNewProduct()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Новое изделие",
                                         "Введите наименование:",
                                         QLineEdit::Normal,
                                         "Новое изделие", &ok);
    if (ok && !name.isEmpty()) {
        // Находим раздел "Изделия"
        QString productSectionPath;
        for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
            if (it.key().isEmpty()) continue;
            int lastSlash = it.key().lastIndexOf('/');
            QString folderParent = (lastSlash != -1) ? it.key().left(lastSlash) : "";
            if (folderParent.isEmpty() && it.value().name == "Изделия" && it.value().type == TYPE_PRODUCT) {
                productSectionPath = it.key();
                break;
            }
        }

        if (productSectionPath.isEmpty()) {
            m_db.addFolder("", "Изделия", TYPE_PRODUCT);
            productSectionPath = "Изделия";
        }

        // Проверяем, нет ли уже такого изделия
        for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
            if (it.key().isEmpty()) continue;
            int lastSlash = it.key().lastIndexOf('/');
            QString folderParent = (lastSlash != -1) ? it.key().left(lastSlash) : "";
            if (folderParent == productSectionPath && it.value().name == name && it.value().type == TYPE_PRODUCT) {
                QMessageBox::warning(this, "Ошибка", "Изделие с именем '" + name + "' уже существует!");
                return;
            }
        }

        m_db.beginBatch();

        // Создаем папку изделия
        m_db.addFolder(productSectionPath, name, TYPE_PRODUCT);
        QString productPath = productSectionPath + "/" + name;

        // Подпапки
        if (!m_db.folders().contains(productPath + "/Главный вид 3д модель")) {
            m_db.addFolder(productPath, "Главный вид 3д модель", TYPE_3D);
        }
        if (!m_db.folders().contains(productPath + "/Главный сборочный чертеж")) {
            m_db.addFolder(productPath, "Главный сборочный чертеж", TYPE_DRAWING);
        }
        if (!m_db.folders().contains(productPath + "/Основная спецификация")) {
            m_db.addFolder(productPath, "Основная спецификация", TYPE_SPEC);
        }

        m_db.endBatch();

        refreshTree();
        updateFileTable();
        findAndSelectFolder(productPath);

        NotificationManager::Notification notif;
        notif.type = NotificationManager::TYPE_SUCCESS;
        notif.title = "✅ Новое изделие создано";
        notif.message = QString("Создано изделие: %1").arg(name);
        notif.timestamp = QDateTime::currentDateTime();
        notif.isRead = false;
        notif.isImportant = false;
        notif.folderPath = productPath;
        notif.sourceUser = qgetenv("USERNAME");
        NotificationManager::instance().addNotification("", notif);

        statusBar()->showMessage("✅ Создано изделие: " + name, 3000);
    }
}

void MainWindow::onNewRootFolder()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Новый раздел",
                                         "Введите название раздела:",
                                         QLineEdit::Normal,
                                         "Новый раздел", &ok);
    if (ok && !name.isEmpty()) {
        // Проверяем, нет ли уже такого раздела
        for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
            if (it.key().isEmpty()) continue;
            int lastSlash = it.key().lastIndexOf('/');
            QString folderParent = (lastSlash != -1) ? it.key().left(lastSlash) : "";
            if (folderParent.isEmpty() && it.value().name == name) {
                QMessageBox::warning(this, "Ошибка", "Раздел с именем '" + name + "' уже существует!");
                return;
            }
        }

        m_db.beginBatch();
        m_db.addFolder("", name, TYPE_OTHER);
        m_db.endBatch();

        refreshTree();
        updateFileTable();
        findAndSelectFolder(name);

        NotificationManager::Notification notif;
        notif.type = NotificationManager::TYPE_SUCCESS;
        notif.title = "✅ Новый раздел создан";
        notif.message = QString("Создан раздел: %1").arg(name);
        notif.timestamp = QDateTime::currentDateTime();
        notif.isRead = false;
        notif.isImportant = false;
        notif.folderPath = name;
        notif.sourceUser = qgetenv("USERNAME");

        statusBar()->showMessage("✅ Создан раздел: " + name, 3000);
    }
}

void MainWindow::onAddFolder()
{
    if (m_currentPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите папку!");
        return;
    }

    // ИСПРАВЛЕНИЕ 4: Проверка папки перед началом операции
    if (!m_db.folders().contains(m_currentPath)) {
        QMessageBox::warning(this, "Ошибка", "Папка была удалена другим пользователем!");
        m_currentPath = "";
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, "Новая папка",
                                         "Введите имя папки:",
                                         QLineEdit::Normal,
                                         "Новая папка", &ok);

    if (ok && !name.isEmpty()) {
        // ИСПРАВЛЕНИЕ 5: Валидация имени папки - проверка на уникальность
        for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
            if (it.key().isEmpty()) continue;
            QString path = it.key();
            int lastSlash = path.lastIndexOf('/');
            QString folderParent = (lastSlash != -1) ? path.left(lastSlash) : "";
            if (folderParent == m_currentPath && it.value().name == name) {
                QMessageBox::warning(this, "Ошибка", "Папка с именем '" + name + "' уже существует!");
                return;
            }
        }

        // ИСПРАВЛЕНИЕ 6: Проверка папки ЕЩЁ РАЗ перед операцией
        if (!m_db.folders().contains(m_currentPath)) {
            QMessageBox::warning(this, "Ошибка", "Папка была удалена!");
            return;
        }

        m_db.beginBatch();
        m_db.addFolder(m_currentPath, name, TYPE_FOLDER);
        m_db.endBatch();

        refreshTree();
        updateFileTable();

        QString newPath = m_currentPath + "/" + name;
        findAndSelectFolder(newPath);

        NotificationManager::Notification notif;
        notif.type = NotificationManager::TYPE_SUCCESS;
        notif.title = "✅ Новая папка создана";
        notif.message = QString("Создана папка: %1").arg(name);
        notif.timestamp = QDateTime::currentDateTime();
        notif.isRead = false;
        notif.isImportant = false;
        notif.folderPath = newPath;
        notif.sourceUser = qgetenv("USERNAME");

        statusBar()->showMessage("✅ Создана папка: " + name, 3000);
    }
}

void MainWindow::onAddFiles()
{
    qDebug() << "=== onAddFiles ===";
    qDebug() << "m_currentPath:" << m_currentPath;

    if (m_currentPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите папку или раздел для добавления файлов!");
        return;
    }

    if (!m_db.folders().contains(m_currentPath)) {
        qDebug() << "❌ Папка/раздел не найден в БД:" << m_currentPath;
        QMessageBox::warning(this, "Ошибка", "Папка была удалена из базы данных другим пользователем!");
        m_currentPath = "";
        return;
    }

    QString targetFolder = m_currentPath;
    qDebug() << "Добавляем файлы в:" << targetFolder;

    AddFilesDialog dialog(targetFolder, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList selectedFiles = dialog.getSelectedFiles();
    QString responsible = dialog.getResponsible();  // БАГ 5: ПОЛУЧАЕМ ОТВЕТСТВЕННОГО

    if (selectedFiles.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не выбрано ни одного файла!");
        return;
    }

    // ИСПРАВЛЕНИЕ 2: Проверка папки перед началом операции
    if (!m_db.folders().contains(targetFolder)) {
        QMessageBox::warning(this, "Ошибка", "Папка была удалена из базы данных!");
        return;
    }

    int added = 0;
    int skipped = 0;

    m_db.beginBatch();

    const FolderData& folder = m_db.folders()[targetFolder];
    QStringList existingFiles = folder.files.toList();

    for (const QString& filePath : selectedFiles) {
        // ИСПРАВЛЕНИЕ 3: Проверка папки в цикле
        if (!m_db.folders().contains(targetFolder)) {
            qDebug() << "⚠️ Папка удалена во время добавления файлов";
            break;
        }

        if (!QFile::exists(filePath)) {
            skipped++;
            continue;
        }

        if (existingFiles.contains(filePath)) {
            skipped++;
            continue;
        }

        QSqlQuery checkQuery(m_db.getDatabase());
        checkQuery.prepare("SELECT COUNT(*) FROM documents WHERE folder_path = :folder_path AND path = :path");
        checkQuery.bindValue(":folder_path", targetFolder);
        checkQuery.bindValue(":path", filePath);

        if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
            skipped++;
            checkQuery.clear();  // ИСПРАВЛЕНИЕ: Закрыть query
            continue;
        }
        checkQuery.clear();  // ИСПРАВЛЕНИЕ: Закрыть query

        if (m_db.addFile(targetFolder, filePath, responsible)) {  // БАГ 5: ПЕРЕДАЕМ ОТВЕТСТВЕННОГО
            added++;
            qDebug() << "   ✅ Файл добавлен в:" << targetFolder;
        } else {
            skipped++;
        }
    }

    m_db.endBatch();

    if (added > 0) {
        refreshAll();
        findAndSelectFolder(targetFolder);

        NotificationManager::Notification notif;
        notif.type = NotificationManager::TYPE_SUCCESS;
        notif.title = "✅ Файлы добавлены";
        notif.message = QString("Добавлено файлов: %1\nПропущено: %2").arg(added).arg(skipped);
        notif.timestamp = QDateTime::currentDateTime();
        notif.isRead = false;
        notif.isImportant = false;
        notif.folderPath = targetFolder;
        notif.sourceUser = qgetenv("USERNAME");

        QMessageBox::information(this, "Результат",
                                 QString("✅ Добавлено файлов: %1\nПропущено (уже есть): %2")
                                     .arg(added).arg(skipped));
    } else {
        if (skipped > 0) {
            QMessageBox::information(this, "Информация",
                                     QString("Все выбранные файлы уже есть в этой папке.\n"
                                             "Пропущено: %1").arg(skipped));
        } else {
            QMessageBox::warning(this, "Ошибка",
                                 "Не удалось добавить файлы.\n"
                                 "Проверьте, что файлы существуют.");
        }
    }
}

void MainWindow::onImportFolder()
{
    if (m_currentPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите целевую папку в дереве!");
        return;
    }

    QString dirPath = QFileDialog::getExistingDirectory(this, "Выберите папку для импорта");
    if (dirPath.isEmpty()) return;

    onImportFolderFromPath(dirPath);
    refreshAll();
}

void MainWindow::onImportFolderFromPath(const QString& dirPath)
{
    if (m_currentPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите целевую папку в дереве!");
        return;
    }

    QString targetFolder = m_currentPath;
    qDebug() << "Импорт в папку:" << targetFolder;

    FolderType folderType = m_db.folders()[targetFolder].type;
    int added = 0, skipped = 0, errors = 0;
    int total = 0;
    QStringList addedFolders, addedFiles;  // Для отката в случае ошибки

    QDir dir(dirPath);
    std::function<void(QDir)> countItems = [&](QDir currentDir) {
        QFileInfoList items = currentDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& item : items) {
            if (item.isDir()) {
                countItems(QDir(item.absoluteFilePath()));
            } else {
                total++;
            }
        }
    };
    countItems(dir);

    if (total == 0) {
        QMessageBox::information(this, "Информация", "В папке нет файлов для импорта");
        return;
    }

    m_db.beginBatch();

    int processed = 0;
    std::function<void(QDir, QString)> processDir = [&](QDir currentDir, QString parentPath) {
        if (errors > 0) return;  // Прерываем, если были ошибки

        QFileInfoList items = currentDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QFileInfo& item : items) {
            if (errors > 0) break;  // Прерываем цикл при ошибке

            processed++;

            if (processed % 10 == 0) {
                QApplication::processEvents();
                statusBar()->showMessage(QString("Импорт: %1/%2 (Добавлено: %3, пропущено: %4)")
                                         .arg(processed).arg(total).arg(added).arg(skipped));
            }

            if (item.isDir()) {
                QString folderName = item.fileName();
                QString folderPath = parentPath.isEmpty() ? targetFolder : targetFolder + "/" + parentPath;

                // ===== ПРОВЕРКА НА ДУБЛИ =====
                bool folderExists = false;
                for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
                    if (it.key().isEmpty()) continue;
                    int lastSlash = it.key().lastIndexOf('/');
                    QString parent = (lastSlash != -1) ? it.key().left(lastSlash) : "";
                    if (parent == folderPath && it.value().name == folderName) {
                        folderExists = true;
                        break;
                    }
                }

                if (!folderExists) {
                    if (m_db.addFolder(folderPath, folderName, TYPE_FOLDER)) {
                        QString newFolderPath = folderPath + "/" + folderName;
                        addedFolders.append(newFolderPath);
                        qDebug() << "   ✅ Папка добавлена:" << newFolderPath;
                    } else {
                        qDebug() << "   ❌ Ошибка добавления папки:" << folderPath << folderName;
                        errors++;
                        return;
                    }
                }

                processDir(QDir(item.absoluteFilePath()),
                           parentPath.isEmpty() ? folderName : parentPath + "/" + folderName);
            } else {
                QString fullPath = item.absoluteFilePath();
                QString targetPath = parentPath.isEmpty() ? targetFolder : targetFolder + "/" + parentPath;

                if (m_db.addFile(targetPath, fullPath)) {
                    added++;
                    addedFiles.append(fullPath);
                    qDebug() << "   ✅ Файл добавлен:" << fullPath;
                } else {
                    skipped++;
                    qDebug() << "   ⚠️ Файл пропущен:" << fullPath;
                }
            }
        }
    };

    processDir(dir, "");

    // ===== ОБРАБОТКА ОШИБОК И ОТКАТ =====
    if (errors > 0) {
        qDebug() << "❌ Импорт прерван, выполняем откат...";
        m_db.endBatch();  // Завершаем batch БЕЗ сохранения
        m_db.rollbackDatabase();  // Откатываем все изменения

        QMessageBox::warning(this, "Ошибка импорта",
                             QString("Импорт прерван!\nДобавлено файлов: %1\nДобавлено папок: %2\n\n"
                                    "Все изменения отменены (откат).")
                                 .arg(added).arg(addedFolders.size()));
    } else {
        m_db.endBatch();
        m_currentPath = targetFolder;
        refreshAll();

        QMessageBox::information(this, "Импорт успешен",
                                 QString("✅ Успешно импортировано:\n"
                                        "Файлов: %1\nПапок: %2\nПропущено: %3\n\n"
                                        "в папку: %4")
                                     .arg(added).arg(addedFolders.size()).arg(skipped).arg(targetFolder));
    }

    statusBar()->showMessage("", 0);
}

void MainWindow::onDeleteFile(const QString& filePath, const QString& folderPath)
{
    if (QMessageBox::question(this, "Подтверждение",
                              "Удалить файл?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_db.removeFile(folderPath, filePath);

        NotificationManager::Notification notif;
        notif.type = NotificationManager::TYPE_INFO;
        notif.title = "🗑️ Файл удален";
        notif.message = QString("Удален файл: %1").arg(QFileInfo(filePath).fileName());
        notif.timestamp = QDateTime::currentDateTime();
        notif.isRead = false;
        notif.isImportant = false;
        notif.filePath = filePath;
        notif.folderPath = folderPath;
        notif.sourceUser = qgetenv("USERNAME");
        NotificationManager::instance().addNotification("", notif);

        refreshAll();
        m_previewWidget->clear();
        m_previewWidget->setVisible(false);
        statusBar()->showMessage("Файл удален", 3000);
    }
}

void MainWindow::onDeleteFolder(const QString& path)
{
    if (!m_db.folders().contains(path)) {
        QMessageBox::warning(this, "Ошибка", "Папка не найдена!");
        return;
    }

    FolderType type = m_db.folders()[path].type;
    bool isProduct = (type == TYPE_PRODUCT);

    if (isProduct) {
        QString folderName = m_db.folders()[path].name;
        PinDialog dialog("Введите PIN-код для подтверждения удаления изделия:\n\n" + folderName, this);
        if (dialog.exec() != QDialog::Accepted || !dialog.isPinCorrect()) {
            statusBar()->showMessage("❌ Удаление отменено", 3000);
            return;
        }
    }

    if (!m_db.folders().contains(path)) {
        QMessageBox::warning(this, "Ошибка", "Папка не найдена!");
        return;
    }

    QString folderName = m_db.folders()[path].name;

    if (QMessageBox::question(this, "Подтверждение",
                              "Удалить папку '" + folderName + "' и всё содержимое?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        // ИСПРАВЛЕНИЕ 8: Проверка перед удалением
        if (!m_db.folders().contains(path)) {
            QMessageBox::warning(this, "Ошибка", "Папка уже была удалена!");
            return;
        }

        m_db.removeFolder(path);

        NotificationManager::Notification notif;
        notif.type = NotificationManager::TYPE_INFO;
        notif.title = isProduct ? "🗑️ Изделие удалено" : "🗑️ Папка удалена";
        notif.message = isProduct ? QString("Удалено изделие: %1").arg(folderName)
                                  : QString("Удалена папка: %1").arg(folderName);
        notif.timestamp = QDateTime::currentDateTime();
        notif.isRead = false;
        notif.isImportant = false;
        notif.folderPath = path;
        notif.sourceUser = qgetenv("USERNAME");
        NotificationManager::instance().addNotification("", notif);

        // ИСПРАВЛЕНИЕ 9: Безопасное очищение выделения
        if (m_currentPath == path || m_currentPath.startsWith(path + "/")) {
            m_currentPath = "";
        }

        refreshAll();

        if (m_previewWidget) {
            m_previewWidget->clear();
            m_previewWidget->setVisible(false);
        }

        statusBar()->showMessage("✅ Папка удалена", 3000);
    }
}

void MainWindow::onRenameFolder(const QString& path)
{
    if (!m_db.folders().contains(path)) {
        QMessageBox::warning(this, "Ошибка", "Папка не найдена!");
        return;
    }

    bool ok;
    QString newName = QInputDialog::getText(this, "Переименовать",
                                            "Введите новое имя:",
                                            QLineEdit::Normal,
                                            m_db.folders()[path].name, &ok);
    if (ok && !newName.isEmpty()) {
        // ИСПРАВЛЕНИЕ 7: Проверка на уникальность нового имени
        int lastSlash = path.lastIndexOf('/');
        QString parentPath = (lastSlash != -1) ? path.left(lastSlash) : "";

        for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
            if (it.key().isEmpty()) continue;
            int curSlash = it.key().lastIndexOf('/');
            QString curParent = (curSlash != -1) ? it.key().left(curSlash) : "";

            if (curParent == parentPath && it.value().name == newName && it.key() != path) {
                QMessageBox::warning(this, "Ошибка", "Папка с именем '" + newName + "' уже существует!");
                return;
            }
        }

        m_db.renameFolder(path, newName);
        refreshAll();
        statusBar()->showMessage("Папка переименована", 3000);
    }
}

void MainWindow::onDeleteSection(const QString& path)
{
    if (!m_db.folders().contains(path)) {
        QMessageBox::warning(this, "Ошибка", "Раздел не найден!");
        return;
    }

    QString sectionName = m_db.folders()[path].name;

    // Проверяем, есть ли вложенные папки
    bool hasSubFolders = false;
    for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
        if (it.key().startsWith(path + "/")) {
            hasSubFolders = true;
            break;
        }
    }

    QString message = "Удалить раздел '" + sectionName + "'";
    if (hasSubFolders) {
        message += " и все вложенные папки?";
    } else {
        message += "?";
    }

    if (QMessageBox::question(this, "Подтверждение", message,
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_db.removeFolder(path);

        // Создаём уведомление об удалении раздела
        NotificationManager::Notification notif;
        notif.type = NotificationManager::TYPE_WARNING;
        notif.title = "🗑️ Раздел удален";
        notif.message = QString("Удален раздел: %1").arg(sectionName);
        notif.timestamp = QDateTime::currentDateTime();

        refreshTree();
        m_previewWidget->clear();
        m_previewWidget->setVisible(false);
        m_currentPath = "";
        statusBar()->showMessage("✅ Раздел удален", 3000);
    }
}
// ============================================================
// ТАБЛИЦА ФАЙЛОВ — С ГРУППИРОВКОЙ ПО ТИПАМ
// ============================================================

void MainWindow::updateFileTable()
{
    qDebug() << "=== updateFileTable ===";
    qDebug() << "m_currentPath:" << m_currentPath;

    if (!m_previewWidget) {
        qDebug() << "❌ m_previewWidget is null!";
        return;
    }

    m_previewWidget->clear();
    m_previewWidget->setVisible(false);

    if (!m_fileContainer) {
        qDebug() << "❌ m_fileContainer is null!";
        return;
    }

    // НОВОЕ: Установить DatabaseManager и folderPath для FileCardContainer
    m_fileContainer->setDatabase(&m_db);
    m_fileContainer->setFolderPath(m_currentPath);

    if (m_currentPath.isEmpty()) {
        m_fileContainer->clear();
        return;
    }

    // ИСПРАВЛЕНИЕ 1: Проверка существования папки перед обращением
    if (!m_db.folders().contains(m_currentPath)) {
        qDebug() << "⚠️ Папка была удалена:" << m_currentPath;
        m_currentPath = "";
        m_fileContainer->clear();
        return;
    }

    const FolderData& folder = m_db.folders()[m_currentPath];

    qDebug() << "Папка:" << m_currentPath;
    qDebug() << "Тип папки:" << folder.type;
    qDebug() << "Файлов в папке:" << folder.files.size();

    QStringList allFiles;

    // 1. Файлы из самой папки (раздела)
    for (const QString& filePath : folder.files) {
        if (QFile::exists(filePath) && !allFiles.contains(filePath)) {
            allFiles.append(filePath);
            qDebug() << "  📄 Файл из папки/раздела:" << filePath;
        }
    }

    // 2. Если это РАЗДЕЛ (контейнер) - добавляем файлы из всех вложенных папок
    bool isSection = (folder.type == TYPE_PRODUCT || folder.type == TYPE_REFERENCE ||
                      folder.type == TYPE_ARCHIVE || folder.type == TYPE_TECH_PROCESS ||
                      folder.type == TYPE_OTHER || folder.type == TYPE_ROOT_FOLDER);

    if (isSection) {
        qDebug() << "Это РАЗДЕЛ - добавляем файлы из вложенных папок";
        for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
            if (it.key().isEmpty()) continue;
            if (it.key() == m_currentPath) continue;

            if (it.key().startsWith(m_currentPath + "/")) {
                for (const QString& filePath : it.value().files) {
                    if (QFile::exists(filePath) && !allFiles.contains(filePath)) {
                        allFiles.append(filePath);
                        qDebug() << "  📄 Файл из папки:" << it.key() << "->" << filePath;
                    }
                }
            }
        }
    }

    m_fileContainer->setFiles(allFiles);
    qDebug() << "✅ Установлено файлов:" << allFiles.size();
}

void MainWindow::highlightFileInTable(const QString& filePath)
{
    if (m_fileContainer) {
        m_fileContainer->selectFile(filePath);
        m_fileContainer->scrollToFile(filePath);
    }
}

// ============================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// ============================================================

QString MainWindow::formatFileSize(qint64 size)
{
    if (size < 1024) return QString::number(size) + " B";
    if (size < 1048576) return QString::number(size / 1024) + " KB";
    if (size < 1073741824) return QString::number(size / 1048576) + " MB";
    return QString::number(size / 1073741824) + " GB";
}

void MainWindow::updateStatus()
{
    int productCount = 0, assemblyCount = 0, detailCount = 0;
    int totalFiles = 0;

    for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
        switch (it.value().type) {
        case TYPE_PRODUCT: productCount++; break;
        case TYPE_ASSEMBLY: assemblyCount++; break;
        case TYPE_DETAIL: detailCount++; break;
        default: break;
        }
        totalFiles += it.value().files.size();
    }

    statusBar()->showMessage(QString("Изделий: %1 | СБ: %2 | Деталей: %3 | Файлов: %4")
                                 .arg(productCount).arg(assemblyCount).arg(detailCount).arg(totalFiles));
}

void MainWindow::loadStartupPage()
{
    if (m_startupPage) {
        m_startupPage->loadProducts();
    }
}

void MainWindow::showStartupPage()
{
    if (m_stackedWidget) {
        m_stackedWidget->setCurrentIndex(0);
        m_treeScrollArea->setVisible(false);
        m_fileContainer->setVisible(false);
        m_previewWidget->setVisible(false);
        m_startupPage->setVisible(true);
        m_isOnStartupPage = true;
        if (m_homeBtn) {
            m_homeBtn->setText("📂 Работа");
        }
    }
}

void MainWindow::showWorkPage()
{
    if (m_stackedWidget) {
        m_stackedWidget->setCurrentIndex(1);
        m_treeScrollArea->setVisible(true);
        m_fileContainer->setVisible(true);
        m_treeScrollArea->setObjectName("treeScrollArea");
        m_startupPage->setVisible(false);
        m_isOnStartupPage = false;
        if (m_homeBtn) {
            m_homeBtn->setText("🏠 Главная");
        }
    }
}

void MainWindow::togglePage()
{
    if (m_isOnStartupPage) {
        if (!m_currentPath.isEmpty() && m_db.folders().contains(m_currentPath)) {
            showWorkPage();
            findAndSelectFolder(m_currentPath);
        } else {
            showWorkPage();
            // Выбираем первый элемент в дереве
            if (m_treeItemWidgets.size() > 0) {
                QString firstPath = m_treeItemWidgets.keys().first();
                if (!firstPath.isEmpty()) {
                    m_currentPath = firstPath;
                    updateFileTable();
                    findAndSelectFolder(firstPath);
                }
            }
        }
    } else {
        showStartupPage();
        loadStartupPage();
    }
}

// ============================================================
// УВЕДОМЛЕНИЯ
// ============================================================

void MainWindow::toggleNotificationPanel()
{
    if (!m_notificationPanel) return;

    if (m_notificationPanel->isVisible()) {
        m_notificationPanel->hideNotificationPanel();
        m_isPanelVisible = false;
    } else {
        m_notificationPanel->showNotificationPanel();
        m_isPanelVisible = true;
    }
}

void MainWindow::updateNotificationBadge(int count)
{
    if (!m_badgeLabel) return;

    if (count > 0) {
        m_badgeLabel->setText(QString::number(count));
        m_badgeLabel->setVisible(true);
        setWindowTitle("🔔 КД Менеджер (" + QString::number(count) + " новых)");
    } else {
        m_badgeLabel->setVisible(false);
        setWindowTitle("КД Менеджер - Система управления документацией");
    }
}

// ============================================================
// ЗАКРЫТИЕ ОКНА
// ============================================================

void MainWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << "🔍 closeEvent: сохраняем состояние окна...";

    // ===== СОХРАНЕНИЕ ГЕОМЕТРИИ ОКНА =====
    UserConfig& config = UserConfig::instance();
    QRect currentGeometry = geometry();
    bool currentMaximized = isMaximized();

    qDebug() << "📐 Текущая геометрия:" << currentGeometry;
    qDebug() << "🔍 Окно максимизировано:" << currentMaximized;

    config.setWindowGeometry(currentGeometry);
    config.setWindowMaximized(currentMaximized);

    qDebug() << "✅ Геометрия сохранена в UserConfig";
    qDebug() << "📐 Сохранённая геометрия из config:" << config.getWindowGeometry();
    qDebug() << "🔍 Сохранённое maximize из config:" << config.isWindowMaximized();

    // Сохраняем текущую папку
    if (!m_currentPath.isEmpty()) {
        QSettings settings("KDManager", "MainWindow");
        settings.setValue("lastFolderPath", m_currentPath);
        if (m_db.folders().contains(m_currentPath)) {
            FolderType type = m_db.folders()[m_currentPath].type;
            if (type == TYPE_PRODUCT) {
                settings.setValue("lastProductPath", m_currentPath);
            }
        }
    }

    bool shouldSave = false;

    if (m_db.isDirty()) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Несохранённые изменения",
                                      "Есть несохранённые изменения. Сохранить перед выходом?",
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        }

        shouldSave = (reply == QMessageBox::Yes);
    }

    m_db.enableAutoReload(false);

    if (shouldSave) {
        m_db.saveDatabase();
    }

    if (m_changesPanel) {
        m_changesPanel->saveNotificationsToDatabase();
    }

    qDebug() << "✅ closeEvent: состояние сохранено";
    event->accept();
}

// ============================================================
// ДОПОЛНИТЕЛЬНЫЙ МЕТОД ДЛЯ УДАЛЕНИЯ ВЫДЕЛЕННЫХ ФАЙЛОВ
// ============================================================

void MainWindow::onDeleteSelectedFiles()
{
    QStringList selectedFiles = m_fileContainer->getSelectedFiles();
    if (selectedFiles.isEmpty()) {
        QMessageBox::information(this, "Информация", "Выберите файлы для удаления");
        return;
    }

    if (QMessageBox::question(this, "Подтверждение",
                              "Удалить выбранные файлы (" + QString::number(selectedFiles.size()) + " шт.)?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

        int deleted = 0;
        m_db.beginBatch();
        for (const QString& filePath : selectedFiles) {
            if (!filePath.isEmpty() && !m_currentPath.isEmpty()) {
                m_db.removeFile(m_currentPath, filePath);
                deleted++;
            }
        }
        m_db.endBatch();

        refreshAll();
        m_previewWidget->clear();
        m_previewWidget->setVisible(false);
        statusBar()->showMessage("Удалено файлов: " + QString::number(deleted), 3000);
    }
}
// ============================================================
// КОММЕНТАРИИ
// ============================================================

void MainWindow::onShowComments()
{
    if (m_currentPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите папку для просмотра комментариев!");
        return;
    }

    // Получаем выбранный файл из контейнера
    QStringList selectedFiles = m_fileContainer->getSelectedFiles();

    if (selectedFiles.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите файл!");
        return;
    }

    QString filePath = selectedFiles.first();

    // Получаем полный путь документа из БД
    QString docKey = m_currentPath + "|" + filePath;
    if (!m_db.documents().contains(docKey)) {
        QMessageBox::warning(this, "Ошибка", "Документ не найден в БД!");
        return;
    }

    DocInfo& doc = m_db.documents()[docKey];
    QString fullPath = doc.path;  // Полный путь документа

    CommentDialog dialog(fullPath, m_currentPath, &m_db, this);
    dialog.exec();
}

void MainWindow::onEditComment(const QString& filePath)
{
    if (m_currentPath.isEmpty() || !m_db.folders().contains(m_currentPath)) {
        QMessageBox::warning(this, "Ошибка", "Папка не выбрана!");
        return;
    }

    // Используем составной ключ: folderPath + "|" + filePath
    QString docKey = m_currentPath + "|" + filePath;
    if (!m_db.documents().contains(docKey)) {
        QMessageBox::warning(this, "Ошибка", "Документ не найден!");
        return;
    }

    bool ok;
    QString comment = QInputDialog::getText(this, "Редактировать комментарий",
                                            "Введите комментарий:",
                                            QLineEdit::Normal,
                                            m_db.documents()[docKey].comment, &ok);
    if (ok) {
        m_db.documents()[docKey].comment = comment;
        m_db.saveDatabase();
        refreshAll();
        statusBar()->showMessage("Комментарий обновлен", 3000);
    }
}

// ============================================================
// ПОИСК
// ============================================================

void MainWindow::onSearch()
{
    if (m_searchDialog) {
        m_searchDialog->raise();
        m_searchDialog->activateWindow();
        return;
    }

    m_searchDialog = new SearchDialog(this);
    m_searchDialog->setDatabase(&m_db);
    m_searchDialog->setMainWindow(this);
    m_searchDialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(m_searchDialog, &SearchDialog::fileSelected,
            this, [this](const QString& filePath, const QString& folderPath) {
                showWorkPageAndSelectFile(filePath, folderPath);
            });

    connect(m_searchDialog, &QDialog::finished, [this]() {
        m_searchDialog = nullptr;
    });

    m_searchDialog->show();
}

void MainWindow::onShowTasks()
{
    QString userFIO = UserConfig::instance().getUserFIO();
    TasksDialog* dialog = new TasksDialog(userFIO, &m_db, this);
    dialog->exec();
    delete dialog;
}

void MainWindow::showWorkPageAndSelectFile(const QString& filePath, const QString& folderPath)
{
    qDebug() << "=== showWorkPageAndSelectFile ===";
    qDebug() << "filePath:" << filePath;
    qDebug() << "folderPath:" << folderPath;

    // 1. Переключаемся на рабочую страницу
    showWorkPage();

    // 2. Проверяем, что папка существует
    if (!m_db.folders().contains(folderPath)) {
        qDebug() << "❌ Папка не найдена:" << folderPath;
        QMessageBox::warning(this, "Ошибка", "Папка не найдена в базе данных!");
        return;
    }

    // 3. Выделяем папку в дереве
    findAndSelectFolder(folderPath);

    // 4. Устанавливаем текущую папку
    m_currentPath = folderPath;

    // 5. Обновляем таблицу файлов
    updateFileTable();

    // 6. Подсвечиваем файл в таблице
    highlightFileInTable(filePath);

    // 7. Открываем превью
    if (QFile::exists(filePath)) {
        m_previewWidget->setVisible(true);
        m_previewWidget->setFile(filePath);

        QSplitter* splitter = qobject_cast<QSplitter*>(m_previewWidget->parent());
        if (splitter && splitter->count() >= 3) {
            splitter->setSizes(QList<int>() << 200 << 400 << 300);
        }
    } else {
        m_previewWidget->setVisible(false);
        m_previewWidget->clear();
        QMessageBox::warning(this, "Ошибка", "Файл не найден на диске:\n" + filePath);
    }

    statusBar()->showMessage("📂 Открыт файл: " + QFileInfo(filePath).fileName(), 3000);
}

// ============================================================
// ОБНОВЛЕНИЕ БАЗЫ ДАННЫХ
// ============================================================

void MainWindow::onRefreshDB()
{
    // Сохраняем текущую папку
    QString currentPath = m_currentPath;

    // Перезагружаем базу
    m_db.loadDatabase();

    // Восстанавливаем файлы-сироты
    QStringList orphans = m_db.getOrphanFiles();
    if (!orphans.isEmpty()) {
        qDebug() << "⚠️ Найдено файлов-сирот:" << orphans.size();

        // Создаем папку для сирот, если ее нет
        QString orphanFolder = "Файлы-сироты";
        if (!m_db.folders().contains(orphanFolder)) {
            m_db.addFolder("", orphanFolder, TYPE_OTHER);
        }

        // Принимаем все файлы-сироты
        int adopted = m_db.adoptAllOrphanFiles(orphanFolder);
        qDebug() << "✅ Принято файлов-сирот:" << adopted;

        if (adopted > 0) {
            statusBar()->showMessage(QString("✅ Восстановлено файлов-сирот: %1 в папку '%2'")
                                         .arg(adopted).arg(orphanFolder), 3000);
        }
    }

    // Восстанавливаем текущую папку
    if (!currentPath.isEmpty() && m_db.folders().contains(currentPath)) {
        m_currentPath = currentPath;
    }

    // Обновляем дерево
    refreshTree();

    // Обновляем файлы
    if (!m_currentPath.isEmpty() && m_db.folders().contains(m_currentPath)) {
        updateFileTable();
    }

    // Обновляем стартовую страницу
    loadStartupPage();
    if (m_startupPage) {
        m_startupPage->updateStatistics();
    }

    statusBar()->showMessage("✅ База данных перезагружена", 3000);
}

void MainWindow::onBackup()
{
    m_db.backupDatabase();
    QMessageBox::information(this, "Бэкап", "Резервная копия создана!");
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        m_db.loadDatabase();
        refreshAll();
        updateStatus();
        statusBar()->showMessage("Настройки применены", 3000);
    }
}

void MainWindow::onExit()
{
    close();
}

// ============================================================
// DRAG & DROP
// ============================================================

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        if (!m_currentPath.isEmpty() && m_db.folders().contains(m_currentPath)) {
            event->acceptProposedAction();
            ThemeManager& tm = ThemeManager::instance();
            setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(tm.currentTheme() == ThemeManager::Dark ? "#2A3F3F" : "#e3f2fd"));
        } else {
            event->ignore();
        }
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    ThemeManager& tm = ThemeManager::instance();
    setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(tm.backgroundColor()));

    const QMimeData* mimeData = event->mimeData();
    if (!mimeData->hasUrls()) {
        return;
    }

    if (m_currentPath.isEmpty() || !m_db.folders().contains(m_currentPath)) {
        QMessageBox::warning(this, "Ошибка",
                             "Сначала выберите папку в дереве, в которую хотите добавить файлы!");
        return;
    }

    QList<QUrl> urls = mimeData->urls();
    QStringList filePaths;
    QStringList folderPaths;

    qDebug() << "=== DROP EVENT ===";
    qDebug() << "m_currentPath (папка/раздел):" << m_currentPath;

    for (const QUrl& url : urls) {
        QString filePath = url.toLocalFile();
        if (!filePath.isEmpty()) {
            QFileInfo fi(filePath);
            if (fi.isFile()) {
                filePaths << filePath;
                qDebug() << "Файл для добавления:" << filePath;
            } else if (fi.isDir()) {
                folderPaths << filePath;
                qDebug() << "Папка для импорта:" << filePath;
            }
        }
    }

    QString currentFolder = m_currentPath;

    // Сначала обрабатываем папки (импорт)
    for (const QString& folderPath : folderPaths) {
        QString oldPath = m_currentPath;
        onImportFolderFromPath(folderPath);
        m_currentPath = oldPath;
    }

    // Затем обрабатываем отдельные файлы
    if (!filePaths.isEmpty()) {
        int added = 0;
        int skipped = 0;

        const FolderData& folder = m_db.folders()[currentFolder];
        QStringList existingFiles = folder.files.toList();

        qDebug() << "📋 Файлы в папке ДО добавления:" << existingFiles;

        m_db.beginBatch();

        for (const QString& file : filePaths) {
            qDebug() << "🔍 Проверяем файл:" << file;
            qDebug() << "   Папка назначения:" << currentFolder;

            if (existingFiles.contains(file)) {
                qDebug() << "   ⚠️ Файл уже есть в текущей папке, пропускаем";
                skipped++;
                continue;
            }

            QSqlQuery checkQuery(m_db.getDatabase());
            checkQuery.prepare("SELECT COUNT(*) FROM documents WHERE folder_path = :folder_path AND path = :path");
            checkQuery.bindValue(":folder_path", currentFolder);
            checkQuery.bindValue(":path", file);

            if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
                qDebug() << "   ⚠️ Запись уже есть в БД для этой папки, пропускаем";
                skipped++;
                continue;
            }

            qDebug() << "   ✅ Добавляем файл...";
            if (m_db.addFile(currentFolder, file)) {
                added++;
                qDebug() << "   ✅ Файл успешно добавлен";
            } else {
                skipped++;
                qDebug() << "   ❌ Ошибка при добавлении файла";
            }
        }

        m_db.endBatch();

        qDebug() << "📊 ИТОГО - Добавлено:" << added << "Пропущено:" << skipped;

        m_currentPath = currentFolder;
        refreshAll();

        if (added > 0) {
            NotificationManager::Notification notif;
            notif.type = NotificationManager::TYPE_SUCCESS;
            notif.title = "✅ Файлы загружены";
            notif.message = QString("Добавлено: %1 файл(ов)\nПропущено: %2").arg(added).arg(skipped);
            notif.timestamp = QDateTime::currentDateTime();
            notif.isRead = false;
            notif.isImportant = false;
            notif.folderPath = currentFolder;
            notif.sourceUser = qgetenv("USERNAME");

            statusBar()->showMessage(QString("✅ Добавлено файлов: %1, пропущено: %2").arg(added).arg(skipped), 3000);
        } else {
            if (skipped > 0) {
                statusBar()->showMessage(QString("ℹ️ Все файлы уже есть в папке (пропущено: %1)").arg(skipped), 3000);
            } else {
                statusBar()->showMessage("❌ Нет подходящих файлов для этой папки", 3000);
            }
        }
    }

    event->acceptProposedAction();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent* event)
{
    Q_UNUSED(event);
    ThemeManager& tm = ThemeManager::instance();
    setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(tm.backgroundColor()));
}

// ============================================================
// ТОЧЕЧНОЕ ОБНОВЛЕНИЕ
// ============================================================

void MainWindow::onFolderRemoved(const QString& folderPath)
{
    if (m_batchUpdate) return;
    qDebug() << "🗑️ Точечное удаление папки:" << folderPath;
    refreshTree();
    updateFileTable();
}

// ============================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// ============================================================

void MainWindow::onAddFolderToPath(const QString& parentPath)
{
    if (parentPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите папку!");
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, "Новая папка",
                                         "Введите имя папки:",
                                         QLineEdit::Normal,
                                         "Новая папка", &ok);

    if (!ok || name.isEmpty()) {
        return;
    }

    // Проверяем, нет ли уже такой папки
    for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
        if (it.key().isEmpty()) continue;
        QString path = it.key();
        int lastSlash = path.lastIndexOf('/');
        QString folderParent = (lastSlash != -1) ? path.left(lastSlash) : "";
        if (folderParent == parentPath && it.value().name == name) {
            QMessageBox::warning(this, "Ошибка", "Папка с именем '" + name + "' уже существует!");
            return;
        }
    }

    m_db.beginBatch();
    m_db.addFolder(parentPath, name, TYPE_FOLDER);
    m_db.endBatch();

    refreshTree();
    updateFileTable();

    QString newPath = parentPath + "/" + name;
    findAndSelectFolder(newPath);

    statusBar()->showMessage("✅ Создана папка: " + name, 3000);
}

void MainWindow::onAddFolderToSection(const QString& parentPath, FolderType sectionType)
{
    bool ok;
    QString name = QInputDialog::getText(this, "Новая папка",
                                         "Введите имя папки:",
                                         QLineEdit::Normal,
                                         "Новая папка", &ok);

    if (!ok || name.isEmpty()) {
        return;
    }

    // Проверяем, нет ли уже такой папки
    for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
        if (it.key().isEmpty()) continue;
        QString path = it.key();
        int lastSlash = path.lastIndexOf('/');
        QString folderParent = (lastSlash != -1) ? path.left(lastSlash) : "";
        if (folderParent == parentPath && it.value().name == name) {
            QMessageBox::warning(this, "Ошибка", "Папка с именем '" + name + "' уже существует!");
            return;
        }
    }

    // Определяем тип папки в зависимости от раздела
    FolderType type = TYPE_FOLDER;
    if (sectionType == TYPE_PRODUCT) type = TYPE_PRODUCT;
    else if (sectionType == TYPE_REFERENCE) type = TYPE_REFERENCE;
    else if (sectionType == TYPE_ARCHIVE) type = TYPE_ARCHIVE;
    else if (sectionType == TYPE_TECH_PROCESS) type = TYPE_TECH_PROCESS;
    else type = TYPE_OTHER;

    m_db.beginBatch();
    m_db.addFolder(parentPath, name, type);
    m_db.endBatch();

    refreshTree();
    updateFileTable();

    QString newPath = parentPath.isEmpty() ? name : parentPath + "/" + name;
    findAndSelectFolder(newPath);

    statusBar()->showMessage("✅ Создана папка: " + name, 3000);
}

QString MainWindow::getFolderIcon(FolderType type)
{
    switch(type) {
    case TYPE_PRODUCT: return "📦";
    case TYPE_ASSEMBLY: return "🔧";
    case TYPE_DETAIL: return "⚙️";
    case TYPE_SPEC: return "📋";
    case TYPE_3D: return "🖥️";
    case TYPE_DRAWING: return "📐";
    case TYPE_REFERENCE: return "📚";
    case TYPE_ARCHIVE: return "📦";
    case TYPE_TECH_PROCESS: return "⚙️";
    case TYPE_OTHER: return "📎";
    case TYPE_ROOT_FOLDER: return "📁";
    default: return "📁";
    }
}

QString MainWindow::getSectionPath(FolderType type)
{
    switch(type) {
    case TYPE_PRODUCT: return "Изделия";
    case TYPE_REFERENCE: return "Справочники";
    case TYPE_ARCHIVE: return "Архив";
    case TYPE_TECH_PROCESS: return "Техпроцессы";
    case TYPE_OTHER: return "Другое";
    default: return "";
    }
}
// ============================================================
// ИНИЦИАЛИЗАЦИЯ UI
// ============================================================

void MainWindow::setupUI()
{
    setWindowTitle("КД Менеджер - Система управления документацией");

    // Загружаем сохраненный размер и состояние окна
    QSettings settings("KDManager", "MainWindow");

    bool compactMode = settings.value("ui/compactMode", false).toBool();
    if (compactMode) {
        // Компактный режим: меньшее окно
        resize(1000, 600);
    } else {
        // Нормальный режим
        resize(1400, 800);
    }

    // Восстанавливаем геометрию окна если была сохранена
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("windowState")) {
        restoreState(settings.value("windowState").toByteArray());
    }

    ThemeManager& tm = ThemeManager::instance();
    setStyleSheet(QString(
        "QMainWindow {"
        "   background-color: %1;"
        "}"
        "QMenuBar {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 %2, stop:1 %3);"
        "   border-bottom: 1px solid %4;"
        "   color: %5;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QMenuBar::item {"
        "   padding: 4px 12px;"
        "   background-color: transparent;"
        "}"
        "QMenuBar::item:selected {"
        "   background-color: rgba(96,125,139,0.2);"
        "   border-radius: 4px;"
        "}"
        "QMenu {"
        "   background-color: %6;"
        "   border: 1px solid %7;"
        "   border-radius: 8px;"
        "   padding: 4px;"
        "   color: %5;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QMenu::item {"
        "   padding: 6px 24px;"
        "   border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "   background-color: %8;"
        "}"
        "QStatusBar {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 %2, stop:1 %3);"
        "   border-top: 1px solid %4;"
        "   padding: 2px 12px;"
        "   color: %5;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QToolTip {"
        "   background-color: %6;"
        "   border: 1px solid %7;"
        "   border-radius: 6px;"
        "   color: %5;"
        "   padding: 4px 8px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
    ).arg(tm.backgroundColor(),
          tm.secondaryGradientStart(), tm.secondaryGradientEnd(),
          tm.borderColor(), tm.textColor(),
          tm.cardBackground(), tm.borderColor(),
          tm.hoverColor()));

    QWidget* central = new QWidget(this);
    central->setStyleSheet(QString("QWidget { background-color: %1; }").arg(tm.backgroundColor()));
    setCentralWidget(central);

    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ============================================================
    // ПАНЕЛЬ ИНСТРУМЕНТОВ
    // ============================================================
    QToolBar* toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(32, 32));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolBar->setStyleSheet(QString(
        "QToolBar {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 %1, stop:1 %2);"
        "   border: none;"
        "   border-bottom: 1px solid %3;"
        "   padding: 6px 12px 8px 12px;"
        "   spacing: 2px;"
        "   min-height: 72px;"
        "}"
        "QToolButton {"
        "   background: transparent;"
        "   border: none;"
        "   border-radius: 8px;"
        "   color: %4;"
        "   font-weight: 500;"
        "   font-size: 10px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   padding: 4px 6px;"
        "   min-width: 66px;"
        "   min-height: 54px;"
        "}"
        "QToolButton:hover {"
        "   background: rgba(52, 152, 219, 0.08);"
        "   border: 1px solid rgba(52, 152, 219, 0.20);"
        "}"
        "QToolButton:pressed {"
        "   background: rgba(52, 152, 219, 0.15);"
        "   border: 1px solid rgba(52, 152, 219, 0.35);"
        "}"
    ).arg(tm.currentTheme() == ThemeManager::Dark ? "#2A3F3F" : "#f5f7fa",
          tm.currentTheme() == ThemeManager::Dark ? "#1F2937" : "#eef1f5",
          tm.borderColor(),
          tm.textColor()));

    auto addToolButton = [&](const QString& text, const QIcon& icon, auto slot) {
        QToolButton* btn = new QToolButton(this);
        btn->setText(text);
        btn->setIcon(icon);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(70, 54);
        btn->setIconSize(QSize(24, 24));
        QString btnTextColor = tm.currentTheme() == ThemeManager::Dark ? "#E0E0E0" : "#1A1A1A";
        btn->setStyleSheet(QString(
            "QToolButton {"
            "   background: transparent;"
            "   border: none;"
            "   border-radius: 8px;"
            "   color: %1;"
            "   font-weight: 500;"
            "   font-size: 10px;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "   padding: 4px 6px;"
            "   min-width: 66px;"
            "   min-height: 54px;"
            "}"
            "QToolButton:hover {"
            "   background: rgba(52, 152, 219, 0.08);"
            "   border: 1px solid rgba(52, 152, 219, 0.20);"
            "}"
            "QToolButton:pressed {"
            "   background: rgba(52, 152, 219, 0.15);"
            "   border: 1px solid rgba(52, 152, 219, 0.35);"
            "}"
        ).arg(btnTextColor));
        connect(btn, &QToolButton::clicked, this, slot);
        toolBar->addWidget(btn);
        return btn;
    };

    auto addSeparator = [&]() {
        QFrame* sep = new QFrame();
        sep->setFrameShape(QFrame::VLine);
        sep->setStyleSheet(QString(
            "QFrame {"
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "                               stop:0 transparent,"
            "                               stop:0.2 %1,"
            "                               stop:0.5 %1,"
            "                               stop:0.8 %1,"
            "                               stop:1 transparent);"
            "   width: 1px;"
            "   margin: 6px 2px;"
            "}"
        ).arg(tm.borderColor()));
        sep->setFixedHeight(40);
        toolBar->addWidget(sep);
    };

    m_homeBtn = new QToolButton(this);
    m_homeBtn->setText("Главная");
    m_homeBtn->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    m_homeBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    m_homeBtn->setFixedSize(70, 54);
    m_homeBtn->setIconSize(QSize(24, 24));
    m_homeBtn->setStyleSheet(QString(
        "QToolButton {"
        "   background: transparent;"
        "   border: none;"
        "   border-radius: 8px;"
        "   font-weight: 600;"
        "   color: %1;"
        "   padding: 4px 6px;"
        "   font-size: 10px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QToolButton:hover {"
        "   background: rgba(52, 152, 219, 0.10);"
        "   border: 1px solid rgba(52, 152, 219, 0.25);"
        "}"
        "QToolButton:pressed {"
        "   background: rgba(52, 152, 219, 0.18);"
        "   border: 1px solid rgba(52, 152, 219, 0.40);"
        "}"
    ).arg(tm.textColor()));
    connect(m_homeBtn, &QToolButton::clicked, this, &MainWindow::togglePage);
    toolBar->addWidget(m_homeBtn);

    addSeparator();

    addToolButton("Изделие",
                  style()->standardIcon(QStyle::SP_FileDialogNewFolder),
                  &MainWindow::onNewProduct);

    addToolButton("Раздел",
                  style()->standardIcon(QStyle::SP_DirIcon),
                  &MainWindow::onNewRootFolder);

    addSeparator();

    addToolButton("Файлы",
                  style()->standardIcon(QStyle::SP_FileIcon),
                  &MainWindow::onAddFiles);

    addToolButton("Импорт",
                  style()->standardIcon(QStyle::SP_DriveNetIcon),
                  &MainWindow::onImportFolder);

    addSeparator();

    m_searchBtn = addToolButton("Поиск",
                                style()->standardIcon(QStyle::SP_FileDialogDetailedView),
                                &MainWindow::onSearch);

    m_tasksBtn = addToolButton("Задачи",
                               style()->standardIcon(QStyle::SP_FileDialogListView),
                               &MainWindow::onShowTasks);

    addSeparator();

    addToolButton("Обновить",
                  style()->standardIcon(QStyle::SP_BrowserReload),
                  &MainWindow::onRefreshDB);

    addToolButton("Бэкап",
                  style()->standardIcon(QStyle::SP_DriveHDIcon),
                  &MainWindow::onBackup);

    addToolButton("Настройки",
                  style()->standardIcon(QStyle::SP_ComputerIcon),
                  &MainWindow::onSettings);

    addSeparator();

    // ===== КНОПКА УВЕДОМЛЕНИЙ =====
    m_notificationBtn = new QToolButton(this);
    m_notificationBtn->setText("🔔\nУведомления");
    m_notificationBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    m_notificationBtn->setFixedSize(90, 54);
    m_notificationBtn->setIconSize(QSize(24, 24));
    m_notificationBtn->setStyleSheet(QString(
        "QToolButton {"
        "   background: transparent;"
        "   border: none;"
        "   border-radius: 8px;"
        "   color: %1;"
        "   font-weight: 500;"
        "   font-size: 9px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   padding: 4px 6px;"
        "}"
        "QToolButton:hover {"
        "   background: rgba(52, 152, 219, 0.08);"
        "   border: 1px solid rgba(52, 152, 219, 0.20);"
        "}"
        "QToolButton:pressed {"
        "   background: rgba(52, 152, 219, 0.15);"
        "   border: 1px solid rgba(52, 152, 219, 0.35);"
        "}"
    ).arg(tm.textColor()));
    connect(m_notificationBtn, &QToolButton::clicked, this, &MainWindow::toggleNotificationPanel);
    toolBar->addWidget(m_notificationBtn);

    // Бейдж с количеством уведомлений
    m_badgeLabel = new QLabel(m_notificationBtn);
    m_badgeLabel->setAlignment(Qt::AlignCenter);
    m_badgeLabel->setStyleSheet(QString(
        "QLabel {"
        "   background-color: #f44336;"
        "   color: white;"
        "   border-radius: 9px;"
        "   font-weight: bold;"
        "   font-size: 11px;"
        "   padding: 2px 5px;"
        "}"
    ));
    m_badgeLabel->setFixedSize(18, 18);
    m_badgeLabel->move(72, 2);
    m_badgeLabel->setVisible(false);

    addSeparator();

    addToolButton("Выход",
                  style()->standardIcon(QStyle::SP_DialogCloseButton),
                  &MainWindow::onExit);

    mainLayout->addWidget(toolBar);

    // ============================================================
    // СТЕК СТРАНИЦ
    // ============================================================
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setStyleSheet(QString("QStackedWidget { background-color: %1; }").arg(tm.backgroundColor()));

    m_startupPage = new StartupPage(m_db, this);
    m_stackedWidget->addWidget(m_startupPage);

    m_productTreePage = new ProductTreePage(nullptr);  // Отдельное окно

    connect(m_startupPage, &StartupPage::productSelected, [this](const QString& productPath) {
        m_currentPath = productPath;
        updateFileTable();
        updateStatus();
        showWorkPage();
        findAndSelectFolder(productPath);
    });

    connect(m_startupPage, &StartupPage::showProductTree, [this](const QString& productName, const QString& productPath) {
        m_currentPath = productPath;
        updateFileTable();
        updateStatus();
        showWorkPage();
        findAndSelectFolder(productPath);
    });

    connect(m_startupPage, &StartupPage::openInteractiveTree, [this](const QString& productName, const QString& productPath) {
        m_productTreePage->showProduct(productName);
        m_productTreePage->animateIn();
    });

    connect(m_productTreePage, &ProductTreePage::backRequested, [this]() {
        // Просто закрывается само в closeEvent
    });

    connect(m_startupPage, &StartupPage::createNewProduct, this, &MainWindow::onNewProduct);

    connect(m_startupPage, &StartupPage::showAbout, [this]() {
        if (m_aboutDialog) {
            if (!m_aboutDialog->isVisible()) {
                m_aboutDialog->show();
            }
            m_aboutDialog->raise();
            m_aboutDialog->activateWindow();
            return;
        }

        m_aboutDialog = new AboutDialog(this);
        connect(m_aboutDialog, &AboutDialog::finished, [this]() {
            if (m_aboutDialog) {
                m_aboutDialog->hide();
            }
        });

        m_aboutDialog->show();
    });

    // ============================================================
    // СТРАНИЦА РАБОТЫ
    // ============================================================
    QWidget* workPage = new QWidget(this);
    workPage->setStyleSheet(QString("QWidget { background-color: %1; }").arg(tm.backgroundColor()));
    QVBoxLayout* workLayout = new QVBoxLayout(workPage);
    workLayout->setSpacing(0);
    workLayout->setContentsMargins(0, 0, 0, 0);

    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, workPage);
    mainSplitter->setStyleSheet(QString(
        "QSplitter {"
        "   background-color: %1;"
        "}"
        "QSplitter::handle {"
        "   background-color: %2;"
        "   width: 2px;"
        "}"
    ).arg(tm.backgroundColor(), tm.borderColor()));

    // ===== НОВОЕ КАСТОМНОЕ ДЕРЕВО =====
    QWidget* treeContainer = new QWidget(mainSplitter);
    treeContainer->setStyleSheet(QString("QWidget { background-color: %1; }").arg(tm.backgroundColor()));
    QVBoxLayout* treeLayout = new QVBoxLayout(treeContainer);
    treeLayout->setContentsMargins(8, 4, 8, 4);
    treeLayout->setSpacing(2);

    QLabel* treeLabel = new QLabel("📂 Структура", this);
    treeLabel->setStyleSheet(QString(
        "QLabel {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 %1, stop:1 %2);"
        "   padding: 8px 16px;"
        "   font-weight: 600;"
        "   font-size: 13px;"
        "   border-bottom: 1px solid %3;"
        "   color: %4;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
    ).arg(tm.secondaryGradientStart(), tm.secondaryGradientEnd(),
          tm.borderColor(), tm.textColor()));
    treeLayout->addWidget(treeLabel);

    // Скролл-контейнер для дерева
    m_treeScrollArea = new QScrollArea(treeContainer);
    m_treeScrollArea->setWidgetResizable(true);
    m_treeScrollArea->setStyleSheet(QString(
        "QScrollArea {"
        "   background: %1;"
        "   border: none;"
        "}"
        "QScrollBar:vertical {"
        "   background: transparent;"
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

    m_treeContainer = new QWidget(m_treeScrollArea);
    m_treeContainer->setStyleSheet("background: transparent;");
    m_treeLayout = new QVBoxLayout(m_treeContainer);
    m_treeLayout->setContentsMargins(8, 12, 8, 12);  // ← УВЕЛИЧЕНЫ МАРЖИНЫ
    m_treeLayout->setSpacing(8);  // ← УВЕЛИЧЕН СПЕЙСИНГ С 2 НА 8
    m_treeLayout->setAlignment(Qt::AlignTop);

    m_treeScrollArea->setWidget(m_treeContainer);
    treeLayout->addWidget(m_treeScrollArea);

    mainSplitter->addWidget(treeContainer);
    mainSplitter->setSizes(QList<int>() << 50 << 1050 << 0);

    // ===== КОНТЕЙНЕР КАРТОЧЕК ФАЙЛОВ =====
    QWidget* tableContainer = new QWidget(mainSplitter);
    tableContainer->setStyleSheet(QString("QWidget { background-color: %1; }").arg(tm.backgroundColor()));
    QVBoxLayout* tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(0);

    QLabel* tableLabel = new QLabel("📄 Файлы в папке", this);
    tableLabel->setStyleSheet(QString(
        "QLabel {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 %1, stop:1 %2);"
        "   padding: 8px 16px;"
        "   font-weight: 600;"
        "   font-size: 13px;"
        "   border-bottom: 1px solid %3;"
        "   color: %4;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
    ).arg(tm.secondaryGradientStart(), tm.secondaryGradientEnd(),
          tm.borderColor(), tm.textColor()));
    tableLayout->addWidget(tableLabel);

    m_fileContainer = new FileCardContainer(tableContainer);
    m_fileContainer->setStyleSheet(QString("background-color: %1;").arg(tm.backgroundColor()));

    connect(m_fileContainer, &FileCardContainer::fileSelected,
            [this](const QString& filePath) {
                if (!filePath.isEmpty() && QFile::exists(filePath)) {
                    m_previewWidget->setVisible(true);
                    m_previewWidget->setFile(filePath);

                    QSplitter* splitter = qobject_cast<QSplitter*>(m_previewWidget->parent());
                    if (splitter && splitter->count() >= 3) {
                        splitter->setSizes(QList<int>() << 200 << 400 << 300);
                    }
                }
            });

    connect(m_fileContainer, &FileCardContainer::fileDoubleClicked,
            [this](const QString& filePath) {
                if (!filePath.isEmpty() && QFile::exists(filePath)) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
                }
            });

    connect(m_fileContainer, &FileCardContainer::fileDeleted,
            [this](const QString& filePath) {
                if (!m_currentPath.isEmpty() && m_db.folders().contains(m_currentPath)) {
                    m_db.removeFile(m_currentPath, filePath);
                    refreshAll();
                    m_previewWidget->clear();
                    m_previewWidget->setVisible(false);
                    statusBar()->showMessage("Файл удален", 3000);
                }
            });

    connect(m_fileContainer, &FileCardContainer::fileCommentRequested,
            [this](const QString& filePath) {
                // Получаем полный путь документа из БД
                QString docKey = m_currentPath + "|" + filePath;
                if (m_db.documents().contains(docKey)) {
                    DocInfo& doc = m_db.documents()[docKey];
                    QString fullPath = doc.path;
                    CommentDialog dialog(fullPath, m_currentPath, &m_db, this);
                    dialog.exec();
                } else {
                    QMessageBox::warning(this, "Ошибка", "Документ не найден в БД!");
                }
            });

    tableLayout->addWidget(m_fileContainer);
    mainSplitter->addWidget(tableContainer);

    // ===== ПРЕВЬЮ =====
    m_previewWidget = new PreviewWidget(mainSplitter);
    m_previewWidget->setVisible(false);
    m_previewWidget->setMinimumWidth(200);
    mainSplitter->addWidget(m_previewWidget);

    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);
    mainSplitter->setStretchFactor(2, 0);

    // Проверяем, что в splitter ровно 3 элемента перед установкой свойств
    if (mainSplitter->count() >= 3) {
        mainSplitter->setCollapsible(0, false);
        mainSplitter->setCollapsible(1, false);
        mainSplitter->setCollapsible(2, true);
    }

    workLayout->addWidget(mainSplitter);
    m_stackedWidget->addWidget(workPage);

    mainLayout->addWidget(m_stackedWidget);

    // ===== СТАТУС-БАР =====
    m_statusBar = statusBar();
    setupStatusIndicator();

    // ===== ПАНЕЛЬ УВЕДОМЛЕНИЙ =====
    m_notificationPanel = new NotificationPanel(this);
    m_notificationPanel->connectToManager(NotificationManager::instance());
    qDebug() << "✅ NotificationPanel инициализирована";

    // ===== ПАНЕЛЬ ОНЛАЙН ПОЛЬЗОВАТЕЛЕЙ =====
    m_onlineUsersPanel = new OnlineUsersPanel(this);
    m_onlineUsersPanel->connectToManager(OnlineUsersManager::instance());
    addDockWidget(Qt::RightDockWidgetArea, m_onlineUsersPanel);
    qDebug() << "✅ OnlineUsersPanel инициализирована";

    // ===== МЕНЮ =====
    QMenuBar* menuBar = new QMenuBar(this);
    menuBar->setStyleSheet(QString(
        "QMenuBar {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 %1, stop:1 %2);"
        "   border-bottom: 1px solid %3;"
        "   color: %4;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QMenuBar::item {"
        "   padding: 4px 12px;"
        "   background-color: transparent;"
        "}"
        "QMenuBar::item:selected {"
        "   background-color: rgba(96,125,139,0.2);"
        "   border-radius: 4px;"
        "}"
    ).arg(tm.secondaryGradientStart(), tm.secondaryGradientEnd(),
          tm.borderColor(), tm.textColor()));

    QMenu* fileMenu = menuBar->addMenu("Файл");

    QAction* actionNewProduct = new QAction("🏢 Новое изделие (Ctrl+N)", this);
    actionNewProduct->setShortcut(QKeySequence::New);
    connect(actionNewProduct, &QAction::triggered, this, &MainWindow::onNewProduct);
    fileMenu->addAction(actionNewProduct);

    QAction* actionNewRoot = new QAction("📁 Новый раздел", this);
    connect(actionNewRoot, &QAction::triggered, this, &MainWindow::onNewRootFolder);
    fileMenu->addAction(actionNewRoot);

    QAction* actionNewFolder = new QAction("📂 Новая папка (Ctrl+Shift+N)", this);
    actionNewFolder->setShortcut(QKeySequence("Ctrl+Shift+N"));
    connect(actionNewFolder, &QAction::triggered, this, &MainWindow::onAddFolder);
    fileMenu->addAction(actionNewFolder);

    fileMenu->addSeparator();

    QAction* actionSearch = new QAction("🔍 Поиск (Ctrl+F)", this);
    actionSearch->setShortcut(QKeySequence::Find);
    connect(actionSearch, &QAction::triggered, this, &MainWindow::onSearch);
    fileMenu->addAction(actionSearch);

    fileMenu->addSeparator();

    QAction* actionRefresh = new QAction("🔄 Обновить базу (F5)", this);
    actionRefresh->setShortcut(QKeySequence::Refresh);
    connect(actionRefresh, &QAction::triggered, this, &MainWindow::onRefreshDB);
    fileMenu->addAction(actionRefresh);

    QAction* actionBackup = new QAction("💾 Создать бэкап (Ctrl+Shift+S)", this);
    actionBackup->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(actionBackup, &QAction::triggered, this, &MainWindow::onBackup);
    fileMenu->addAction(actionBackup);

    QAction* actionSettings = new QAction("⚙️ Настройки", this);
    connect(actionSettings, &QAction::triggered, this, &MainWindow::onSettings);
    fileMenu->addAction(actionSettings);

    QAction* actionComments = new QAction("💬 Комментарии (Ctrl+M)", this);
    actionComments->setShortcut(QKeySequence("Ctrl+M"));
    connect(actionComments, &QAction::triggered, this, &MainWindow::onShowComments);
    fileMenu->addAction(actionComments);

    fileMenu->addSeparator();

    QAction* actionExit = new QAction("🚪 Выход (Ctrl+W)", this);
    actionExit->setShortcut(QKeySequence::Close);
    connect(actionExit, &QAction::triggered, this, &MainWindow::onExit);
    fileMenu->addAction(actionExit);

    QMenu* editMenu = menuBar->addMenu("Правка");

    QAction* actionAddFiles = new QAction("📄 Добавить файлы", this);
    connect(actionAddFiles, &QAction::triggered, this, &MainWindow::onAddFiles);
    editMenu->addAction(actionAddFiles);

    QAction* actionImport = new QAction("📥 Импортировать папку", this);
    connect(actionImport, &QAction::triggered, this, &MainWindow::onImportFolder);
    editMenu->addAction(actionImport);

    QMenu* viewMenu = menuBar->addMenu("Вид");
    QAction* actionToggleNotifications = new QAction("🔔 Показать уведомления", this);
    actionToggleNotifications->setShortcut(QKeySequence("Ctrl+Shift+U"));
    connect(actionToggleNotifications, &QAction::triggered, this, &MainWindow::toggleNotificationPanel);
    viewMenu->addAction(actionToggleNotifications);

    QAction* actionToggleOnlineUsers = new QAction("👥 Показать пользователей онлайн", this);
    actionToggleOnlineUsers->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(actionToggleOnlineUsers, &QAction::triggered, [this]() {
        if (m_onlineUsersPanel) {
            if (m_onlineUsersPanel->isVisible()) {
                m_onlineUsersPanel->hide();
            } else {
                m_onlineUsersPanel->show();
            }
        }
    });
    viewMenu->addAction(actionToggleOnlineUsers);

    setMenuBar(menuBar);

    // ===== ДОПОЛНИТЕЛЬНЫЕ ГОРЯЧИЕ КЛАВИШИ =====
    QAction* actionDelete = new QAction(this);
    actionDelete->setShortcut(QKeySequence::Delete);
    connect(actionDelete, &QAction::triggered, this, &MainWindow::onDeleteSelectedFiles);
    addAction(actionDelete);

    QAction* actionHidePreview = new QAction(this);
    actionHidePreview->setShortcut(QKeySequence("Esc"));
    connect(actionHidePreview, &QAction::triggered, [this]() {
        if (m_previewWidget->isVisible()) {
            m_previewWidget->setVisible(false);
            m_previewWidget->clear();

            QSplitter* splitter = qobject_cast<QSplitter*>(m_previewWidget->parent());
            if (splitter && splitter->count() >= 3) {
                splitter->setSizes(QList<int>() << 300 << 500 << 0);
            }
        }
        if (m_isPanelVisible) {
            toggleNotificationPanel();
        }
    });
    addAction(actionHidePreview);

    QAction* actionSelectAll = new QAction(this);
    actionSelectAll->setShortcut(QKeySequence::SelectAll);
    connect(actionSelectAll, &QAction::triggered, [this]() {
        m_fileContainer->selectAll();
    });
    addAction(actionSelectAll);

    updateNotificationBadge(0);
}
void MainWindow::addTreeItemWithStyle(const QString& path, const FolderData& data, int level, bool isExpanded, const QString& style)
{
    QString icon = "📁";
    switch (data.type) {
    case TYPE_3D: icon = "🖥️"; break;
    case TYPE_DRAWING: icon = "📐"; break;
    case TYPE_SPEC: icon = "📋"; break;
    case TYPE_ASSEMBLY: icon = "🔧"; break;
    case TYPE_DETAIL: icon = "⚙️"; break;
    case TYPE_PRODUCT: icon = "📦"; break;
    case TYPE_REFERENCE: icon = "📚"; break;
    case TYPE_ARCHIVE: icon = "📦"; break;
    case TYPE_TECH_PROCESS: icon = "⚙️"; break;
    case TYPE_OTHER: icon = "📎"; break;
    default: icon = "📁"; break;
    }

    bool hasChildren = false;
    const auto& folders = m_db.folders();
    for (auto it = folders.begin(); it != folders.end(); ++it) {
        if (it.key().startsWith(path + "/")) {
            hasChildren = true;
            break;
        }
    }

    TreeItemWidget* itemWidget = new TreeItemWidget(data.name, icon, hasChildren, isExpanded, m_treeContainer);
    itemWidget->setIndentLevel(level);
    itemWidget->setStyleSheet(style);

    if (path == m_currentPath) {
        itemWidget->setSelected(true);
    }

    connect(itemWidget, &TreeItemWidget::clicked, [this, path]() {
        onTreeItemClicked(path);
    });

    connect(itemWidget, &TreeItemWidget::expandToggled, [this, path](bool expanded) {
        m_expandedState[path] = expanded;
        refreshTree();
    });

    itemWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(itemWidget, &QWidget::customContextMenuRequested, [this, path, itemWidget](const QPoint& pos) {
        showTreeContextMenuForPath(path, itemWidget, pos);
    });

    m_treeItemWidgets[path] = itemWidget;
    m_treeLayout->addWidget(itemWidget);
}
// ============================================================
// ���������� ����� ����
// ============================================================

void MainWindow::onThemeChanged()
{
    qDebug() << "=== onThemeChanged ===";

    // ������������� �� ������ � ��������� � ������ �������
    refreshAll();

    // ��������� ���� � �������� ����
    ThemeManager& tm = ThemeManager::instance();

    // ��������� ��� �������� ����
    setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(tm.backgroundColor()));

    // ��������� ���� �������
    if (m_stackedWidget) {
        m_stackedWidget->setStyleSheet(QString("QStackedWidget { background-color: %1; }").arg(tm.backgroundColor()));
    }

    // ���������� ��� �������� ������� � ����� ����
    if (m_startupPage) {
        m_startupPage->update();
    }

    if (m_fileContainer) {
        m_fileContainer->setStyleSheet(QString("background-color: %1;").arg(tm.backgroundColor()));
    }

    if (m_notificationPanel) {
        m_notificationPanel->updateTheme();
    }

    statusBar()->showMessage("? ���� ��������", 2000);
}

// ============================================================
// ��������������� ������ ��� ������������ ������
// ============================================================

QString MainWindow::getHeaderStyle() const
{
    ThemeManager& tm = ThemeManager::instance();
    return QString(
        "QLabel {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 %1, stop:1 %2);"
        "   border-radius: 10px;"
        "   padding: 12px 16px;"
        "   font-size: 14px;"
        "   font-weight: 700;"
        "   color: #FFFFFF;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   margin: 8px 4px;"
        "   border: none;"
        "}"
    ).arg(tm.primaryGradientStart(), tm.primaryGradientEnd());
}

QString MainWindow::getSectionHeaderStyle() const
{
    ThemeManager& tm = ThemeManager::instance();
    return QString(
        "QLabel {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 %1, stop:1 %2);"
        "   border-radius: 10px;"
        "   padding: 12px 16px;"
        "   font-size: 14px;"
        "   font-weight: 700;"
        "   color: #FFFFFF;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   margin: 12px 4px 4px 4px;"
        "   border: none;"
        "}"
    ).arg(tm.accentColor(), tm.accentColor());
}

QString MainWindow::getSeparatorStyle() const
{
    ThemeManager& tm = ThemeManager::instance();
    return QString(
        "QFrame {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "                               stop:0 transparent,"
        "                               stop:0.1 %1,"
        "                               stop:0.9 %1,"
        "                               stop:1 transparent);"
        "   max-height: 1px;"
        "   margin: 8px 12px;"
        "}"
    ).arg(tm.separatorColor());
}

QString MainWindow::getButtonStyle() const
{
    ThemeManager& tm = ThemeManager::instance();
    return QString(
        "QPushButton {"
        "   background-color: %1;"
        "   border: 2px dashed %2;"
        "   border-radius: 8px;"
        "   padding: 10px 14px;"
        "   font-size: 12px;"
        "   font-weight: 500;"
        "   color: %3;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "   border: 2px dashed %5;"
        "}"
    ).arg(tm.cardBackground(), tm.borderColor(), tm.textColor(), 
          tm.hoverColor(), tm.secondaryTextColor());
}

QString MainWindow::getTreeLabelStyle() const
{
    ThemeManager& tm = ThemeManager::instance();
    return QString(
        "QLabel {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 %1, stop:1 %2);"
        "   padding: 8px 16px;"
        "   font-weight: 600;"
        "   font-size: 13px;"
        "   border-bottom: 1px solid %3;"
        "   color: %4;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
    ).arg(tm.secondaryGradientStart(), tm.secondaryGradientEnd(), 
          tm.borderColor(), tm.textColor());
}

QString MainWindow::getBackgroundStyle() const
{
    ThemeManager& tm = ThemeManager::instance();
    return QString("QWidget { background-color: %1; }").arg(tm.backgroundColor());
}
