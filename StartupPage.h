#ifndef STARTPAGE_H
#define STARTPAGE_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QMap>
#include <QSet>
#include "DatabaseManager.h"

class StartupPage : public QWidget
{
    Q_OBJECT

public:
    explicit StartupPage(DatabaseManager& db, QWidget *parent = nullptr);

    void loadProducts();
    void addProductCard(const QString& productPath);
    void removeProductCard(const QString& productPath);
    void updateStatistics();

signals:
    void productSelected(const QString& productPath);
    void createNewProduct();
    void showAbout();
    void showProductTree(const QString& productName, const QString& productPath);
    void openInteractiveTree(const QString& productName, const QString& productPath);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onSearchTextChanged(const QString& text);
    void onPinCard(const QString& productPath);
    void onUnpinCard(const QString& productPath);
    void showCardContextMenu(const QPoint& pos);
    void showCardContextMenu(const QString& productPath, const QString& productName, const QPoint& pos);
    void onAboutClicked();  // <-- НОВЫЙ СЛОТ

private:
    void setupUI();
    QFrame* createProductCard(const QString& path, const FolderData& data);
    void savePinnedState();
    void loadPinnedState();

    DatabaseManager& m_db;
    QLineEdit* m_searchEdit;
    QGridLayout* m_productsLayout;
    QScrollArea* m_scrollArea;
    QMap<QString, QFrame*> m_productCards;
    QSet<QString> m_pinnedProducts;
    QString m_currentContextPath;

    QWidget* m_statsContainer;
    QMap<QString, QFrame*> m_statCards;
    QPushButton* m_aboutBtn;  // <-- НОВАЯ КНОПКА
};

#endif // STARTPAGE_H