# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

if (NOT ANDROID
        AND NOT IOS
        AND NOT WASM
        AND NOT rtems)
    add_subdirectory(insightuploader)
endif()
