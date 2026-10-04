#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QMenu>
#include <QScrollArea>
#include "DatabaseManager.h"

// Предварительные объявления
class MainWindow;
class TreeItemWidget;

class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchDialog(QWidget *parent = nullptr);

    void setDatabase(DatabaseManager* db) { m_db = db; }
    void setMainWindow(MainWindow* mainWindow);

signals:
    void fileSelected(const QString& filePath, const QString& folderPath);

private slots:
    void onSearch();
    void onItemClicked(QListWidgetItem* item);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onClear();
    void onClose();
    void onSearchTextChanged(const QString& text);
    void showContextMenu(const QPoint& pos);

private:
    void setupUI();
    void applyTheme();
    void highlightInTree(const QString& path);
    void openFile(const QString& filePath, const QString& folderPath);

    DatabaseManager* m_db;
    MainWindow* m_mainWindow;

    QLineEdit* m_searchEdit;
    QPushButton* m_searchBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_closeBtn;
    QListWidget* m_resultsList;
    QLabel* m_statusLabel;
    QTimer* m_searchTimer;
};

#endif // SEARCHDIALOG_H
