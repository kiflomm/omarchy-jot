#include "backend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {
QString now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
int find(const QJsonArray &items, const QString &id) {
    for (int i = 0; i < items.size(); ++i)
        if (items[i].toObject().value("id").toString() == id) return i;
    return -1;
}
QJsonObject emptyDocument() {
    return {{"version", 1}, {"notes", QJsonArray()}, {"tasks", QJsonArray()}};
}
bool valid(const QJsonObject &document) {
    if (document.value("version") != 1 || !document.value("notes").isArray() || !document.value("tasks").isArray())
        return false;
    QSet<QString> ids;
    for (const auto &kind : {QString("notes"), QString("tasks")}) {
        for (const auto &value : document.value(kind).toArray()) {
            if (!value.isObject()) return false;
            const auto item = value.toObject();
            const QString id = item.value("id").toString();
            if (id.isEmpty() || ids.contains(id)) return false;
            ids.insert(id);
            if (!item.value("createdAt").isString() || !item.value("updatedAt").isString()) return false;
            if (kind == "notes" && (!item.value("title").isString() || !item.value("body").isString())) return false;
            if (kind == "tasks" && (!item.value("text").isString() || !item.value("done").isBool())) return false;
        }
    }
    return true;
}
}

Backend::Backend(const QString &directory, QObject *parent) : QObject(parent) {
    const auto dir = directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : directory;
    m_path = QDir(dir).filePath("jot.json");
    m_document = emptyDocument();
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &Backend::flush);
    m_refreshTimer.setInterval(1500);
    connect(&m_refreshTimer, &QTimer::timeout, this, &Backend::refresh);
    refresh();
    m_refreshTimer.start();
}

bool Backend::fail(const QString &message) {
    m_error = message;
    emit changed();
    return false;
}

bool Backend::read(QJsonObject &document) {
    QFile file(m_path);
    if (!file.exists()) { document = emptyDocument(); return true; }
    if (!file.open(QIODevice::ReadOnly)) return fail(tr("Could not read your data: %1").arg(file.errorString()));
    QJsonParseError error;
    const auto parsed = QJsonDocument::fromJson(file.readAll(), &error);
    if (file.error() != QFile::NoError) return fail(tr("Could not read your data: %1").arg(file.errorString()));
    if (error.error != QJsonParseError::NoError || !parsed.isObject() || !valid(parsed.object()))
        return fail(tr("Your data file is invalid or from a newer Jot version. It has been left untouched. Location: %1").arg(m_path));
    document = parsed.object();
    return true;
}

bool Backend::refresh() {
    if (m_dirty) return true;
    QJsonObject document;
    if (!read(document)) return false;
    if (document != m_document) {
        m_document = document;
        m_undo.clear();
        m_redo.clear();
        syncSelection();
        emit changed();
    }
    return true;
}

void Backend::syncSelection() {
    const auto notes = m_document.value("notes").toArray();
    int index = find(notes, m_selected);
    if (index < 0 && !notes.isEmpty()) index = 0;
    const auto item = index < 0 ? QJsonObject() : notes[index].toObject();
    m_selected = item.value("id").toString();
    m_title = item.value("title").toString();
    m_body = item.value("body").toString();
    emit selectionChanged();
}

bool Backend::mutate(const std::function<bool(QJsonObject &)> &action, bool history) {
    const QString directory = QFileInfo(m_path).absolutePath();
    if (!QDir().mkpath(directory)) return fail(tr("Could not create the data folder: %1").arg(directory));
    QLockFile lock(m_path + ".lock");
    if (!lock.tryLock(250)) return fail(tr("Another Jot process is saving. Please try again."));
    QJsonObject document;
    if (!read(document)) return false;
    const auto before = document;
    if (!action(document)) return false;
    if (document != before) {
        QSaveFile file(m_path);
        const auto bytes = QJsonDocument(document).toJson();
        if (!file.open(QIODevice::WriteOnly)) return fail(tr("Could not save your changes: %1").arg(file.errorString()));
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        if (file.write(bytes) != bytes.size() || !file.commit())
            return fail(tr("Could not save your changes: %1").arg(file.errorString()));
        if (history) {
            if (before != m_document) m_undo.clear();
            m_undo.append({before, document});
            if (m_undo.size() > 50) m_undo.removeFirst();
            m_redo.clear();
        }
    }
    m_document = document;
    m_error.clear();
    emit changed();
    return true;
}

