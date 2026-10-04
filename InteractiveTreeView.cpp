#include "InteractiveTreeView.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLinearGradient>
#include <QFont>
#include <QTransform>

InteractiveTreeView::InteractiveTreeView(QWidget* parent)
    : QWidget(parent), m_offset(0, 0), m_velocity(0, 0) {
    setFocusPolicy(Qt::StrongFocus);

    m_inertiaTimer = new QTimer(this);
    connect(m_inertiaTimer, &QTimer::timeout, this, &InteractiveTreeView::updateInertia);

    // Подключаемся к изменениям темы
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        update();
    });
}

InteractiveTreeView::~InteractiveTreeView() = default;

void InteractiveTreeView::setRootNode(const TreeNode& root) {
    m_root = root;
    resetView();
}

void InteractiveTreeView::resetView() {
    m_offset = QPoint(0, 0);
    m_velocity = QPoint(0, 0);
    m_inertiaTimer->stop();
    update();
}

void InteractiveTreeView::layoutNodes(TreeNode& node, int level, int& yPos, int xOffset) {
    node.screenPos = QPoint(xOffset, yPos);
    yPos += NODE_HEIGHT + 60;  // Больше расстояния между уровнями

    if (node.expanded && !node.children.isEmpty()) {
        // Детали одного уровня расположены в ряд, центрированы под родителем
        int childCount = node.children.size();
        int totalWidth = childCount * (NODE_WIDTH + NODE_SPACING) - NODE_SPACING;
        int startX = xOffset - totalWidth / 2;

        for (int i = 0; i < childCount; ++i) {
            int childX = startX + i * (NODE_WIDTH + NODE_SPACING);
            layoutNodes(node.children[i], level + 1, yPos, childX);
        }
    }
}

void InteractiveTreeView::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    ThemeManager& tm = ThemeManager::instance();
    painter.fillRect(rect(), tm.backgroundColor());

    // Пересчитаем позиции
    int yPos = 50;
    layoutNodes(m_root, 0, yPos, width() / 2);

    // Сохраняем трансформацию
    painter.save();

    // Рисуем соединительные линии
    std::function<void(const TreeNode&)> drawLines = [&](const TreeNode& node) {
        for (const auto& child : node.children) {
            drawLine(painter, node.screenPos + m_offset, child.screenPos + m_offset);
            drawLines(child);
        }
    };
    drawLines(m_root);

    // Рисуем узлы с 3D эффектом
    std::function<void(const TreeNode&, int)> drawNodes = [&](const TreeNode& node, int depth) {
        drawNode(painter, node, depth);
        for (const auto& child : node.children) {
            drawNodes(child, depth + 1);
        }
    };
    drawNodes(m_root, 0);

    painter.restore();
}

