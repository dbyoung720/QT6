#!/usr/bin/env bash

source "${BASH_SOURCE%/*}/../common/unix/install-ffmpeg.sh" "linux"
source "${BASH_SOURCE%/*}/../common/unix/install-ffmpeg-android.sh" "android-x86"
source "${BASH_SOURCE%/*}/../common/unix/install-ffmpeg-android.sh" "android-x86_64"