void Backend::setQuery(const QString &query) {
    if (m_query == query) return;
    m_query = query;
    emit changed();
}

void Backend::setTaskFilter(const QString &filter) {
    if (filter != "all" && filter != "open" && filter != "done") return;
    m_filter = filter;
    emit changed();
}

QVariantList Backend::notes() const {
    QVariantList result;
    for (const auto &value : m_document.value("notes").toArray()) {
        auto item = value.toObject();
        if (item.value("id").toString() == m_selected) {
            item["title"] = m_title;
            item["body"] = m_body;
        }
        const auto title = item.value("title").toString();
        const auto body = item.value("body").toString();
        if (!m_query.isEmpty() && !title.contains(m_query, Qt::CaseInsensitive) && !body.contains(m_query, Qt::CaseInsensitive)) continue;
        item["preview"] = body.simplified().left(100);
        result.append(item.toVariantMap());
    }
    return result;
}

QVariantList Backend::tasks() const {
    QVariantList result;
    for (const auto &value : m_document.value("tasks").toArray()) {
        const auto item = value.toObject();
        const bool done = item.value("done").toBool();
        if ((m_filter == "open" && done) || (m_filter == "done" && !done)) continue;
        if (!m_query.isEmpty() && !item.value("text").toString().contains(m_query, Qt::CaseInsensitive)) continue;
        result.append(item.toVariantMap());
    }
    return result;
}

int Backend::openCount() const {
    int count = 0;
    for (const auto &v : m_document.value("tasks").toArray()) if (!v.toObject().value("done").toBool()) ++count;
    return count;
}
int Backend::doneCount() const { return m_document.value("tasks").toArray().size() - openCount(); }
int Backend::noteCount() const { return m_document.value("notes").toArray().size(); }

QString Backend::addNote(const QString &title, const QString &body) {
    if (!flush()) return {};
    const auto id = uuid(), date = now();
    if (!mutate([&](QJsonObject &doc) {
        auto notes = doc.value("notes").toArray();
        notes.prepend(QJsonObject{{"id", id}, {"title", title}, {"body", body}, {"createdAt", date}, {"updatedAt", date}});
        doc["notes"] = notes;
        return true;
    })) return {};
    m_selected = id;
    syncSelection();
    return id;
}

bool Backend::selectNote(const QString &id) {
    if (id == m_selected) return true;
    if (!flush()) return false;
    if (find(m_document.value("notes").toArray(), id) < 0) return fail(tr("This note no longer exists."));
    m_selected = id;
    syncSelection();
    return true;
}

void Backend::edit() {
    m_dirty = true;
    m_saveTimer.start();
    emit changed();
}
void Backend::editTitle(const QString &text) {
    if (m_selected.isEmpty() || text == m_title) return;
    m_title = text;
    edit();
}
void Backend::editBody(const QString &text) {
    if (m_selected.isEmpty() || text == m_body) return;
    m_body = text;
    edit();
}

bool Backend::flush() {
    m_saveTimer.stop();
    if (!m_dirty) return true;
    // Merge only the edited note under the lock, preserving tasks and notes
    // added by command-line clients while the editor was open.
    if (!mutate([&](QJsonObject &doc) {
        auto notes = doc.value("notes").toArray();
        const int index = find(notes, m_selected);
        if (index < 0) return fail(tr("This note was deleted elsewhere. Copy your text before closing Jot."));
        auto note = notes[index].toObject();
        const int oldIndex = find(m_document.value("notes").toArray(), m_selected);
        if (oldIndex >= 0 && note != m_document.value("notes").toArray()[oldIndex].toObject())
            return fail(tr("This note changed elsewhere. Copy your text before reloading Jot."));
        note["title"] = m_title;
        note["body"] = m_body;
        note["updatedAt"] = now();
        notes[index] = note;
        doc["notes"] = notes;
        return true;
    })) return false;
    m_dirty = false;
    emit changed();
    return true;
}

