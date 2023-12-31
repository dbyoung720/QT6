// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "layoutinfo_p.h"

#include <QtDesigner/abstractformeditor.h>
#include <QtDesigner/container.h>
#include <QtDesigner/abstractmetadatabase.h>
#include <QtDesigner/qextensionmanager.h>

#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/qsplitter.h>
#include <QtCore/qdebug.h>
#include <QtCore/qhash.h>
#include <QtCore/qrect.h>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

namespace qdesigner_internal {
/*!
  \overload
*/
LayoutInfo::Type LayoutInfo::layoutType(const QDesignerFormEditorInterface *core, const QLayout *layout)
{
    Q_UNUSED(core);
    if (!layout)
        return NoLayout;
    if (qobject_cast<const QHBoxLayout*>(layout))
        return HBox;
    if (qobject_cast<const QVBoxLayout*>(layout))
        return VBox;
    if (qobject_cast<const QGridLayout*>(layout))
        return Grid;
    if (qobject_cast<const QFormLayout*>(layout))
       return Form;
    return UnknownLayout;
}

static const QHash<QString, LayoutInfo::Type> &layoutNameTypeMap()
{
    static const QHash<QString, LayoutInfo::Type> nameTypeMap = {
        {u"QVBoxLayout"_s, LayoutInfo::VBox},
        {u"QHBoxLayout"_s, LayoutInfo::HBox},
        {u"QGridLayout"_s, LayoutInfo::Grid},
        {u"QFormLayout"_s, LayoutInfo::Form}
    };
    return nameTypeMap;
}

LayoutInfo::Type LayoutInfo::layoutType(const QString &typeName)
{
    return layoutNameTypeMap().value(typeName, NoLayout);
}

QString LayoutInfo::layoutName(Type t)
{
    return layoutNameTypeMap().key(t);
}

/*!
  \overload
*/
LayoutInfo::Type LayoutInfo::layoutType(const QDesignerFormEditorInterface *core, const QWidget *w)
{
    if (const QSplitter *splitter = qobject_cast<const QSplitter *>(w))
        return  splitter->orientation() == Qt::Horizontal ? HSplitter : VSplitter;
    return layoutType(core, w->layout());
}

LayoutInfo::Type LayoutInfo::managedLayoutType(const QDesignerFormEditorInterface *core,
                                               const QWidget *w,
                                               QLayout **ptrToLayout)
{
    if (ptrToLayout)
        *ptrToLayout = nullptr;
    if (const QSplitter *splitter = qobject_cast<const QSplitter *>(w))
        return  splitter->orientation() == Qt::Horizontal ? HSplitter : VSplitter;
    QLayout *layout = managedLayout(core, w);
    if (!layout)
        return NoLayout;
    if (ptrToLayout)
        *ptrToLayout = layout;
    return layoutType(core, layout);
}

QWidget *LayoutInfo::layoutParent(const QDesignerFormEditorInterface *core, QLayout *layout)
{
    Q_UNUSED(core);

    QObject *o = layout;
    while (o) {
        if (QWidget *widget = qobject_cast<QWidget*>(o))
            return widget;

        o = o->parent();
    }
    return nullptr;
}

void LayoutInfo::deleteLayout(const QDesignerFormEditorInterface *core, QWidget *widget)
{
    if (QDesignerContainerExtension *container = qt_extension<QDesignerContainerExtension*>(core->extensionManager(), widget))
        widget = container->widget(container->currentIndex());

    Q_ASSERT(widget != nullptr);

    QLayout *layout = managedLayout(core, widget);

    if (layout == nullptr || core->metaDataBase()->item(layout) != nullptr) {
        delete layout;
        widget->updateGeometry();
        return;
    }

    qDebug() << "trying to delete an unmanaged layout:" << "widget:" << widget << "layout:" << layout;
}

LayoutInfo::Type LayoutInfo::laidoutWidgetType(const QDesignerFormEditorInterface *core,
                                               QWidget *widget,
                                               bool *isManaged,
                                               QLayout **ptrToLayout)
{
    if (isManaged)
        *isManaged = false;
    if (ptrToLayout)
        *ptrToLayout = nullptr;

    QWidget *parent = widget->parentWidget();
    if (!parent)
        return NoLayout;

    // 1) Splitter
    if (QSplitter *splitter  = qobject_cast<QSplitter*>(parent)) {
        if (isManaged)
            *isManaged = core->metaDataBase()->item(splitter);
        return  splitter->orientation() == Qt::Horizontal ? HSplitter : VSplitter;
    }

    // 2) Layout of parent
    QLayout *parentLayout = parent->layout();
    if (!parentLayout)
        return NoLayout;

    if (parentLayout->indexOf(widget) != -1) {
        if (isManaged)
            *isManaged = core->metaDataBase()->item(parentLayout);
        if (ptrToLayout)
            *ptrToLayout = parentLayout;
        return layoutType(core, parentLayout);
    }

    // 3) Some child layout (see below comment about Q3GroupBox)
    const auto childLayouts = parentLayout->findChildren<QLayout*>();
    if (childLayouts.isEmpty())
        return NoLayout;
    for (QLayout *layout : childLayouts) {
        if (layout->indexOf(widget) != -1) {
            if (isManaged)
                *isManaged = core->metaDataBase()->item(layout);
            if (ptrToLayout)
                *ptrToLayout = layout;
            return layoutType(core, layout);
        }
    }

    return NoLayout;
}

QLayout *LayoutInfo::internalLayout(const QWidget *widget)
{
    return widget->layout();
}


QLayout *LayoutInfo::managedLayout(const QDesignerFormEditorInterface *core, const QWidget *widget)
{
    if (widget == nullptr)
        return nullptr;

    QLayout *layout = widget->layout();
    if (!layout)
        return nullptr;

    return managedLayout(core, layout);
}

QLayout *LayoutInfo::managedLayout(const QDesignerFormEditorInterface *core, QLayout *layout)
{
    if (!layout)
        return nullptr;

    QDesignerMetaDataBaseInterface *metaDataBase = core->metaDataBase();

    if (!metaDataBase)
        return layout;
    /* This code exists mainly for the Q3GroupBox class, for which
     * widget->layout() returns an internal VBoxLayout. */
    const QDesignerMetaDataBaseItemInterface *item = metaDataBase->item(layout);
    if (item == nullptr) {
        layout = layout->findChild<QLayout*>();
        item = metaDataBase->item(layout);
    }
    if (!item)
        return nullptr;
    return layout;
}

// Is it a a dummy grid placeholder created by Designer?
bool LayoutInfo::isEmptyItem(QLayoutItem *item)
{
    if (item == nullptr) {
        qDebug() << "** WARNING Zero-item passed on to isEmptyItem(). This indicates a layout inconsistency.";
        return true;
    }
    return item->spacerItem() != nullptr;
}

QDESIGNER_SHARED_EXPORT void getFormLayoutItemPosition(const QFormLayout *formLayout, int index, int *rowPtr, int *columnPtr, int *rowspanPtr, int *colspanPtr)
{
    int row;
    QFormLayout::ItemRole role;
    formLayout->getItemPosition(index, &row, &role);
    const int columnspan = role == QFormLayout::SpanningRole ? 2 : 1;
    const int column = (columnspan > 1 || role == QFormLayout::LabelRole) ? 0 : 1;
    if (rowPtr)
        *rowPtr = row;
    if (columnPtr)
        *columnPtr = column;
    if (rowspanPtr)
        *rowspanPtr = 1;
    if (colspanPtr)
        *colspanPtr = columnspan;
}

static inline QFormLayout::ItemRole formLayoutRole(int column, int colspan)
{
    if (colspan > 1)
        return QFormLayout::SpanningRole;
    return column == 0 ? QFormLayout::LabelRole : QFormLayout::FieldRole;
}

QDESIGNER_SHARED_EXPORT void formLayoutAddWidget(QFormLayout *formLayout, QWidget *w, const QRect &r, bool insert)
{
    // Consistent API galore...
    if (insert) {
        const bool spanning = r.width() > 1;
        if (spanning) {
            formLayout->insertRow(r.y(), w);
        } else {
            QWidget *label = nullptr;
            QWidget *field = nullptr;
            if (r.x() == 0) {
                label = w;
            } else {
                field = w;
            }
            formLayout->insertRow(r.y(), label, field);
        }
    } else {
        formLayout->setWidget(r.y(), formLayoutRole(r.x(), r.width()), w);
    }
}

} // namespace qdesigner_internal

QT_END_NAMESPACE
