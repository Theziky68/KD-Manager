#ifndef MODEL3DVIEWER_H
#define MODEL3DVIEWER_H

#include <QWidget>
#include <QString>
#include <QLabel>
#include <QFileInfo>

class Model3DViewer : public QWidget
{
    Q_OBJECT

public:
    explicit Model3DViewer(QWidget *parent = nullptr);
    ~Model3DViewer();

    bool loadModel(const QString& filePath);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_filePath;
    bool m_modelLoaded;
    QLabel* m_label;
};

#endif
