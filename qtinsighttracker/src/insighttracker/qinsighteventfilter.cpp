// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtInsightTracker/private/qinsighteventfilter_p.h>
#include <QtInsightTracker/private/qinsightcategoryfilter_p.h>
#include <QtInsightTracker/private/qinsighttracker_p.h>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMetaEnum>
#include <QtGui/QGuiApplication>
#include <QtGui/QtEvents>
#include <QtGui/QWindow>
//#include <QtWidgets/qgesture.h>

Q_LOGGING_CATEGORY(lcInsightEventFilter, "qt.insight.eventfilter", QtDebugMsg)

QInsightEventFilter::QInsightEventFilter(InsightTrackerImpl *tracker)
    : m_tracker(tracker)
{
    qCDebug(lcInsightEventFilter) << "Installing event filter";
    if (qApp) {
        qApp->installEventFilter(this);
    } else {
        qCCritical(lcInsightEventFilter) << "QGuiApplication needs to be created before the Qt "
                                            "Insight Tracker can be initialized";
    }

    const auto *metaObject = qt_getEnumMetaObject(QEvent::None);
    const auto me = metaObject->enumerator(metaObject->indexOfEnumerator("Type"));
    for (const auto &event : tracker->configuration()->events() ) {
        bool ok = false;
        const auto key = me.keyToValue(event.toLatin1().constData(), &ok);
        if (ok)
            m_events.insert(key, event);
        else
            qCWarning(lcInsightEventFilter) << "unknown event" << event;
    }
    if (!m_events.isEmpty())
        qCInfo(lcInsightEventFilter) << "tracking events" << m_events.values();
}

void QInsightEventFilter::setCategoryFilter(QInsightCategoryFilter *filter)
{
    m_categoryFilter = filter;
}

QString QInsightEventFilter::generateName(QObject *object)
{
    QString objectName;
    // generate a unique name for the object based on its place in the object tree

    if (!object->objectName().isEmpty())
        objectName = object->objectName();
    else
        objectName = QString::fromUtf8(object->metaObject()->className());

    if (object->parent())
        objectName = generateName(object->parent()) + u"/" + objectName;
    return objectName;
}

namespace {
template <class QEnum>
static inline QString enumToString(const QEnum value)
{
    const auto *metaObject = qt_getEnumMetaObject(value);
    const auto me = metaObject->enumerator(metaObject->indexOfEnumerator(qt_getEnumName(value)));
    return QString::fromLatin1(me.valueToKey(int(value)));
}
} // anonymous namespace

bool QInsightEventFilter::isTracked(QEvent *event)
{
    return m_events.contains(event->type());
}

