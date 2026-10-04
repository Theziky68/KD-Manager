#ifndef INTERACTIVETREEVIEW_H
#define INTERACTIVETREEVIEW_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QPoint>
#include <QTimer>
#include <QPropertyAnimation>

struct TreeNode {
    QString id;
    QString name;
    QString type;  // "product", "assembly", "detail"
    QVector<TreeNode> children;
    QPoint screenPos;
    bool expanded = true;
};

class InteractiveTreeView : public QWidget {
    Q_OBJECT

public:
    explicit InteractiveTreeView(QWidget* parent = nullptr);
    ~InteractiveTreeView();

    void setRootNode(const TreeNode& root);
    void resetView();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    static constexpr int LEVEL_SPACING = 150;
    static constexpr int NODE_SPACING = 100;
    static constexpr int NODE_WIDTH = 200;
    static constexpr int NODE_HEIGHT = 70;
    static constexpr float FRICTION = 0.92f;

    TreeNode m_root;
    QPoint m_offset;           // Смещение для скролла
    QPoint m_lastMousePos;
    QPoint m_velocity;         // Скорость инерции
    bool m_isDragging = false;
    QTimer* m_inertiaTimer;

    void layoutNodes(TreeNode& node, int level, int& yPos, int xOffset);
    void drawNode(QPainter& painter, const TreeNode& node, int depth);
    void drawLine(QPainter& painter, QPoint from, QPoint to);
    void applyInertia();
    TreeNode* findNodeAt(TreeNode& node, const QPoint& pos);

private slots:
    void updateInertia();
};

#endif
