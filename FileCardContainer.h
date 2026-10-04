#ifndef FILECARDCONTAINER_H
#define FILECARDCONTAINER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMap>
#include <QList>
#include <QLabel>
#include "FileCardWidget.h"

class DatabaseManager;

class FileCardContainer : public QWidget
{
    Q_OBJECT

public:
    explicit FileCardContainer(QWidget* parent = nullptr);
    ~FileCardContainer();

    void setDatabase(DatabaseManager* db) { m_db = db; }
    void setFolderPath(const QString& folderPath) { m_folderPath = folderPath; }
    void setFiles(const QStringList& files);
    void addFile(const QString& filePath);
    void removeFile(const QString& filePath);
    void clear();
    void selectFile(const QString& filePath);
    void deselectAll();
    void scrollToFile(const QString& filePath);
    QStringList getSelectedFiles() const;
    int count() const { return m_cardWidgets.size(); }
    void selectAll();

signals:
    void fileSelected(const QString& filePath);
    void fileDoubleClicked(const QString& filePath);
    void fileDeleted(const QString& filePath);
    void fileCommentRequested(const QString& filePath);

private slots:
    void onFileClicked(const QString& filePath);
    void onFileDoubleClicked(const QString& filePath);
    void onFileRightClicked(const QPoint& pos, const QString& filePath);

private:
    void setupUI();
    void rebuildLayout();
    void clearLayout();
    void addGroupHeader(const QString& title, const QString& icon, const QString& color);
    void addSeparator();

    QScrollArea* m_scrollArea;
    QWidget* m_containerWidget;
    QVBoxLayout* m_layout;
    QMap<QString, FileCardWidget*> m_cardWidgets;
    QList<QString> m_filePaths;
    QString m_selectedFile;

    QStringList m_modelFiles;
    QStringList m_drawingFiles;
    QStringList m_otherFiles;

    DatabaseManager* m_db;
    QString m_folderPath;
};

#endif
