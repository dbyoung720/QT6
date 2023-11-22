// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtInsightTracker/QInsightConfiguration>
#include <QtInsightTracker/QInsightTracker>
#include <QtInsightTracker/private/qinsightreporter_p.h>
#include <QtInsightTracker/private/qinsighteventfilter_p.h>
#include <QtInsightTracker/private/qinsighttracker_p.h>
#include <QtCore/qthread.h>

Q_LOGGING_CATEGORY(lcInsight, "qt.insight", QtWarningMsg)

InsightTrackerImpl::InsightTrackerImpl()
{
}

InsightTrackerImpl::~InsightTrackerImpl()
{
}

void InsightTrackerImpl::init()
{
    if (!m_initialized) {
        qCInfo(lcInsight) << "Enabling Qt Insight Tracker";

        if (!m_config.isValid()) {
            qCWarning(lcInsight)
                    << "Failed to enabled Qt Insight Tracker, the configuration is not valid";
            return;
        }

        m_workerThread = new QThread();
        m_reporter = new QInsightReporter();
        m_eventFilter.reset(new QInsightEventFilter(this));

        QObject::connect(m_workerThread, &QThread::started, m_reporter, [this] {
            m_reporter->init(&m_config);
        });
        QObject::connect(m_workerThread, &QThread::finished,
                         m_reporter, [this, thread = QThread::currentThread()] {
            m_reporter->moveToThread(thread);
        });

        m_reporter->moveToThread(m_workerThread);
        m_workerThread->start();

        m_initialized = true;
    }
}

void InsightTrackerImpl::deinit()
{
    if (m_initialized) {
        qCInfo(lcInsight) << "Disabling Qt Insight Tracker";

        m_eventFilter.reset();

        m_workerThread->quit();
        m_workerThread->wait();

        delete m_workerThread;
        m_workerThread = nullptr;

        delete m_reporter;
        m_reporter = nullptr;

        m_initialized = false;
    }
}

void InsightTrackerImpl::sendScreenView(const QString &name,
                                        const std::optional<InsightTracker::ContextData> &context)
{
    if (m_initialized) {
        QMetaObject::invokeMethod(m_reporter, [=] {
            m_reporter->trackScreenView(name, context);
        });
    }
}

void InsightTrackerImpl::sendClickEvent(const QString &name, const QString &category, int x, int y,
                                        const std::optional<InsightTracker::ContextData> &context)
{
    if (m_initialized) {
        if (category.isEmpty() || InsightTrackerImpl::instance().configuration()->categories().contains(category))
            QMetaObject::invokeMethod(m_reporter, [=] {
                m_reporter->trackClickEvent(name, category, x, y, context);
            });
        else
            qCDebug(lcInsight) << "category filtered:" << category;
    }
}

void InsightTrackerImpl::sendEvent(
        const QString &event, const QString &target,
        const std::optional<InsightTracker::EventCoordinates> &coordinates,
        const std::optional<QString> &data)
{
    if (m_initialized) {
        QMetaObject::invokeMethod(
                m_reporter, [=] { m_reporter->trackEvent(event, target, coordinates, data); });
    }
}

void InsightTrackerImpl::clearCache()
{
    if (m_initialized) {
        QMetaObject::invokeMethod(m_reporter, [=] {
            m_reporter->clearCache();
        });
    }
}

void InsightTrackerImpl::startNewSession()
{
    if (m_initialized) {
        QMetaObject::invokeMethod(m_reporter, [=] {
            m_reporter->startNewSession();
        });
    }
}

bool InsightTrackerImpl::isEnabled() const
{
    return m_initialized;
}

void InsightTrackerImpl::setEnabled(bool enabled)
{
    if (enabled)
        init();
    else
        deinit();
}
QInsightConfiguration *InsightTrackerImpl::configuration()
{
    return &m_config;
}


void InsightTrackerImpl::setCategoryFilter(QInsightCategoryFilter *filter)
{
    m_eventFilter->setCategoryFilter(filter);
}


/*!
    \class QInsightTracker
    \inmodule QtInsightTracker
    \brief Control the Qt Insight Tracker and send events to the back-end server.

    QInsightTracker provides a C++ API that can be used to control the tracker
    and to send events to the back-end server.

 */

/*!
    Constructs a tracker object.
    All tracker objects share the same back-end server and configuration.
 */
QInsightTracker::QInsightTracker()
{
}

/*!
    \fn QInsightTracker::~QInsightTracker()
    \brief Destroys the tracker object and frees all allocated resources.
 */

/*!
    Send a screen view event \a name.
 */
void QInsightTracker::sendScreenView(const QString &name) const
{
    InsightTrackerImpl::instance().sendScreenView(name);
}
/*!
    \overload
    Send a screen view event \a name with additional context data.
    Additional context data can be sent as a key/value pair in
    \a contextKey and \a contextValue.
 */
void QInsightTracker::sendScreenView(const QString &name, const QString &contextKey,
                                     double contextValue) const
{
    InsightTrackerImpl::instance().sendScreenView(
            name, InsightTracker::ContextData{ contextKey, contextValue });
}

/*!
    Send a click event \a name.
    Event can have extra details with \a category and click coordinates \a x and \a y.
 */
void QInsightTracker::sendClickEvent(const QString &name, const QString &category, int x, int y) const
{
    InsightTrackerImpl::instance().sendClickEvent(name, category, x, y);
}

/*!
    \overload
    Send a click event \a name with additional context data.
    Event can have extra details with \a category and click coordinates \a x and \a y.
    Additional context data can be sent as a key/value pair in
    \a contextKey and \a contextValue.
 */
void QInsightTracker::sendClickEvent(const QString &name, const QString &category, int x, int y,
                                     const QString &contextKey, double contextValue) const
{
    InsightTrackerImpl::instance().sendClickEvent(
            name, category, x, y, InsightTracker::ContextData{ contextKey, contextValue });
}

/*!
    Clear all events from the local cache.
 */
void QInsightTracker::clearCache()
{
    InsightTrackerImpl::instance().clearCache();
}

/*!
    \brief Start a new session
    A new session id is generated and then used in all subsequent events.
 */
void QInsightTracker::startNewSession()
{
    InsightTrackerImpl::instance().startNewSession();
}

/*!
    Is tracking enabled.
    \return true if tracking is enabled
    \sa QInsightTracker::setEnabled
 */
bool QInsightTracker::isEnabled() const
{
    return InsightTrackerImpl::instance().isEnabled();
}

/*!
    Enable or disable tracking.
    \sa QInsightTracker::isEnabled
 */
void QInsightTracker::setEnabled(bool enabled)
{
    InsightTrackerImpl::instance().setEnabled(enabled);
}

/*!
    \brief Configure the tracker
    \sa QInsightConfiguration
 */
QInsightConfiguration *QInsightTracker::configuration() const
{
    return InsightTrackerImpl::instance().configuration();
}
