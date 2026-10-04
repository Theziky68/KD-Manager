#include "TreeItemWidget.h"
#include "ThemeManager.h"
#include <QStyle>
#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>

TreeItemWidget::TreeItemWidget(const QString& text, const QString& icon, bool hasChildren, bool isExpanded, QWidget* parent)
    : QWidget(parent)
    , m_text(text)
    , m_icon(icon)
    , m_hasChildren(hasChildren)
    , m_isExpanded(isExpanded)
    , m_isSelected(false)
    , m_isHovered(false)
    , m_indentLevel(0)
{
    setupUI();
    setFixedHeight(48);  // ← УВЕЛИЧЕНА С 44 НА 48
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true);
    setProperty("folderPath", "");
}

void TreeItemWidget::setupUI()
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12 + m_indentLevel * 24, 6, 12, 6);
    layout->setSpacing(10);

    // Иконка
    m_iconLabel = new QLabel(m_icon, this);
    m_iconLabel->setFixedSize(28, 28);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 18px;"
        "   background: transparent;"
        "   border: none;"
        "}"
        );
    layout->addWidget(m_iconLabel);

    // Текст
    m_textLabel = new QLabel(m_text, this);
    m_textLabel->setStyleSheet(QString(
        "QLabel {"
        "   font-size: 13px;"
        "   font-weight: 500;"
        "   color: %1;"
        "   background: transparent;"
        "   border: none;"
        "}"
    ).arg(ThemeManager::instance().textColor()));
    m_textLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(m_textLabel, 1);

    // Кнопка раскрытия
    m_expandBtn = new QPushButton(this);
    m_expandBtn->setFixedSize(28, 28);
    m_expandBtn->setCursor(Qt::PointingHandCursor);
    m_expandBtn->setStyleSheet(QString(
        "QPushButton {"
        "   background: transparent;"
        "   border: none;"
        "   border-radius: 14px;"
        "   font-size: 16px;"
        "   color: %1;"
        "   font-weight: bold;"
        "   padding: 0px;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(52, 152, 219, 0.15);"
        "   color: #3498db;"
        "}"
        "QPushButton:pressed {"
        "   background: rgba(52, 152, 219, 0.25);"
        "}"
    ).arg(ThemeManager::instance().secondaryTextColor()));

    if (m_hasChildren) {
        m_expandBtn->setText(m_isExpanded ? "▼" : "▶");
        connect(m_expandBtn, &QPushButton::clicked, [this]() {
            setExpanded(!m_isExpanded);
            emit expandToggled(m_isExpanded);
        });
    } else {
        m_expandBtn->setVisible(false);
    }
    layout->addWidget(m_expandBtn);

    setLayout(layout);
}

void TreeItemWidget::setExpanded(bool expanded)
{
    if (m_isExpanded == expanded) return;
    m_isExpanded = expanded;
    if (m_hasChildren) {
        m_expandBtn->setText(m_isExpanded ? "▼" : "▶");
    }
    update();
}

void TreeItemWidget::setSelected(bool selected)
{
    m_isSelected = selected;
    update();
}

void TreeItemWidget::setHasChildren(bool hasChildren)
{
    m_hasChildren = hasChildren;
    m_expandBtn->setVisible(hasChildren);
    if (hasChildren) {
        m_expandBtn->setText(m_isExpanded ? "▼" : "▶");
    }
    update();
}

void TreeItemWidget::setIndentLevel(int level)
{
    m_indentLevel = level;
    QLayout* layout = this->layout();
    if (layout) {
        layout->setContentsMargins(12 + level * 20, 4, 8, 4);
    }
}

void TreeItemWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect();
    rect.adjust(4, 2, -4, -2);

    ThemeManager& tm = ThemeManager::instance();
    QColor accentColor(52, 152, 219);
    QColor indentLineColor = tm.currentTheme() == ThemeManager::Dark ? QColor(100, 100, 100, 100) : QColor(200, 200, 200, 100);

    // ===== ВЫБРАННЫЙ ЭЛЕМЕНТ =====
    if (m_isSelected) {
        QLinearGradient gradient(0, 0, 0, rect.height());
        gradient.setColorAt(0, QColor(52, 152, 219, 25));
        gradient.setColorAt(1, QColor(52, 152, 219, 15));

        painter.setBrush(gradient);
        painter.setPen(QPen(accentColor, 1.5));
        painter.drawRoundedRect(rect, 8, 8);

        painter.fillRect(QRect(4, rect.top(), 3, rect.height()), accentColor);
    }
    // ===== РАСКРЫТАЯ ВЕТКА =====
    else if (m_isExpanded && m_hasChildren) {
        QColor expandedBg = tm.currentTheme() == ThemeManager::Dark ? QColor(63, 81, 181, 80) : QColor(240, 248, 255, 80);
        QColor expandedBorder = tm.currentTheme() == ThemeManager::Dark ? QColor(99, 102, 241, 100) : QColor(200, 220, 240);
        painter.setBrush(expandedBg);
        painter.setPen(QPen(expandedBorder, 1));
        painter.drawRoundedRect(rect, 8, 8);
    }
    // ===== НАВЕДЕНИЕ =====
    else if (m_isHovered) {
        QLinearGradient gradient(0, 0, 0, rect.height());
        gradient.setColorAt(0, QColor(52, 152, 219, 12));
        gradient.setColorAt(1, QColor(52, 152, 219, 5));

        painter.setBrush(gradient);
        painter.setPen(QPen(QColor(52, 152, 219, 80), 1));
        painter.drawRoundedRect(rect, 8, 8);
    }
    // ===== ОБЫЧНОЕ СОСТОЯНИЕ =====
    else {
        painter.setBrush(Qt::transparent);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect, 8, 8);
    }

    // Рисуем линию индентации
    if (m_indentLevel > 0) {
        painter.setPen(QPen(indentLineColor, 1));
        int lineX = 12 + (m_indentLevel - 1) * 24 + 12;
        painter.drawLine(lineX, rect.top(), lineX, rect.bottom());
    }
}

void TreeItemWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

void TreeItemWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Двойной клик - переключаем раскрытие
        if (m_hasChildren) {
            setExpanded(!m_isExpanded);
            emit expandToggled(m_isExpanded);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TreeItemWidget::enterEvent(QEnterEvent* event)
{
    m_isHovered = true;
    update();
    QWidget::enterEvent(event);
}

void TreeItemWidget::leaveEvent(QEvent* event)
{
    m_isHovered = false;
    update();
    QWidget::leaveEvent(event);
}