bool QInsightEventFilter::eventFilter(QObject *receiver, QEvent *event)
{
     if (m_pendingInput) {
        bool send = true;
        if (event->isInputEvent()) {
            QInputEvent *e = static_cast<QInputEvent *>(event);
            send = (m_type != e->type() || m_timestamp != e->timestamp());
        }
        if (send) {
            m_tracker->sendEvent(enumToString(m_type), m_receiver, m_coordinates, m_data);
            m_pendingInput = false;
        }
    }

    if (!isTracked(event))
        return false;

    // send last Resize event
    if (m_pendingResize && event->type() != QEvent::Resize) {
        m_tracker->sendEvent(enumToString(QEvent::Resize), QString(), m_coordinates);
        m_pendingResize = false;
    }

    if (m_categoryFilter) {
        QString category = m_categoryFilter->objectCategory(receiver);
        if (!category.isEmpty() && !m_tracker->configuration()->categories().contains(category)) {
            qCDebug(lcInsightEventFilter) << "category filtered:" << category;
            // if input event was propagated to this object, don't send it.
            m_pendingInput = false;
            return false;
        }
    }

    switch (event->type()) {
    // QSinglePointEvent
    case QEvent::MouseMove:
    case QEvent::NonClientAreaMouseMove:
    case QEvent::HoverMove:
    case QEvent::HoverEnter:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::NonClientAreaMouseButtonPress:
    case QEvent::NonClientAreaMouseButtonDblClick:
    case QEvent::NonClientAreaMouseButtonRelease:
    case QEvent::Wheel:
    case QEvent::Enter:
    {
        QSinglePointEvent *e = static_cast<QSinglePointEvent *>(event);
        m_receiver = generateName(receiver);
        m_data = enumToString(e->button());
        m_coordinates = InsightTracker::EventCoordinates(e->globalPosition().x(),
                                                         e->globalPosition().y());
        m_timestamp = e->timestamp();
        m_type = e->type();
        m_pendingInput = true;
        break;
    }

    case QEvent::Leave:
    case QEvent::DragLeave:
    case QEvent::HoverLeave:
        m_tracker->sendEvent(enumToString(event->type()), generateName(receiver));
        break;

    // QTouchEvent
    case QEvent::TouchUpdate:
    case QEvent::TouchBegin:
    case QEvent::TouchCancel:
    case QEvent::TouchEnd:
    {
        QTouchEvent *e = static_cast<QTouchEvent *>(event);
        m_tracker->sendEvent(
                enumToString(e->type()), generateName(receiver),
                InsightTracker::EventCoordinates(e->points().constFirst().scenePosition().x(),
                                                 e->points().constFirst().scenePosition().y()));
        break;
    }


    // QFocusEvent
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    {
        QFocusEvent *e = static_cast<QFocusEvent *>(event);
        m_tracker->sendEvent(enumToString(e->type()), generateName(receiver), std::nullopt,
                             enumToString(e->reason()));
        break;
    }

    // input events: keys
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    {
        QKeyEvent *e = static_cast<QKeyEvent *>(event);
        m_receiver = generateName(receiver);
        QKeySequence sequence(e->keyCombination().toCombined());
        if (e->text().isEmpty() || (e->modifiers() != Qt::NoModifier && e->modifiers() != Qt::ShiftModifier))
            m_data = sequence.toString();
        else
            m_data = QLatin1String("[text]");
        m_coordinates = std::nullopt;
        m_timestamp = e->timestamp();
        m_type = e->type();
        m_pendingInput = true;
        break;
    }

    case QEvent::Shortcut:
    {
        QShortcutEvent *e = static_cast<QShortcutEvent *>(event);
        m_tracker->sendEvent(enumToString(e->type()), generateName(receiver), std::nullopt,
                             e->key().toString());
        break;
    }

    // input events: intents
    case QEvent::ContextMenu:
    {
        QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
        m_tracker->sendEvent(enumToString(e->type()), generateName(receiver),
                             InsightTracker::EventCoordinates(e->globalX(), e->globalY()));
        break;
    }

    // input events: drag'n'drop
    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::Drop:
    {
        QDropEvent *e = static_cast<QDropEvent *>(event);
        m_tracker->sendEvent(
                enumToString(e->type()), generateName(receiver),
                InsightTracker::EventCoordinates(e->position().x(), e->position().y()));
        break;
    }

    // input events: gestures
    case QEvent::NativeGesture:
    {
        QNativeGestureEvent *e = static_cast<QNativeGestureEvent *>(event);
        m_tracker->sendEvent(
                enumToString(e->type()), generateName(receiver),
                InsightTracker::EventCoordinates(e->position().x(), e->position().y())),
                enumToString(e->gestureType());
        break;
    }

    // needs Widgets support, and we probably don't want to require that in the
    // tracker library
//    case QEvent::Gesture:
//    {
//        QGestureEvent *e = static_cast<QGestureEvent *>(event);
//        if (e->activeGestures().constFirst()->state() == Qt::GestureStarted) {
//            m_tracker->sendEvent(enumToString(e->type()), generateName(receiver), 0, 0,
//                                 enumToString(e->activeGestures().constFirst()->gestureType()));
//        }
//        break;
//    }

    // window and widget events
    case QEvent::Show:
    case QEvent::Hide:
        m_tracker->sendEvent(enumToString(event->type()), generateName(receiver));
        break;

    case QEvent::Resize:
    {
        QResizeEvent *e = static_cast<QResizeEvent *>(event);
        m_coordinates = InsightTracker::EventCoordinates(e->size().width(), e->size().height());
        m_pendingResize = true;
        break;
    }

    case QEvent::Quit:
        m_tracker->sendEvent(enumToString(event->type()), generateName(receiver));
        break;

    default:
        m_tracker->sendEvent(enumToString(event->type()), generateName(receiver));
        break;
    }

    return false;
}
