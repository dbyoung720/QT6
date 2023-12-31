# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys
import json

_CWD = os.getcwd()

# TODO(crbug.com/1320176): Consider either integrating this functionality into
# ts_library.py or replacing the regex if only "tslib" is ever rewritten.
def main(argv):
    parser = argparse.ArgumentParser()
    # List of imports and what they should be rewritten to. Specified as an
    # array of "src|dest" strings. Note that the src will be treated as regex
    # matched against the entire import path. I.e. "foo|bar" will translate to
    # re.replace("^foo$", "bar", line).
    parser.add_argument('--import_mappings', nargs='*')
    # The directory to output the rewritten files to.
    parser.add_argument('--out_dir', required=True)
    # The directory to output the manifest file to. The manifest can be used
    # by downstream tools (such as generate_grd) to capture all files that were
    # rewritten.
    parser.add_argument('--manifest_out', required=True)
    # The directory all in_files are under, used to construct a real path on
    # disk via base_dir + in_file.
    parser.add_argument('--base_dir', required=True)
    # List of files to rewrite imports for, all files should be provided
    # relative to base_dir. Each files path is preserved when outputed
    # into out_dir. I.e. "a/b/c/foo" will be outputted to "base_dir/a/b/c/foo".
    parser.add_argument('--in_files', nargs='*')
    args = parser.parse_args(argv)

    manifest = {'base_dir': args.out_dir, 'files': []}
    import_mappings = dict()
    for mapping in args.import_mappings:
        (src, dst) = mapping.split('|')
        import_mappings[src] = dst

    def _map_import(import_path):
        for regex in import_mappings.keys():
            if re.match(f"^{regex}$", import_path):
                return import_mappings[regex]
        generated_import_match = re.match(r'^generated:(.*)', import_path)
        if generated_import_match:
            return generated_import_match.group(1)
        return None

    for f in args.in_files:
        output = []
        for line in open(os.path.join(args.base_dir, f), 'r').readlines():
            # Investigate JS parsing if this is insufficient.
            match = re.match(r'^(import .*["\'])(.*)(["\'];)$', line)
            if match and _map_import(match.group(2)):
                new_import = _map_import(match.group(2))
                line = f"{match.group(1)}{new_import}{match.group(3)}\n"
            output.append(line)
        with open(os.path.join(args.out_dir, f), 'w') as out_file:
            out_file.write(''.join(output))
        manifest['files'].append(f)

    with open(args.manifest_out, 'w') as manifest_file:
        json.dump(manifest, manifest_file)


if __name__ == '__main__':
    main(sys.argv[1:])
