#ifndef FILECARDWIDGET_H
#define FILECARDWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QFileInfo>
#include <QDateTime>
#include <QMenu>
#include <QAction>

class FileCardWidget : public QFrame
{
    Q_OBJECT

public:
    explicit FileCardWidget(const QString& filePath,
                           const QString& folderPath,
                           const QString& docStatus,
                           const QString& responsible,
                           QWidget* parent = nullptr);
    ~FileCardWidget();

    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    QString getFilePath() const { return m_filePath; }
    void updateFileInfo();

signals:
    void clicked(const QString& filePath);
    void doubleClicked(const QString& filePath);
    void rightClicked(const QPoint& pos, const QString& filePath);
    void deleteRequested(const QString& filePath);
    void commentRequested(const QString& filePath);
    void mouseEntered(const QString& filePath);
    void mouseLeft();
    void previewRequested(const QString& filePath, const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;  // <-- ДОБАВЛЕНО

private:
    void setupUI();
    void updateStyle();
    QString formatFileSize(qint64 size);
    QString getFileIcon(const QString& ext);
    QString getStatusStyle(const QString& status);

    QString m_filePath;
    QString m_fileName;
    QString m_fileExt;
    qint64 m_fileSize;
    QDateTime m_modified;
    QString m_folderPath;
    QString m_docStatus;
    QString m_responsibleUser;

    QLabel* m_iconLabel;
    QLabel* m_nameLabel;
    QLabel* m_infoLabel;
    QLabel* m_statusIndicator;
    QLabel* m_responsibleLabel;
    QLabel* m_checkLabel;

    bool m_selected;
    bool m_hovered;
};

#endif // FILECARDWIDGET_H