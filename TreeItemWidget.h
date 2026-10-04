#ifndef TREEITEMWIDGET_H
#define TREEITEMWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QLinearGradient>

class TreeItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TreeItemWidget(const QString& text, const QString& icon, bool hasChildren, bool isExpanded, QWidget* parent = nullptr);

    void setExpanded(bool expanded);
    bool isExpanded() const { return m_isExpanded; }
    void setSelected(bool selected);
    bool isSelected() const { return m_isSelected; }
    void setHasChildren(bool hasChildren);
    void setIndentLevel(int level);

    // ===== ПУБЛИЧНЫЕ МЕТОДЫ ДЛЯ РАБОТЫ С ПУТЁМ =====
    void setFolderPath(const QString& path) { setProperty("folderPath", path); }
    QString getFolderPath() const { return property("folderPath").toString(); }

signals:
    void clicked();
    void expandToggled(bool expanded);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUI();
    void updateStyle();

    QLabel* m_iconLabel;
    QLabel* m_textLabel;
    QPushButton* m_expandBtn;

    QString m_text;
    QString m_icon;
    bool m_hasChildren;
    bool m_isExpanded;
    bool m_isSelected;
    bool m_isHovered;
    int m_indentLevel;
};

#endif // TREEITEMWIDGET_H