bool Backend::updateNote(const QString &id, const QString &title, const QString &body) {
    if (!selectNote(id)) return false;
    editTitle(title);
    editBody(body);
    return flush();
}

bool Backend::deleteNote(const QString &id) {
    if (!flush()) return false;
    if (!mutate([&](QJsonObject &doc) {
        auto notes = doc.value("notes").toArray();
        const int index = find(notes, id);
        if (index < 0) return fail(tr("This note no longer exists."));
        notes.removeAt(index);
        doc["notes"] = notes;
        return true;
    })) return false;
    syncSelection();
    return true;
}

QString Backend::addTask(const QString &text) {
    if (text.trimmed().isEmpty()) { fail(tr("Enter a task first.")); return {}; }
    if (!flush()) return {};
    const auto id = uuid(), date = now();
    if (!mutate([&](QJsonObject &doc) {
        auto tasks = doc.value("tasks").toArray();
        tasks.prepend(QJsonObject{{"id", id}, {"text", text.trimmed()}, {"done", false}, {"createdAt", date}, {"updatedAt", date}});
        doc["tasks"] = tasks;
        return true;
    })) return {};
    return id;
}

bool Backend::setTaskDone(const QString &id, bool done) {
    if (!flush()) return false;
    return mutate([&](QJsonObject &doc) {
        auto tasks = doc.value("tasks").toArray();
        const int index = find(tasks, id);
        if (index < 0) return fail(tr("This task no longer exists."));
        auto task = tasks[index].toObject();
        if (task.value("done").toBool() == done) return true;
        task["done"] = done;
        task["updatedAt"] = now();
        tasks[index] = task;
        doc["tasks"] = tasks;
        return true;
    });
}

bool Backend::updateTask(const QString &id, const QString &text) {
    if (text.trimmed().isEmpty()) return fail(tr("Enter a task first."));
    if (!flush()) return false;
    return mutate([&](QJsonObject &doc) {
        auto tasks = doc.value("tasks").toArray();
        const int index = find(tasks, id);
        if (index < 0) return fail(tr("This task no longer exists."));
        auto task = tasks[index].toObject();
        task["text"] = text.trimmed();
        task["updatedAt"] = now();
        tasks[index] = task;
        doc["tasks"] = tasks;
        return true;
    });
}

bool Backend::deleteTask(const QString &id) {
    if (!flush()) return false;
    return mutate([&](QJsonObject &doc) {
        auto tasks = doc.value("tasks").toArray();
        const int index = find(tasks, id);
        if (index < 0) return fail(tr("This task no longer exists."));
        tasks.removeAt(index);
        doc["tasks"] = tasks;
        return true;
    });
}

bool Backend::applyHistory(bool redo) {
    if (!flush()) return false;
    auto &from = redo ? m_redo : m_undo;
    auto &to = redo ? m_undo : m_redo;
    if (from.isEmpty()) return true;
    const auto change = from.last();
    if (!mutate([&](QJsonObject &doc) {
        if (doc != (redo ? change.before : change.after))
            return fail(tr("Your data changed elsewhere. Reload Jot before using undo."));
        doc = redo ? change.after : change.before;
        return true;
    }, false)) return false;
    from.removeLast();
    to.append(change);
    syncSelection();
    emit changed();
    return true;
}
bool Backend::undo() { return applyHistory(false); }
bool Backend::redo() { return applyHistory(true); }
void Backend::dismissError() { m_error.clear(); emit changed(); }
