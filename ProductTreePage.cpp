#include "ProductTreePage.h"
#include "ThemeManager.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QCloseEvent>
#include <QGraphicsDropShadowEffect>

ProductTreePage::ProductTreePage(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("Структура изделия");
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);

    createUI();
    applyTheme();



    // Подключаемся к изменениям темы
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &ProductTreePage::applyTheme);
}

ProductTreePage::~ProductTreePage() = default;

void ProductTreePage::createUI() {
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== ВЕРХНЯЯ ПАНЕЛЬ (шапка) =====
    QWidget* headerWidget = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(24, 16, 24, 16);
    headerLayout->setSpacing(16);

    m_backButton = new QPushButton("← Назад", this);
    m_backButton->setMinimumWidth(110);
    m_backButton->setFixedHeight(40);
    m_backButton->setCursor(Qt::PointingHandCursor);
    connect(m_backButton, &QPushButton::clicked, this, [this]() {
        emit backRequested();
        close();
    });

    m_titleLabel = new QLabel(this);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setStyleSheet("font-size: 20px; font-weight: 700; font-family: 'Segoe UI', 'Arial', sans-serif;");

    headerLayout->addWidget(m_backButton);
    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addStretch();

    // Разделитель под шапкой
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);

    // ===== ОСНОВНОЙ КОНТЕНТ С ДЕРЕВОМ =====
    m_treeView = new InteractiveTreeView(this);

    mainLayout->addWidget(headerWidget);
    mainLayout->addWidget(separator);
    mainLayout->addWidget(m_treeView, 1);

    centralWidget->setLayout(mainLayout);
}

void ProductTreePage::applyTheme() {
    ThemeManager& tm = ThemeManager::instance();

    // Фон главного окна
    setStyleSheet(QString(
        "QMainWindow { background-color: %1; }"
        ).arg(tm.backgroundColor()));

    // Стиль кнопки "Назад"
    m_backButton->setStyleSheet(QString(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "                              stop:0 %1, stop:1 %2);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 10px 16px;"
        "  font-weight: 700;"
        "  font-size: 14px;"
        "  font-family: 'Segoe UI', 'Arial', sans-serif;"
        "  min-width: 100px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "                              stop:0 %3, stop:1 %4);"
        "}"
        "QPushButton:pressed {"
        "  background: %2;"
        "}"
        ).arg(tm.primaryGradientStart(), tm.primaryGradientEnd(),
              tm.accentColor(), tm.primaryGradientStart()));

    // Стиль заголовка
    m_titleLabel->setStyleSheet(QString(
        "QLabel {"
        "  color: %1;"
        "  font-size: 20px;"
        "  font-weight: 700;"
        "  font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        ).arg(tm.textColor()));

    // Стиль разделителя
    QList<QFrame*> frames = findChildren<QFrame*>();
    for (QFrame* frame : frames) {
        if (frame->frameShape() == QFrame::HLine) {
            frame->setStyleSheet(QString(
                "QFrame {"
                "  background-color: %1;"
                "  border: none;"
                "}"
                ).arg(tm.borderColor()));
        }
    }

    // Фон дерева
    m_treeView->setStyleSheet(QString(
        "InteractiveTreeView { background-color: %1; }"
        ).arg(tm.backgroundColor()));
}

void ProductTreePage::showProduct(const QString& productName) {
    m_titleLabel->setText("📦 " + productName);

    // Создаём пример дерева
    TreeNode root = createSampleTree(productName);
    m_treeView->setRootNode(root);
}

void ProductTreePage::animateIn() {
    showFullScreen();  // <-- Вместо showMaximized()
    raise();
    activateWindow();
}

void ProductTreePage::animateOut() {
    // Просто закрываем окно
    close();
}

void ProductTreePage::closeEvent(QCloseEvent* event) {
    emit backRequested();
    event->accept();
}

TreeNode ProductTreePage::createSampleTree(const QString& productName) {
    TreeNode root;
    root.id = "product_1";
    root.name = productName;
    root.type = "product";
    root.expanded = true;

    // Сборка 1: Кузов
    TreeNode assembly1;
    assembly1.id = "assembly_1";
    assembly1.name = "Кузов";
    assembly1.type = "assembly";
    assembly1.expanded = true;

    TreeNode detail1;
    detail1.id = "detail_1";
    detail1.name = "Передняя панель";
    detail1.type = "detail";
    detail1.expanded = true;

    TreeNode detail2;
    detail2.id = "detail_2";
    detail2.name = "Боковые стойки";
    detail2.type = "detail";
    detail2.expanded = true;

    TreeNode detail3;
    detail3.id = "detail_3";
    detail3.name = "Крыша";
    detail3.type = "detail";
    detail3.expanded = true;

    assembly1.children.push_back(detail1);
    assembly1.children.push_back(detail2);
    assembly1.children.push_back(detail3);

    // Сборка 2: Ходовая часть
    TreeNode assembly2;
    assembly2.id = "assembly_2";
    assembly2.name = "Ходовая часть";
    assembly2.type = "assembly";
    assembly2.expanded = true;

    TreeNode detail4;
    detail4.id = "detail_4";
    detail4.name = "Передние колёса";
    detail4.type = "detail";
    detail4.expanded = true;

    TreeNode detail5;
    detail5.id = "detail_5";
    detail5.name = "Задние колёса";
    detail5.type = "detail";
    detail5.expanded = true;

    TreeNode detail6;
    detail6.id = "detail_6";
    detail6.name = "Амортизаторы";
    detail6.type = "detail";
    detail6.expanded = true;

    assembly2.children.push_back(detail4);
    assembly2.children.push_back(detail5);
    assembly2.children.push_back(detail6);

    // Сборка 3: Двигатель
    TreeNode assembly3;
    assembly3.id = "assembly_3";
    assembly3.name = "Двигатель";
    assembly3.type = "assembly";
    assembly3.expanded = true;

    TreeNode detail7;
    detail7.id = "detail_7";
    detail7.name = "Блок цилиндров";
    detail7.type = "detail";
    detail7.expanded = true;

    TreeNode detail8;
    detail8.id = "detail_8";
    detail8.name = "Коленчатый вал";
    detail8.type = "detail";
    detail8.expanded = true;

    TreeNode detail9;
    detail9.id = "detail_9";
    detail9.name = "Система охлаждения";
    detail9.type = "detail";
    detail9.expanded = true;

    assembly3.children.push_back(detail7);
    assembly3.children.push_back(detail8);
    assembly3.children.push_back(detail9);

    // Сборка 4: Электроника
    TreeNode assembly4;
    assembly4.id = "assembly_4";
    assembly4.name = "Электроника";
    assembly4.type = "assembly";
    assembly4.expanded = true;

    TreeNode detail10;
    detail10.id = "detail_10";
    detail10.name = "ЭБУ";
    detail10.type = "detail";
    detail10.expanded = true;

    TreeNode detail11;
    detail11.id = "detail_11";
    detail11.name = "Генератор";
    detail11.type = "detail";
    detail11.expanded = true;

    assembly4.children.push_back(detail10);
    assembly4.children.push_back(detail11);

    root.children.push_back(assembly1);
    root.children.push_back(assembly2);
    root.children.push_back(assembly3);
    root.children.push_back(assembly4);

    return root;
}
