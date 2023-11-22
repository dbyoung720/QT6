// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtTest>

#include <QtInsightTracker/qinsightconfiguration.h>
#include <QtInsightTracker/qinsighttracker.h>
#include <QtInsightTracker/private/qinsighteventfilter_p.h>

#include <QtCore/qloggingcategory.h>
#include <QtCore/qjsondocument.h>

#ifdef QTINSIGHT_HAS_WIDGETS
#include <QtWidgets/qwidget.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qpushbutton.h>
#endif

#ifdef QTINSIGHT_HAS_UITOOLS
#include <QtUiTools/quiloader.h>
#endif

#ifdef QTINSIGHT_HAS_QUICK
#include <QtQuick/qquickview.h>
#endif

using namespace Qt::StringLiterals;

struct Event {
    Event(const QString &type = {}, const QString &object = {}, const QString &data = {},
          QPointF position = {}, const QString &screenId = {})
        : type(type), object(object), data(data), position(position), screenId(screenId)
    {}
    QString type;
    QString object;
    QString data;
    QPointF position;
    QString screenId;

    friend inline QDebug operator<<(QDebug dbg, const Event &event) {
        dbg << "Event(" << event.type << event.object << event.data << event.position
            << event.screenId << ")";
        return dbg;
    }
};

class tst_QInsightEventFilter : public QObject
{
    Q_OBJECT
public:
    tst_QInsightEventFilter();

    qsizetype forMatchingEvents(const Event &match,
                                const std::function<void(const Event &)> &func = {}) {
        qsizetype matchCount = 0;
        const QRegularExpression typeRx(match.type);
        const QRegularExpression objectRx(match.object);
        const QRegularExpression dataRx(match.data);
        const QRegularExpression screenRx(match.screenId);
        for (const auto &event : capturedEvents) {
            bool matches = true;
            if (!match.type.isEmpty())
                matches = typeRx.match(event.type).hasMatch();
            if (matches && !match.object.isEmpty())
                matches = objectRx.match(event.object).hasMatch();
            if (matches && !match.data.isEmpty())
                matches = dataRx.match(event.data).hasMatch();
            if (matches && !match.position.isNull())
                matches = match.position == event.position;
            if (matches && !match.screenId.isEmpty())
                matches = screenRx.match(event.screenId).hasMatch();
            if (matches) {
                ++matchCount;
                if (func)
                    func(event);
            }
        }
        return matchCount;
    }

private slots:
    void init();
    void cleanup();
    void initTestCase();
    void cleanupTestCase();

    void generateName();

    void trackEvents_data();
    void trackEvents();

private:
    inline static QRegularExpression regexp = QRegularExpression("^New event \\(\\d+ bytes\\) (?<payload>.*)");
    inline static QtMessageHandler oldMessageHandler = {};
    static void messageHandler(QtMsgType msgType, const QMessageLogContext &logContext, const QString &text)
    {
        if (QByteArrayView(logContext.category) == "qt.insight.events") {
            QRegularExpressionMatch match = regexp.match(text);
            if (match.hasMatch()) {
                QJsonParseError parseError;
                const QStringView payload = match.capturedView(u"payload"_s);
                auto jsonDoc = QJsonDocument::fromJson(payload.toUtf8(), &parseError);
                if (parseError.error == QJsonParseError::NoError)
                    jsonDoc = QJsonDocument::fromJson(jsonDoc["co"].toString().toUtf8(), &parseError);

                if (parseError.error != QJsonParseError::NoError) {
                    qFatal().noquote().nospace() << "Invalid JSON generated for event: " << parseError.error
                                                << " " << parseError.errorString() << "\n"
                                                << "Source (" << parseError.offset << "): " << payload;
                }
                const auto eventObject = jsonDoc["data"].toArray()[0].toObject()["data"].toObject();
                if (eventObject.isEmpty()) {
                    qFatal().noquote().nospace() << "Invalid JSON structure, expected an object, got:\n"
                                                 << jsonDoc;
                } else {
                    const Event event{
                        eventObject["type"].toString(),
                        eventObject["object"].toString(),
                        eventObject["data"].toString(),
                        QPointF(eventObject["position"].toObject()["x"].toDouble(),
                                eventObject["position"].toObject()["y"].toDouble()),
                        eventObject["screen_id"].toString()
                    };
                    capturedEvents.append(event);
                }
            }
            // swallow other output for this category, no need to spam
        } else if (oldMessageHandler) {
            oldMessageHandler(msgType, logContext, text);
        }
    }
    inline static QList<Event> capturedEvents = {};
    QMetaEnum buttonEnum;
    QMetaEnum keysEnum;
    QMetaEnum modifiersEnum;
    QMetaEnum eventEnum;

