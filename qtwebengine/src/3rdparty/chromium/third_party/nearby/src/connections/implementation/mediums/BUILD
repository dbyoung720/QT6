# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
licenses(["notice"])

cc_library(
    name = "mediums",
    srcs = [
        "ble.cc",
        "ble_v2.cc",
        "bluetooth_classic.cc",
        "bluetooth_radio.cc",
        "mediums.cc",
        "wifi_direct.cc",
        "wifi_hotspot.cc",
        "wifi_lan.cc",
    ] + select({
        "@platforms//cpu:arm": [
            "webrtc_stub.cc",
        ],
        "//conditions:default": [
            "webrtc_stub.cc",
        ],
    }),
    hdrs = [
        "ble.h",
        "ble_v2.h",
        "bluetooth_classic.h",
        "bluetooth_radio.h",
        "mediums.h",
        "wifi.h",
        "wifi_direct.h",
        "wifi_hotspot.h",
        "wifi_lan.h",
    ] + select({
        "@platforms//cpu:arm": [
            "webrtc_stub.h",
        ],
        "//conditions:default": [
            "webrtc_stub.h",
        ],
    }),
    copts = select({
        "@platforms//cpu:arm": [
            "-DNO_WEBRTC",
        ],
        "//conditions:default": [],
    }),
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections/implementation:__subpackages__",
    ],
    deps = [
        ":utils",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "//connections:core_types",
        "//connections/implementation/mediums/ble_v2",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//proto/mediums:web_rtc_signaling_frames_cc_proto",
    ] + select({
        "@platforms//cpu:arm": [
            "//connections/implementation/proto:offline_wire_formats_cc_proto",
        ],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "utils",
    srcs = [
        "utils.cc",
    ] + select({
        "@platforms//cpu:arm": [
            "webrtc_peer_id_stub.cc",
        ],
        "//conditions:default": [
            "webrtc_peer_id_stub.cc",
        ],
    }),
    hdrs = [
        "lost_entity_tracker.h",
        "utils.h",
    ] + select({
        "@platforms//cpu:arm": [
            "webrtc_peer_id_stub.h",
            "webrtc_socket_stub.h",
        ],
        "//conditions:default": [
            "webrtc_peer_id_stub.h",
            "webrtc_socket_stub.h",
        ],
    }),
    copts = select({
        "@platforms//cpu:arm": [
            "-DNO_WEBRTC",
        ],
        "//conditions:default": [],
    }),
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections/implementation:__pkg__",
        "//connections/implementation/mediums/ble_v2:__subpackages__",
        "//connections/implementation/mediums/webrtc:__pkg__",
    ],
    deps = [
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/platform:base",
        "//internal/platform:types",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "core_internal_mediums_test",
    size = "small",
    srcs = [
        "ble_test.cc",
        "ble_v2_test.cc",
        "bluetooth_classic_test.cc",
        "bluetooth_radio_test.cc",
        "lost_entity_tracker_test.cc",
        "wifi_direct_test.cc",
        "wifi_hotspot_test.cc",
        "wifi_lan_test.cc",
        "wifi_test.cc",
    ],
    defines = ["NO_WEBRTC"],
    shard_count = 16,
    deps = [
        ":mediums",
        ":utils",
        "//connections/implementation/mediums/ble_v2",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
