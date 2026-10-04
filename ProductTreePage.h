#ifndef PRODUCTTREEPAGE_H
#define PRODUCTTREEPAGE_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include "InteractiveTreeView.h"

class ProductTreePage : public QMainWindow {
    Q_OBJECT

public:
    explicit ProductTreePage(QWidget* parent = nullptr);
    ~ProductTreePage();

    void showProduct(const QString& productName);
    void animateIn();
    void animateOut();

signals:
    void backRequested();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void applyTheme();

private:
    InteractiveTreeView* m_treeView;
    QPushButton* m_backButton;
    QLabel* m_titleLabel;

    void createUI();
    TreeNode createSampleTree(const QString& productName);
};

#endif
