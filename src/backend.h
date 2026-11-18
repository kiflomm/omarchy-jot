#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <functional>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList notes READ notes NOTIFY changed)
    Q_PROPERTY(QVariantList tasks READ tasks NOTIFY changed)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectionChanged)
    Q_PROPERTY(QString noteTitle READ noteTitle NOTIFY selectionChanged)
    Q_PROPERTY(QString noteBody READ noteBody NOTIFY selectionChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(QString taskFilter READ taskFilter WRITE setTaskFilter NOTIFY changed)
    Q_PROPERTY(int openCount READ openCount NOTIFY changed)
    Q_PROPERTY(int doneCount READ doneCount NOTIFY changed)
    Q_PROPERTY(int noteCount READ noteCount NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString dataPath READ dataPath CONSTANT)

public:
    explicit Backend(const QString &directory = {}, QObject *parent = nullptr);
    QVariantList notes() const;
    QVariantList tasks() const;
    QString selectedId() const { return m_selected; }
    QString noteTitle() const { return m_title; }
    QString noteBody() const { return m_body; }
    QString query() const { return m_query; }
    QString taskFilter() const { return m_filter; }
    int openCount() const;
    int doneCount() const;
    int noteCount() const;
    bool dirty() const { return m_dirty; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString error() const { return m_error; }
    QString dataPath() const { return m_path; }
    QJsonObject document() const { return m_document; }

    void setQuery(const QString &query);
    void setTaskFilter(const QString &filter);
    Q_INVOKABLE QString addNote(const QString &title = {}, const QString &body = {});
    Q_INVOKABLE bool selectNote(const QString &id);
    Q_INVOKABLE void editTitle(const QString &text);
    Q_INVOKABLE void editBody(const QString &text);
    Q_INVOKABLE bool updateNote(const QString &id, const QString &title, const QString &body);
    Q_INVOKABLE bool deleteNote(const QString &id);
    Q_INVOKABLE QString addTask(const QString &text);
    Q_INVOKABLE bool setTaskDone(const QString &id, bool done);
    Q_INVOKABLE bool updateTask(const QString &id, const QString &text);
    Q_INVOKABLE bool deleteTask(const QString &id);
    Q_INVOKABLE bool flush();
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    Q_INVOKABLE bool refresh();
    Q_INVOKABLE void dismissError();

signals:
    void changed();
    void selectionChanged();

private:
    struct Change { QJsonObject before, after; };
    bool read(QJsonObject &document);
    bool mutate(const std::function<bool(QJsonObject &)> &action, bool history = true);
    bool fail(const QString &message);
    bool applyHistory(bool redo);
    void syncSelection();
    void edit();
    QString m_path, m_selected, m_title, m_body, m_query, m_filter = "open", m_error;
    QJsonObject m_document;
    bool m_dirty = false;
    QTimer m_saveTimer, m_refreshTimer;
    QList<Change> m_undo, m_redo;
};
