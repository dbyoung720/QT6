// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0


#include "qdesigner_formwindowcommand_p.h"
#include "qdesigner_objectinspector_p.h"
#include "layout_p.h"

#include <QtDesigner/abstractformeditor.h>
#include <QtDesigner/abstractformwindow.h>
#include <QtDesigner/abstractobjectinspector.h>
#include <QtDesigner/abstractactioneditor.h>
#include <QtDesigner/abstractmetadatabase.h>
#include <QtDesigner/propertysheet.h>
#include <QtDesigner/abstractpropertyeditor.h>
#include <QtDesigner/qextensionmanager.h>

#include <QtCore/qvariant.h>
#include <QtWidgets/qwidget.h>
#include <QtWidgets/qlabel.h>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

namespace qdesigner_internal {

// ---- QDesignerFormWindowCommand ----
QDesignerFormWindowCommand::QDesignerFormWindowCommand(const QString &description,
                                                       QDesignerFormWindowInterface *formWindow,
                                                       QUndoCommand *parent)
    : QUndoCommand(description, parent),
      m_formWindow(formWindow)
{
}

QDesignerFormWindowInterface *QDesignerFormWindowCommand::formWindow() const
{
    return m_formWindow;
}

QDesignerFormEditorInterface *QDesignerFormWindowCommand::core() const
{
    if (QDesignerFormWindowInterface *fw = formWindow())
        return fw->core();

    return nullptr;
}

void QDesignerFormWindowCommand::undo()
{
    cheapUpdate();
}

void QDesignerFormWindowCommand::redo()
{
    cheapUpdate();
}

void QDesignerFormWindowCommand::cheapUpdate()
{
    if (core()->objectInspector())
        core()->objectInspector()->setFormWindow(formWindow());

    if (core()->actionEditor())
        core()->actionEditor()->setFormWindow(formWindow());
}

QDesignerPropertySheetExtension* QDesignerFormWindowCommand::propertySheet(QObject *object) const
{
    return  qt_extension<QDesignerPropertySheetExtension*>(formWindow()->core()->extensionManager(), object);
}

void QDesignerFormWindowCommand::updateBuddies(QDesignerFormWindowInterface *form,
                                               const QString &old_name,
                                               const QString &new_name)
{
    QExtensionManager* extensionManager = form->core()->extensionManager();

    const auto label_list = form->findChildren<QLabel*>();
    if (label_list.isEmpty())
        return;

    const QString buddyProperty = u"buddy"_s;
    const QByteArray oldNameU8 = old_name.toUtf8();
    const QByteArray newNameU8 = new_name.toUtf8();

    for (QLabel *label : label_list) {
        if (QDesignerPropertySheetExtension* sheet =
                qt_extension<QDesignerPropertySheetExtension*>(extensionManager, label)) {
            const int idx = sheet->indexOf(buddyProperty);
            if (idx != -1) {
                const QByteArray oldBuddy = sheet->property(idx).toByteArray();
                if (oldBuddy == oldNameU8)
                    sheet->setProperty(idx, newNameU8);
            }
        }
    }
}

void QDesignerFormWindowCommand::selectUnmanagedObject(QObject *unmanagedObject)
{
    // Keep selection in sync
    if (QDesignerObjectInspector *oi = qobject_cast<QDesignerObjectInspector *>(core()->objectInspector())) {
        oi->clearSelection();
        oi->selectObject(unmanagedObject);
    }
    core()->propertyEditor()->setObject(unmanagedObject);
}

} // namespace qdesigner_internal

QT_END_NAMESPACE
