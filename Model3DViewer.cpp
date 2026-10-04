#include "Model3DViewer.h"
#include <QPainter>
#include <QFile>
#include <QDebug>

Model3DViewer::Model3DViewer(QWidget *parent)
    : QWidget(parent)
    , m_modelLoaded(false)
{
    m_label = new QLabel(this);
    m_label->setText("3D Viewer (не поддерживается в этой версии)");
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet(
        "QLabel {"
        "  font-size: 14px;"
        "  color: #666;"
        "  border: 1px solid #ddd;"
        "  border-radius: 4px;"
        "  padding: 20px;"
        "}"
    );

    setMinimumHeight(300);
    m_label->setGeometry(0, 0, width(), height());
}

Model3DViewer::~Model3DViewer()
{
}

bool Model3DViewer::loadModel(const QString& filePath)
{
    if (filePath.isEmpty()) {
        qDebug() << "❌ Model3DViewer: Путь пустой";
        return false;
    }

    if (!QFile::exists(filePath)) {
        qDebug() << "❌ Model3DViewer: Файл не найден:" << filePath;
        m_label->setText("Файл не найден:\n" + filePath);
        return false;
    }

    m_filePath = filePath;
    m_modelLoaded = true;

    qDebug() << "📦 Model3DViewer: Загружаем модель:" << filePath;

    QString fileName = QFileInfo(filePath).fileName();
    m_label->setText("3D Модель:\n" + fileName + "\n\n(Полная поддержка требует OpenGL)");

    return true;
}

void Model3DViewer::clear()
{
    m_modelLoaded = false;
    m_filePath.clear();
    m_label->setText("3D Viewer (готов)");
}

void Model3DViewer::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    if (!m_modelLoaded) {
        painter.fillRect(rect(), Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "Модель не загружена");
    } else {
        painter.fillRect(rect(), Qt::lightGray);
        painter.drawText(rect(), Qt::AlignCenter, "3D Модель:\n" + m_filePath);
    }

    QWidget::paintEvent(event);
}
