#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>
#include "backend.h"
#include "theme.h"
#include "translator.h"

class JotTests : public QObject {
    Q_OBJECT
    static bool write(const QString &path, const QByteArray &bytes) {
        QSaveFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
    }
    static QPoint center(QQuickItem *item) { return item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint(); }
    static QQuickItem *itemNamed(QQuickItem *root, const QString &name) {
        if (root->objectName() == name) return root;
        for (auto *child : root->childItems())
            if (auto *item = itemNamed(child, name)) return item;
        return nullptr;
    }

private slots:
    void initTestCase() { QQuickStyle::setStyle("Material"); }
    void notesPersistAndSearch() {
        QTemporaryDir dir;
        Backend backend(dir.path());
        const auto first = backend.addNote("Ideas", "A useful thought");
        QVERIFY(!first.isEmpty());
        backend.editTitle("Project ideas");
        backend.editBody("A second thought\nwith Unicode: ሰላም");
        QVERIFY(backend.dirty());
        QTRY_VERIFY(!backend.dirty());
        Backend restored(dir.path());
        QCOMPARE(restored.noteTitle(), "Project ideas");
        QCOMPARE(restored.noteBody(), "A second thought\nwith Unicode: ሰላም");
        restored.setQuery("SECOND");
        QCOMPARE(restored.notes().size(), 1);
        restored.setQuery("missing");
        QCOMPARE(restored.notes().size(), 0);
        QVERIFY(restored.deleteNote(first));
        QCOMPARE(restored.noteCount(), 0);
        QVERIFY(restored.undo());
        QCOMPARE(restored.noteCount(), 1);
        QVERIFY(restored.redo());
        QCOMPARE(restored.noteCount(), 0);
    }
    void switchingNotesFlushesDraft() {
        QTemporaryDir dir;
        Backend backend(dir.path());
        const auto first = backend.addNote("First", "");
        const auto second = backend.addNote("Second", "");
        backend.editBody("Do not lose this");
        QVERIFY(backend.selectNote(first));
        QVERIFY(backend.selectNote(second));
        QCOMPARE(backend.noteBody(), "Do not lose this");
        QVERIFY(!backend.dirty());
    }
    void tasksCompleteEditFilterUndo() {
        QTemporaryDir dir;
        Backend backend(dir.path());
        QVERIFY(backend.addTask("   ").isEmpty());
        const auto id = backend.addTask("  Buy coffee  ");
        QVERIFY(!id.isEmpty());
        QCOMPARE(backend.openCount(), 1);
        QVERIFY(backend.setTaskDone(id, true));
        QCOMPARE(backend.openCount(), 0);
        QCOMPARE(backend.doneCount(), 1);
        QCOMPARE(backend.tasks().size(), 0);
        backend.setTaskFilter("done");
        QCOMPARE(backend.tasks().size(), 1);
        QVERIFY(backend.updateTask(id, "Buy tea"));
        QCOMPARE(backend.tasks()[0].toMap()["text"].toString(), "Buy tea");
        QVERIFY(backend.undo());
        QCOMPARE(backend.tasks()[0].toMap()["text"].toString(), "Buy coffee");
        QVERIFY(backend.redo());
        QVERIFY(backend.deleteTask(id));
        QCOMPARE(backend.doneCount(), 0);
        QVERIFY(backend.undo());
        Backend restored(dir.path());
        QCOMPARE(restored.doneCount(), 1);
    }
    void invalidDataIsNeverOverwritten() {
        QTemporaryDir dir;
        const auto path = dir.filePath("jot.json");
        const QByteArray broken("{broken");
        QVERIFY(write(path, broken));
        Backend backend(dir.path());
        QVERIFY(!backend.error().isEmpty());
        QVERIFY(backend.addNote("Attempt", "").isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), broken);
    }
    void saveFailureKeepsDraft() {
        QTemporaryDir dir;
        Backend backend(dir.path());
        QVERIFY(!backend.addNote("Draft", "original").isEmpty());
        QVERIFY(write(dir.filePath("jot.json.lock"), "busy"));
        backend.editBody("Unsaved text");
        QVERIFY(!backend.flush());
        QVERIFY(backend.dirty());
        QCOMPARE(backend.noteBody(), "Unsaved text");
        QVERIFY(!backend.error().isEmpty());
        QVERIFY(QFile::remove(dir.filePath("jot.json.lock")));
        QVERIFY(backend.flush());
        QVERIFY(!backend.dirty());
        Backend restored(dir.path());
        QCOMPARE(restored.noteBody(), "Unsaved text");
    }
    void concurrentClientsMergeAndRejectConflicts() {
        QTemporaryDir dir;
        Backend first(dir.path());
        const auto id = first.addNote("Original", "text");
        Backend second(dir.path());
        first.editBody("My draft");
        QVERIFY(!second.addTask("An external task").isEmpty());
        QVERIFY(first.flush());
        QCOMPARE(first.openCount(), 1);
        QVERIFY(second.refresh());
        first.editBody("Still editing");
        QVERIFY(second.updateNote(id, "External title", "External text"));
        QVERIFY(!first.flush());
        QVERIFY(first.dirty());
        QCOMPARE(first.noteBody(), "Still editing");
        Backend restored(dir.path());
        QCOMPARE(restored.noteBody(), "External text");
    }
    void undoDoesNotOverwriteExternalChanges() {
        QTemporaryDir dir;
        Backend first(dir.path());
        QVERIFY(!first.addTask("First").isEmpty());
        Backend second(dir.path());
        QVERIFY(!second.addTask("Second").isEmpty());
        QVERIFY(!first.undo());
        Backend restored(dir.path());
        QCOMPARE(restored.openCount(), 2);
        QVERIFY(first.refresh());
        QVERIFY(!first.canUndo());
    }
    void themeFollowsAtomicReplacement() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), "#FFD60A");
        QCOMPARE(theme.radius(), 0);
        QVERIFY(QDir(dir.path()).mkpath("theme"));
        QVERIFY(write(dir.filePath("theme/colors.toml"), "accent = '#112233'\n"));
        QTRY_COMPARE(theme.accent(), "#112233");
        QCOMPARE(theme.accentForeground(), "white");
        QVERIFY(write(dir.filePath("theme/colors.toml"), "accent = '#ffffff'\n"));
        QTRY_COMPARE(theme.accent(), "#ffffff");
        QCOMPARE(theme.accentForeground(), "black");
    }
    void themeFollowsHyprlandRounding() {
        QTemporaryDir dir;
        const auto response = dir.filePath("response"), command = dir.filePath("hyprctl");
        QVERIFY(write(response, "{\"int\":0}"));
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }
    void translatorFallbackAndLiveLanguage() {
        QTemporaryDir dir;
        const auto path = dir.filePath("language");
        Translator translator(path, dir.path());
        QCOMPARE(translator.language(), "en");
        QVERIFY(translator.translate("Main", "Save").isNull());
        QVERIFY(write(dir.filePath("pt.json"), R"({"Save":"Guardar", "Open":"Abrir"})"));
        QVERIFY(write(dir.filePath("pt-BR.json"), R"({"Save":"Salvar"})"));
        QVERIFY(write(path, "pt-BR\n"));
        QTRY_COMPARE(translator.language(), "pt-BR");
        QCOMPARE(translator.translate("Main", "Save"), "Salvar");
        QCOMPARE(translator.translate("Main", "Open"), "Abrir");
        QCOMPARE(Translator::pluralCategory("ar", 11), "many");
    }
    void realQmlInteractionAndScreenshots() {
        QTemporaryDir dir;
        Backend backend(dir.path());
        backend.addNote("A slower morning", "Good ideas need a little space.\n\nA few things to remember this week:\n\n• Make time for the small project.\n• Keep the plan simple.\n• Take a walk before opening the laptop.\n\nOne step is enough to get started.");
        backend.addNote("Weekend ideas", "Visit the bookshop. Try a new recipe.");
        backend.selectNote(backend.document()["notes"].toArray()[1].toObject()["id"].toString());
        backend.addTask("Sketch the first version");
        const auto done = backend.addTask("Collect a few ideas");
        backend.setTaskDone(done, true);
        backend.addTask("Set aside an hour to build");
        Theme theme(dir.path());
        Translator translator(dir.filePath("language"));
        qApp->installTranslator(&translator);
        QQmlApplicationEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError> &errors) { for (const auto &e : errors) warnings << e.toString(); });
        connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::qWait(100);
        const QString shots = qEnvironmentVariable("JOT_SCREENSHOT_DIR", "/tmp/jot-screenshots");
        QVERIFY(QDir().mkpath(shots));
        QVERIFY(window->grabWindow().save(shots + "/notes.png"));
        auto *tab = window->findChild<QQuickItem *>("tasksTab");
        QVERIFY(tab);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(tab));
        QCOMPARE(window->property("page").toInt(), 1);
        auto *input = window->findChild<QQuickItem *>("taskInput");
        QVERIFY(input);
        input->forceActiveFocus();
        for (const char c : QByteArray("Ship the little project")) QTest::keyClick(window, c);
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(backend.openCount(), 3);
        QVERIFY(input->property("text").toString().isEmpty());
        QTest::qWait(100);
        auto *check = itemNamed(window->contentItem(), "taskCheck");
        QVERIFY(check);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(check));
        QCOMPARE(backend.openCount(), 2);
        QCOMPARE(backend.doneCount(), 2);
        auto *all = window->findChild<QQuickItem *>("allTasks");
        QVERIFY(all);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(all));
        QTest::keyClick(window, Qt::Key_Escape);
        QTest::qWait(100);
        QVERIFY(window->grabWindow().save(shots + "/tasks.png"));
        QVERIFY(window->setProperty("page", 0));
        auto *body = window->findChild<QQuickItem *>("noteBody");
        QVERIFY(body);
        body->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_Q);
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(window->isVisible());
        QVERIFY(backend.noteBody().endsWith("q "));
        QVERIFY(backend.flush());
        QTest::keyClick(window, Qt::Key_Escape);
        QTest::keyClick(window, Qt::Key_F1);
        QTest::qWait(100);
        auto *help = window->findChild<QObject *>("helpDialog");
        QVERIFY(help);
        QVERIFY(help->property("visible").toBool());
        QVERIFY(window->grabWindow().save(shots + "/help.png"));
        QMetaObject::invokeMethod(help, "close");
        QVERIFY(write(dir.filePath("language"), "da\n"));
        QTRY_COMPARE(translator.language(), "da");
        QTest::qWait(100);
        QVERIFY(window->grabWindow().save(shots + "/danish.png"));
        window->resize(720, 520);
        QTest::qWait(100);
        QVERIFY(window->grabWindow().save(shots + "/compact.png"));
        QVERIFY(write(dir.filePath("language"), "ar\n"));
        QTRY_COMPARE(translator.language(), "ar");
        QCOMPARE(QGuiApplication::layoutDirection(), Qt::RightToLeft);
        QTest::qWait(100);
        QVERIFY(window->grabWindow().save(shots + "/rtl.png"));
        QTest::keyClick(window, Qt::Key_N, Qt::ControlModifier);
        const auto id = backend.selectedId();
        auto *title = window->findChild<QQuickItem *>("noteTitle");
        QVERIFY(title);
        for (const char c : QByteArray("A new note")) QTest::keyClick(window, c);
        QTest::keyClick(window, Qt::Key_Return);
        for (const char c : QByteArray("Some useful text")) QTest::keyClick(window, c);
        QVERIFY(backend.flush());
        Backend restored(dir.path());
        QVERIFY(restored.selectNote(id));
        QCOMPARE(restored.noteTitle(), "A new note");
        QCOMPARE(restored.noteBody(), "Some useful text");
        auto *remove = window->findChild<QQuickItem *>("deleteNote");
        QVERIFY(remove);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(remove));
        auto *confirm = itemNamed(window->contentItem(), "confirmDelete");
        QVERIFY(confirm);
        QTest::qWait(100);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(confirm));
        QCOMPARE(backend.noteCount(), 2);
        QVERIFY(backend.undo());
        QCOMPARE(backend.noteCount(), 3);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        qApp->removeTranslator(&translator);
    }
};

QTEST_MAIN(JotTests)
#include "jot_tests.moc"