    inline QPointF pointFromString(QString string) {
        if (string == "$center") {
            if (QWindow *window = qobject_cast<QWindow *>(currentUi.get()))
                return QPointF(window->width() / 2, window->height() / 2);
#ifdef QTINSIGHT_HAS_WIDGETS
            else if (QWidget *widget = qobject_cast<QWidget *>(currentUi.get()))
                return QPointF(widget->width() / 2, widget->height() / 2);
#endif
        }
        QTextStream scanner(&string);
        char slash;
        double x, y;
        scanner >> x >> slash >> y;
        return QPointF(x, y);
    }

    inline void waitForEventPropagation(QWindow *window)
    {
        QTest::mouseMove(window,
                         QPoint(QRandomGenerator::system()->bounded(window->width()),
                                QRandomGenerator::system()->bounded(window->height())));
        QTest::qWait(100);
    }

    std::unique_ptr<QInsightTracker> tracker;
    const QString m_configFile;
    QFileInfoList m_testFiles;
    std::unique_ptr<QObject> currentUi;
    QJsonDocument m_scriptDocument;
};

tst_QInsightEventFilter::tst_QInsightEventFilter()
    : m_configFile(QFINDTESTDATA("testdata/config.json"))
{
    qputenv("QT_INSIGHT_CONFIG", m_configFile.toUtf8());
    tracker.reset(new QInsightTracker);

    auto buttonMO = qt_getEnumMetaObject(Qt::LeftButton);
    buttonEnum = buttonMO->enumerator(buttonMO->indexOfEnumerator("MouseButton"));
    auto keysMO = qt_getEnumMetaObject(Qt::Key_Space);
    keysEnum = keysMO->enumerator(keysMO->indexOfEnumerator("Key"));
    auto modifiersMO = qt_getEnumMetaObject(Qt::NoModifier);
    modifiersEnum = modifiersMO->enumerator(keysMO->indexOfEnumerator("KeyboardModifier"));
    auto eventMO = qt_getEnumMetaObject(QEvent::None);
    eventEnum = eventMO->enumerator(eventMO->indexOfEnumerator("Type"));

    const QDir testDataPath(QFileInfo(m_configFile).absolutePath());
    m_testFiles = testDataPath.entryInfoList({u"*.ui"_s, u"*.qml"_s});
}

void tst_QInsightEventFilter::initTestCase()
{
    QVERIFY(tracker->configuration()->load());
    QVERIFY(tracker->configuration()->isValid());

    oldMessageHandler = qInstallMessageHandler(messageHandler);

    tracker->setEnabled(true);
}

void tst_QInsightEventFilter::cleanupTestCase()
{
    tracker->setEnabled(false);

    qInstallMessageHandler(oldMessageHandler);
    oldMessageHandler = nullptr;
}

void tst_QInsightEventFilter::init()
{
    QLoggingCategory::setFilterRules("qt.insight.events=true");

    tracker->startNewSession();
    capturedEvents.clear();
}

void tst_QInsightEventFilter::cleanup()
{
    currentUi.reset();
    m_scriptDocument = {};
}

void tst_QInsightEventFilter::generateName()
{
#ifdef QTINSIGHT_HAS_WIDGETS
    QWidget parent;
    QFrame frame(&parent);
    QPushButton pushButton("Ok", &frame);
    pushButton.setObjectName("pushButton");

    QCOMPARE(QInsightEventFilter::generateName(&parent), "QWidget");
    QCOMPARE(QInsightEventFilter::generateName(&frame), "QWidget/QFrame");

    frame.setObjectName("frame");

    QCOMPARE(QInsightEventFilter::generateName(&pushButton), "QWidget/frame/pushButton");
#else
    QSKIP("Qt::Widgets not available");
#endif
}

void tst_QInsightEventFilter::trackEvents_data()
{
    QTest::addColumn<QString>("filepath");
    QTest::addColumn<QString>("scriptpath");

    for (const auto &file : m_testFiles) {
        QTest::addRow("%s", file.fileName().toUtf8().constData())
                << file.absoluteFilePath()
                << file.absoluteFilePath() + ".script";
    }
}