void InteractiveTreeView::drawNode(QPainter& painter, const TreeNode& node, int depth) {
    QPoint basePos = node.screenPos + m_offset;

    // Цвета по типу узла
    QColor bgColor;
    QColor textColor = Qt::white;

    ThemeManager& tm = ThemeManager::instance();
    if (node.type == "product") {
        bgColor = QColor(59, 130, 246);   // Синий
        textColor = Qt::white;
    } else if (node.type == "assembly") {
        bgColor = QColor(34, 197, 94);    // Зелёный
        textColor = Qt::white;
    } else {
        bgColor = QColor(139, 92, 246);   // Фиолетовый
        textColor = Qt::white;
    }

    // ===== 3D ЭФФЕКТ: ТРАПЕЦИЯ =====
    // Узлы глубже становятся уже (эффект перспективы)
    float scaleX = 1.0f - (depth * 0.15f);  // 15% уменьшения на каждый уровень
    if (scaleX < 0.5f) scaleX = 0.5f;

    int scaledWidth = NODE_WIDTH * scaleX;
    int widthDiff = (NODE_WIDTH - scaledWidth) / 2;

    // Трапеция для 3D эффекта
    QPolygonF trapezoid;
    trapezoid << QPointF(basePos.x(), basePos.y())                              // верх-лево
              << QPointF(basePos.x() + NODE_WIDTH, basePos.y())                // верх-право
              << QPointF(basePos.x() + NODE_WIDTH - widthDiff, basePos.y() + NODE_HEIGHT)  // низ-право
              << QPointF(basePos.x() + widthDiff, basePos.y() + NODE_HEIGHT);  // низ-лево

    // Градиент
    QLinearGradient gradient(QPointF(basePos.x(), basePos.y()),
                            QPointF(basePos.x(), basePos.y() + NODE_HEIGHT));

    // На глубине цвет темнеет
    int brightnessAdjust = 100 + (depth * 5);
    gradient.setColorAt(0, bgColor.lighter(brightnessAdjust));
    gradient.setColorAt(0.5, bgColor);
    gradient.setColorAt(1, bgColor.darker(110 + depth * 3));

    // Рисуем трапецию (карточка)
    painter.save();
    painter.setBrush(gradient);
    painter.setPen(QPen(bgColor.darker(150), 2));
    painter.drawPolygon(trapezoid);
    painter.restore();

    // Тень для глубины (полупрозрачный слой)
    painter.save();
    QColor shadowColor = QColor(0, 0, 0, 30 + depth * 10);
    painter.setBrush(shadowColor);
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(trapezoid);
    painter.restore();

    // Текст - центрируем в трапеции
    QRectF textRect(basePos.x() + widthDiff * 0.5, basePos.y(),
                    NODE_WIDTH - widthDiff, NODE_HEIGHT);

    painter.setPen(textColor);
    QFont font = painter.font();
    font.setPointSize(9 - depth);  // Шрифт меньше на глубине
    if (font.pointSize() < 7) font.setPointSize(7);
    font.setBold(true);
    painter.setFont(font);

    // Сокращённый текст если не влезает
    QString displayName = node.name;
    if (displayName.length() > 18) {
        displayName = displayName.left(15) + "...";
    }

    painter.drawText(textRect, Qt::AlignCenter, displayName);

    // Иконка развёртывания
    if (!node.children.isEmpty()) {
        QRectF expandRect(textRect.right() - 18, textRect.top() + 2, 14, 14);
        painter.setPen(QPen(textColor, 1));
        painter.drawRect(expandRect);

        QString symbol = node.expanded ? "▼" : "▶";
        painter.drawText(expandRect, Qt::AlignCenter, symbol);
    }
}

void InteractiveTreeView::drawLine(QPainter& painter, QPoint from, QPoint to) {
    ThemeManager& tm = ThemeManager::instance();
    QColor lineColor = tm.currentTheme() == ThemeManager::Dark
        ? QColor(100, 116, 139)
        : QColor(203, 213, 225);

    painter.setPen(QPen(lineColor, 2, Qt::SolidLine));

    // Кривая (Безье) вместо прямой для эффекта течения
    QPoint mid1((from.x() + to.x()) / 2, from.y() + (to.y() - from.y()) / 3);
    QPoint mid2((from.x() + to.x()) / 2, from.y() + 2 * (to.y() - from.y()) / 3);

    QPainterPath path;
    path.moveTo(from);
    path.cubicTo(mid1, mid2, to);
    painter.drawPath(path);
}

void InteractiveTreeView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->pos();
        m_inertiaTimer->stop();
    }
}

void InteractiveTreeView::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_offset += delta;
        m_lastMousePos = event->pos();
        update();
    }
}

void InteractiveTreeView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        m_velocity = event->pos() - m_lastMousePos;
        m_inertiaTimer->start(16);  // ~60 FPS
    }
}

void InteractiveTreeView::wheelEvent(QWheelEvent* event) {
    int delta = event->angleDelta().y() > 0 ? 30 : -30;
    m_offset.setY(m_offset.y() + delta);
    update();
}

void InteractiveTreeView::updateInertia() {
    if (m_velocity.manhattanLength() < 1) {
        m_inertiaTimer->stop();
        return;
    }

    m_offset += m_velocity;
    m_velocity *= FRICTION;
    update();
}

void InteractiveTreeView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

TreeNode* InteractiveTreeView::findNodeAt(TreeNode& node, const QPoint& pos) {
    QRect rect(node.screenPos + m_offset, QSize(NODE_WIDTH, NODE_HEIGHT));
    if (rect.contains(pos)) {
        return &node;
    }

    for (auto& child : node.children) {
        auto* found = findNodeAt(child, pos);
        if (found) return found;
    }
    return nullptr;
}
