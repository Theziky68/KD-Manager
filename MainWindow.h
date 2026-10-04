#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QMimeData>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QMap>
#include <QLabel>
#include <QPushButton>
#include "DatabaseManager.h"
#include "SearchDialog.h"
#include "StartupPage.h"
#include "FileCardContainer.h"
#include "TreeItemWidget.h"

// ===== ДОБАВЛЯЕМ INCLUDE =====
#include "AboutDialog.h"
#include "NotificationPanel.h"
#include "ProductTreePage.h"
#include <QGuiApplication>

class PreviewWidget;
class QPushButton;
class QStatusBar;
class QTimer;
class ChangesPanel;
class QTreeWidgetItem;
class QPoint;
class QToolButton;
class QStackedWidget;
class QLabel;
class QPropertyAnimation;
class QCloseEvent;
class OnlineUsersPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void showWorkPageAndSelectFile(const QString& filePath, const QString& folderPath);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;

private slots:
    void onAutoSave();
    void onAutoRefresh();
    void onImportFolderFromPath(const QString& dirPath);

    void onNewProduct();
    void onNewRootFolder();
    void onAddFiles();
    void onImportFolder();
    void onAddFolder();
    void onDeleteFile(const QString& filePath, const QString& folderPath);
    void onDeleteFolder(const QString& path);
    void onRenameFolder(const QString& path);
    void onEditComment(const QString& filePath);
    void onSearch();
    void onShowTasks();
    void onRefreshDB();
    void onBackup();
    void onSettings();
    void onExit();
    void onShowComments();
    void onDeleteSelectedFiles();
    void onDeleteSection(const QString& path);

    void onTreeItemClicked(const QString& path);
    void onTreeItemExpandToggled(const QString& path, bool expanded);
    void showTreeContextMenu(const QPoint& pos);

    void togglePage();
    void toggleNotificationPanel();
    void updateStatusIndicator(const QString& text, const QString& color);

    void onFolderRemoved(const QString& folderPath);

    void onThemeChanged();

private:
    void setupUI();

    // Методы для генерации динамических стилей
    QString getHeaderStyle() const;
    QString getSectionHeaderStyle() const;
    QString getSeparatorStyle() const;
    QString getButtonStyle() const;
    QString getTreeLabelStyle() const;
    QString getBackgroundStyle() const;
    void setupNotificationButton();
    void setupStatusIndicator();
    QString getSectionPath(FolderType type);
    void setupShortcuts();
    void importFolderRecursive(const QString& dirPath);
    void restoreLastFolder();
    void onAddFolderToPath(const QString& parentPath);
    void onAddFolderToSection(const QString& parentPath, FolderType sectionType);

    void refreshAll();
    void refreshTree();
    void clearTreeContainer();
    void addTreeItem(const QString& path, const FolderData& data, int level, bool isExpanded = false);
    void addTreeItemWithColor(const QString& path, const FolderData& data, int level, bool isExpanded, const QColor& bgColor);  // <-- ДОБАВЛЕНО
    void buildTreeRecursive(const QString& parentPath, int level);
    void buildTreeRecursiveWithGroups(const QString& parentPath, int level);  // <-- ДОБАВЛЕНО
    void findAndSelectFolder(const QString& folderPath);
    void selectTreeItem(const QString& path);
    void updateTreeSelection(const QString& path);
    void showTreeContextMenuForPath(const QString& path, TreeItemWidget* itemWidget, const QPoint& pos);  // <-- ДОБАВЛЕНО
    void addTreeItemWithStyle(const QString& path, const FolderData& data, int level, bool isExpanded, const QString& style);
    void updateFileTable();
    void updateStatus();
    void loadStartupPage();
    void showStartupPage();
    void showWorkPage();

    QString formatFileSize(qint64 size);
    void highlightFileInTable(const QString& filePath);
    void updateNotificationBadge(int count);
    QString getFolderIcon(FolderType type);

    // ===== БАЗА ДАННЫХ =====
    DatabaseManager& m_db;

    // Новое кастомное дерево
    QScrollArea* m_treeScrollArea;
    QWidget* m_treeContainer;
    QVBoxLayout* m_treeLayout;
    QMap<QString, TreeItemWidget*> m_treeItemWidgets;
    QMap<QString, bool> m_expandedState;
    bool m_isUpdatingTree;

    FileCardContainer* m_fileContainer;
    PreviewWidget* m_previewWidget;
    QStatusBar* m_statusBar;

    QToolButton* m_searchBtn;
    QToolButton* m_tasksBtn;
    QToolButton* m_homeBtn;
    QToolButton* m_notificationBtn;
    QLabel* m_badgeLabel;

    QStackedWidget* m_stackedWidget;
    StartupPage* m_startupPage;
    QWidget* m_workPage;
    ProductTreePage* m_productTreePage;
    ChangesPanel* m_changesPanel;

    SearchDialog* m_searchDialog;
    AboutDialog* m_aboutDialog;
    NotificationPanel* m_notificationPanel;
    QDockWidget* m_notificationDock;

    OnlineUsersPanel* m_onlineUsersPanel;

    QTimer* m_autoSaveTimer;
    QTimer* m_autoRefreshTimer;
    QTimer* m_saveAnimationTimer;
    int m_saveAnimationDots;

    QString m_currentPath;
    QString m_selectedPath;
    QStringList m_expandedPaths;
    bool m_isOnStartupPage;
    bool m_isRefreshing;
    bool m_isPanelVisible;
    bool m_batchUpdate;
    QPropertyAnimation* m_panelAnimation;

    QLabel* m_statusIndicator;
    QLabel* m_userIndicator;
};

#endif // MAINWINDOW_H