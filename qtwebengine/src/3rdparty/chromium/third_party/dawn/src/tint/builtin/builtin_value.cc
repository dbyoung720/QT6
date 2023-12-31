// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

////////////////////////////////////////////////////////////////////////////////
// File generated by tools/src/cmd/gen
// using the template:
//   src/tint/builtin/builtin_value.cc.tmpl
//
// Do not modify this file directly
////////////////////////////////////////////////////////////////////////////////

#include "src/tint/builtin/builtin_value.h"

namespace tint::builtin {

/// ParseBuiltinValue parses a BuiltinValue from a string.
/// @param str the string to parse
/// @returns the parsed enum, or BuiltinValue::kUndefined if the string could not be parsed.
BuiltinValue ParseBuiltinValue(std::string_view str) {
    if (str == "__point_size") {
        return BuiltinValue::kPointSize;
    }
    if (str == "frag_depth") {
        return BuiltinValue::kFragDepth;
    }
    if (str == "front_facing") {
        return BuiltinValue::kFrontFacing;
    }
    if (str == "global_invocation_id") {
        return BuiltinValue::kGlobalInvocationId;
    }
    if (str == "instance_index") {
        return BuiltinValue::kInstanceIndex;
    }
    if (str == "local_invocation_id") {
        return BuiltinValue::kLocalInvocationId;
    }
    if (str == "local_invocation_index") {
        return BuiltinValue::kLocalInvocationIndex;
    }
    if (str == "num_workgroups") {
        return BuiltinValue::kNumWorkgroups;
    }
    if (str == "position") {
        return BuiltinValue::kPosition;
    }
    if (str == "sample_index") {
        return BuiltinValue::kSampleIndex;
    }
    if (str == "sample_mask") {
        return BuiltinValue::kSampleMask;
    }
    if (str == "vertex_index") {
        return BuiltinValue::kVertexIndex;
    }
    if (str == "workgroup_id") {
        return BuiltinValue::kWorkgroupId;
    }
    return BuiltinValue::kUndefined;
}

std::ostream& operator<<(std::ostream& out, BuiltinValue value) {
    switch (value) {
        case BuiltinValue::kUndefined:
            return out << "undefined";
        case BuiltinValue::kPointSize:
            return out << "__point_size";
        case BuiltinValue::kFragDepth:
            return out << "frag_depth";
        case BuiltinValue::kFrontFacing:
            return out << "front_facing";
        case BuiltinValue::kGlobalInvocationId:
            return out << "global_invocation_id";
        case BuiltinValue::kInstanceIndex:
            return out << "instance_index";
        case BuiltinValue::kLocalInvocationId:
            return out << "local_invocation_id";
        case BuiltinValue::kLocalInvocationIndex:
            return out << "local_invocation_index";
        case BuiltinValue::kNumWorkgroups:
            return out << "num_workgroups";
        case BuiltinValue::kPosition:
            return out << "position";
        case BuiltinValue::kSampleIndex:
            return out << "sample_index";
        case BuiltinValue::kSampleMask:
            return out << "sample_mask";
        case BuiltinValue::kVertexIndex:
            return out << "vertex_index";
        case BuiltinValue::kWorkgroupId:
            return out << "workgroup_id";
    }
    return out << "<unknown>";
}

}  // namespace tint::builtin