void tst_QInsightEventFilter::trackEvents()
{
    QFETCH(QString, filepath);
    QFETCH(QString, scriptpath);

    QWindow *window = nullptr;
    if (filepath.endsWith(u".ui"_s)) {
#ifdef QTINSIGHT_HAS_UITOOLS
        QFile file(filepath);
        QVERIFY(file.open(QFile::ReadOnly));
        QUiLoader loader;
        QWidget *widget = loader.load(&file);
        QVERIFY(widget);
        currentUi.reset(widget);
        widget->show();
        QVERIFY(QTest::qWaitForWindowActive(widget));
        window = widget->windowHandle();
#else
        QSKIP("Qt::UiTools not available");
#endif
    } else if (filepath.endsWith(u".qml"_s)) {
#ifdef QTINSIGHT_HAS_QUICK
        QQuickView *view = new QQuickView;
        view->setSource(QUrl::fromLocalFile(filepath));
        currentUi.reset(view);
        view->show();
        QVERIFY(QTest::qWaitForWindowActive(view));
        window = view;
#else
        QSKIP("Qt::Quick not available");
#endif
    }
    QVERIFY(currentUi);

    QFile scriptFile(scriptpath);
    QVERIFY(scriptFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QJsonParseError parseError;
    m_scriptDocument = QJsonDocument::fromJson(scriptFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qCritical().noquote().nospace() << "Invalid JSON in script file: " << parseError.error
                                        << " " << parseError.errorString() << "\n"
                                        << "Source (" << parseError.offset << "): " << scriptpath;
        QSKIP("Skipping test");
    }
    QVERIFY(m_scriptDocument.isArray());

    QVERIFY(window);
    QTest::qWait(1000);

    QCOMPARE_GE(forMatchingEvents({"Show"}), 1);
    capturedEvents.clear();

    for (const auto step : m_scriptDocument.array()) {
        const auto stepObject = step.toObject();
        const auto actionObject = stepObject["Action"].toObject();
        const auto action = actionObject["type"].toString();
        if (action == "click" || action == "doubleclick") {
            const auto actionData = actionObject["data"].toArray();
            const auto buttonName = actionData[0].toString().toLatin1();
            const auto button = Qt::MouseButton(buttonEnum.keyToValue(buttonName.constData()));
            const auto position = pointFromString(actionData[1].toString());
            const auto modifiers = actionData.count() > 2
                    ? Qt::KeyboardModifier(modifiersEnum.keysToValue(
                            actionData[1].toString().toLatin1().constData()))
                    : Qt::KeyboardModifier{};

            if (actionObject["type"].toString() == "doubleclick")
                QTest::mouseDClick(window, button, modifiers, position.toPoint());
            else
                QTest::mouseClick(window, button, modifiers, position.toPoint());

            waitForEventPropagation(window);
        } else if (action == "key") {
            const auto actionData = actionObject["data"].toArray();
            const Qt::Key key =
                    Qt::Key(keysEnum.keysToValue(actionData[0].toString().toLatin1().constData()));
            const auto modifiers = actionData.count() > 1
                    ? Qt::KeyboardModifier(modifiersEnum.keysToValue(
                            actionData[1].toString().toLatin1().constData()))
                    : Qt::KeyboardModifier{};

            QTest::keyClick(window, key, modifiers);
            waitForEventPropagation(window);
        }
        const auto eventArray = stepObject["Events"].toArray();
        for (const auto event : eventArray) {
            const auto eventObject = event.toObject();
            QPointF position;
            if (eventObject["position"].isString())
                position = window->mapToGlobal(pointFromString(eventObject["position"].toString()));
            const Event expected{
                eventObject["type"].toString(),
                eventObject["object"].toString(),
                eventObject["data"].toString(),
                position,
                eventObject["screen"].toString()
            };
            auto failLogger = qScopeGuard([action, expected] {
                qDebug() << "action" << action << "expected" << expected;
                for (const auto &event : capturedEvents)
                    qDebug() << "tracked" << event;
            });
            QCOMPARE(forMatchingEvents(expected), eventObject["events"].toInt(1));
            failLogger.dismiss();
        }
        capturedEvents.clear();
    }
}

QTEST_MAIN(tst_QInsightEventFilter)

#include "tst_qinsighteventfilter.moc"
