#include "cli.h"
#include "backend.h"
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTextStream>

int runCli(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Jot: local notes and to-dos. Use 'jot open' for the desktop app.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("collection", "notes | tasks | skill");
    parser.addPositionalArgument("action", "list | add | update | delete; tasks: complete | reopen; skill: install");
    parser.addOptions({{"json", "Print machine-readable JSON."},
                       {"data-dir", "Use a different data directory.", "directory"},
                       {"id", "Note or task ID.", "id"},
                       {"title", "Note title.", "title"},
                       {"body", "Note body.", "body"},
                       {"text", "Task text.", "text"}});
    if (!parser.parse(arguments)) {
        QTextStream(stderr) << parser.errorText() << '\n';
        return 1;
    }
    if (parser.isSet("help")) { QTextStream(stdout) << parser.helpText(); return 0; }
    if (parser.isSet("version")) { QTextStream(stdout) << "jot 0.1.0\n"; return 0; }
    auto output = [&](const QJsonObject &value) {
        QTextStream(stdout) << QJsonDocument(value).toJson(parser.isSet("json") ? QJsonDocument::Compact : QJsonDocument::Indented) << '\n';
    };
    auto error = [&](const QString &message) {
        QTextStream(stderr) << message << '\n';
        if (parser.isSet("json")) output({{"ok", false}, {"error", message}});
        return 1;
    };
    const auto args = parser.positionalArguments();
    if (args.size() != 2) return error("Use 'jot --help' for commands, or 'jot open' for the desktop app.");
    const QString collection = args[0], action = args[1];
    if (collection == "skill" && action == "install") {
        QFile source(":/agent-skill.md");
        if (!source.open(QIODevice::ReadOnly)) return error("Could not read the bundled skill.");
        const QString path = QDir::homePath() + "/.agents/skills/jot/SKILL.md";
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) return error("Could not create the skill directory.");
        QSaveFile file(path);
        const auto data = source.readAll();
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) return error(file.errorString());
        output({{"ok", true}, {"path", path}});
        return 0;
    }
    if (collection != "notes" && collection != "tasks") return error("Unknown collection. Use notes or tasks.");
    const QStringList actions = collection == "notes" ? QStringList{"list", "add", "update", "delete"} : QStringList{"list", "add", "update", "delete", "complete", "reopen"};
    if (!actions.contains(action)) return error("Unknown action. Use 'jot --help'.");
    Backend backend(parser.value("data-dir"));
    if (!backend.error().isEmpty()) return error(backend.error());
    QString id = parser.value("id");
    bool ok = true;
    if (action == "list") {
        output({{"ok", true}, {collection, backend.document().value(collection)}, {"path", backend.dataPath()}});
        return 0;
    }
    if (action != "add" && id.isEmpty()) return error("--id is required.");
    if (collection == "notes") {
        if (action == "add") { id = backend.addNote(parser.value("title"), parser.value("body")); ok = !id.isEmpty(); }
        if (action == "update") {
            if (!parser.isSet("title") && !parser.isSet("body")) return error("Provide --title or --body.");
            ok = backend.selectNote(id);
            if (ok) ok = backend.updateNote(id, parser.isSet("title") ? parser.value("title") : backend.noteTitle(), parser.isSet("body") ? parser.value("body") : backend.noteBody());
        }
        if (action == "delete") ok = backend.deleteNote(id);
    } else {
        if ((action == "add" || action == "update") && !parser.isSet("text")) return error("--text is required.");
        if (action == "add") { id = backend.addTask(parser.value("text")); ok = !id.isEmpty(); }
        if (action == "update") ok = backend.updateTask(id, parser.value("text"));
        if (action == "complete" || action == "reopen") ok = backend.setTaskDone(id, action == "complete");
        if (action == "delete") ok = backend.deleteTask(id);
    }
    if (!ok) return error(backend.error());
    output({{"ok", true}, {"id", id}});
    return 0;
}
