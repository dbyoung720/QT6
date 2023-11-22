// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtInsightTracker/QInsightConfiguration>
#include <QtInsightTracker/private/qinsighttracker_p.h>
#include <QtInsightTrackerQml/private/qinsightcategory_p.h>
#include <QtInsightTrackerQml/private/qinsighttrackerqml_p.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLoggingCategory>

Q_LOGGING_CATEGORY(lcInsightQml, "qt.insight.qml", QtWarningMsg)

/*!
    \qmltype InsightTracker
    \inqmlmodule QtInsightTracker
    \instantiates QInsightTracker
    \brief Provides an interface to the Qt Insight Tracker.
*/

/*!
    \qmltype InsightCategory
    \inqmlmodule QtInsightTracker
    \brief Attaching type for category property.
*/

/*!
    \qmlattachedproperty string InsightCategory::category
    This attached property holds a category name. The property can be attached to
    any QML component and used to filter events with QInsightConfiguration::setCategories().
*/


QInsightTrackerQml::QInsightTrackerQml()
{
    InsightTrackerImpl::instance();
}

QInsightTrackerQml::~QInsightTrackerQml()
{
    InsightTrackerImpl::instance().setCategoryFilter(nullptr);
}

/*!
    \qmlmethod InsightTracker::sendScreenView(string name)
    Send a screen view event \a name.
*/
void QInsightTrackerQml::sendScreenView(const QString &name) const
{
    InsightTrackerImpl::instance().sendScreenView(name);
}

/*!
    \qmlmethod InsightTracker::sendScreenView(string name, string contextKey, double contextValue)
    \overload
    Send a screen view event \a name with additional context data.
    Additional context data can be sent as a key/value pair in
    \a contextKey and \a contextValue.
 */
void QInsightTrackerQml::sendScreenView(const QString &name, const QString &contextKey,
                                        double contextValue) const
{
    InsightTrackerImpl::instance().sendScreenView(
            name, InsightTracker::ContextData{ contextKey, contextValue });
}

/*!
    \qmlmethod InsightTracker::sendClickEvent(string name, string category, int x, int y)
    Send a click event \a name.
    Event can have extra details with \a category and click coordinates \a x and \a y.
*/
void QInsightTrackerQml::sendClickEvent(const QString &name, const QString &category, int x, int y) const
{
    InsightTrackerImpl::instance().sendClickEvent(name, category, x, y);
}

/*!
    \qmlmethod InsightTracker::sendClickEvent(string name, string category, int x, int y, string contextKey, double contextValue)
    \overload
    Send a click event \a name with additional context data.
    Event can have extra details with \a category and click coordinates \a x and \a y.
    Additional context data can be sent as a key/value pair in
    \a contextKey and \a contextValue.
 */
void QInsightTrackerQml::sendClickEvent(const QString &name, const QString &category, int x, int y,
                                        const QString &contextKey, double contextValue) const
{
    InsightTrackerImpl::instance().sendClickEvent(
            name, category, x, y, InsightTracker::ContextData{ contextKey, contextValue });
}

/*!
    \qmlmethod InsightTracker::clearCache()
    Clear all events from the local cache.
*/
void QInsightTrackerQml::clearCache()
{
    InsightTrackerImpl::instance().clearCache();
}

/*!
    \qmlmethod InsightTracker::startNewSession()
    \brief Start a new session
    A new session id is generated and then used in all subsequent events.
*/
void QInsightTrackerQml::startNewSession()
{
    InsightTrackerImpl::instance().startNewSession();
}

/*!
    \qmlproperty bool InsightTracker::enabled
    This property can be used to enable and disable the tracker.
*/

bool QInsightTrackerQml::isEnabled() const
{
    return InsightTrackerImpl::instance().isEnabled();
}

void QInsightTrackerQml::setEnabled(bool enabled)
{
    if (enabled != InsightTrackerImpl::instance().isEnabled()) {
        InsightTrackerImpl::instance().setEnabled(enabled);
        if (enabled)
            InsightTrackerImpl::instance().setCategoryFilter(this);

        emit enabledChanged(enabled);
    }
}

QString QInsightTrackerQml::objectCategory(QObject *receiver) const
{
    QInsightCategoryAttached *attached =
            qobject_cast<QInsightCategoryAttached*>(qmlAttachedPropertiesObject<QInsightCategory>(receiver, false));
    if (attached)
        return attached->category();

    return QString();
}
