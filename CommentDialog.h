#ifndef COMMENTDIALOG_H
#define COMMENTDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QComboBox>
#include <QLineEdit>
#include <QMap>
#include <QScrollArea>

#include "DocumentStatus.h"
#include "ThemeManager.h"

class DatabaseManager;

class CommentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CommentDialog(const QString& filePath, const QString& folderPath,
                           DatabaseManager* db, QWidget* parent = nullptr);
    virtual ~CommentDialog();

    void setStatus(DocumentStatus status);
    void setResponsibleUser(const QString& user);

private slots:
    void addComment();
    void addReply();
    void deleteComment();
    void changeStatus();
    void assignUser();
    void clearResponsible();
    void deleteCommentWithId(int commentId);
    void onCommentSelected(int commentId);
    void showReplies(int commentId);

private:
    struct CommentEntry {
        int id;
        QString user;
        QString text;
        QDateTime time;
        bool isReply;
        int replyTo;
    };

    void setupUI();
    void loadComments();
    void rebuildCommentList();
    void buildReplyPanel(int commentId);
    void saveComment(const QString& text, bool isReply = false);
    void loadStatusAndResponsible();
    void saveStatusAndResponsible();
    void updateStatusDisplay();
    void applyTheme();
    void keyPressEvent(QKeyEvent* event) override;

    QString m_filePath;
    QString m_folderPath;
    DatabaseManager* m_db;
    DocumentStatus m_currentStatus;
    QString m_responsibleUser;

    QTextEdit* m_commentEdit;
    QPushButton* m_addButton;
    QPushButton* m_statusButton;
    QPushButton* m_assignButton;
    QLabel* m_statusLabel;
    QLabel* m_responsibleLabel;

    // Левая панель (комментарии)
    QWidget* m_leftPanel;
    QVBoxLayout* m_commentListLayout;
    QMap<int, QWidget*> m_commentWidgets;

    // Правая панель (ответы)
    QWidget* m_rightPanel;
    QVBoxLayout* m_rightLayout;
    QLabel* m_selectedCommentTitle;
    QWidget* m_repliesContainer;
    QVBoxLayout* m_repliesLayout;
    QTextEdit* m_replyEdit;

    int m_currentCommentId;
    int m_selectedCommentId;

    QList<CommentEntry> m_comments;
};

#endif // COMMENTDIALOG_H
