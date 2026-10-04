#ifndef PREVIEWWIDGET_H
#define PREVIEWWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QPushButton>
#include <QString>
#include <QProcess>
#include "Model3DViewer.h"

class PreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget *parent = nullptr);
    ~PreviewWidget();

    void setFile(const QString& filePath);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onOpenWithCompass();
    void onOpenWithCompassViewer();
    void onModel3DLoaded();

private:
    void setupUI();
    void loadPreview(const QString& filePath);
    void load3DModel(const QString& filePath);
    QString formatFileSize(qint64 size);
    bool isCompassFile(const QString& filePath) const;
    bool findCompassViewer();
    bool findCompassInstalled();
    void updateButtons(const QString& filePath);
    void checkViewerStatus();
    void testViewer();
    void ensureViewerStatus();
    bool m_viewerStatusChecked;

    // Методы для открытия файлов
    bool openWithCompass(const QString& filePath);
    bool openWithCompassViewer(const QString& filePath);
    bool openWithCompassViewerPortable(const QString& filePath);
    QString getShortPath(const QString& longPath);

    QString m_filePath;
    QString m_viewerPath;
    QString m_compassPath;
    QString m_portableViewerPath;

    QLabel* m_nameLabel;
    QLabel* m_infoLabel;
    QLabel* m_iconLabel;
    QPixmap m_currentPixmap;

    QPushButton* m_openCompassBtn;
    QPushButton* m_openViewerBtn;
    QWidget* m_buttonContainer;
    QLabel* m_statusLabel;

    Model3DViewer* m_model3DViewer;
    QWidget* m_previewContainer;

    bool m_hasViewer;
    bool m_hasCompass;
    bool m_hasPortableViewer;
};

#endif // PREVIEWWIDGET_H