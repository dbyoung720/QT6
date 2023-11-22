# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

set(CMAKE_AUTOMOC ON)

qt_internal_add_module(InsightTracker
    QMAKE_MODULE_CONFIG moc
    EXCEPTIONS
    SOURCES
        qinsightconfiguration.cpp qinsightconfiguration.h
        qinsighttracker.cpp qinsighttracker.h qinsighttracker_p.h qinsighttracker_p_p.h
        qtinsighttracker_global.h
        qtinsighttrackerexports.h
        qinsightreporter.cpp qinsightreporter_p.h
        qinsighteventfilter.cpp qinsighteventfilter_p.h qinsightcategoryfilter_p.h
        qsqlitestorage.cpp qsqlitestorage_p.h
        qinsightstorage_p.h
        qtinsight.conf
    DEFINES
        QT_BUILD_INSIGHTTRACKER_LIB
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_FROM_BYTEARRAY
        QT_NO_CAST_TO_ASCII
        QT_NO_URL_CAST_FROM_STRING
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::CorePrivate
        Qt::Gui
        Qt::Network
)

qt_internal_extend_target(InsightTracker
    CONDITION QT_FEATURE_sql
    PUBLIC_LIBRARIES
        Qt::Sql
)

qt_path_join(conf_install_dir ${QT_INSTALL_DIR} ${INSTALL_DATADIR} qtinsight )
qt_copy_or_install(FILES qtinsight.conf DESTINATION ${conf_install_dir})

qt_internal_add_docs(InsightTracker
    doc/qtinsighttracker.qdocconf
)
