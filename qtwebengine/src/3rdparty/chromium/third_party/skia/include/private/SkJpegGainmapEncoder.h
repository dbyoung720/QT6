/*
 * Copyright 2023 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkJpegGainmapEncoder_DEFINED
#define SkJpegGainmapEncoder_DEFINED

#include "include/encode/SkJpegEncoder.h"

class SkPixmap;
class SkWStream;
struct SkGainmapInfo;

class SK_API SkJpegGainmapEncoder {
public:
    /**
     *  Encode a JpegR image to |dst|.
     *
     *  The base image is specified by |base|, and |baseOptions| controls the encoding behavior for
     *  the base image.
     *
     *  The gainmap image is specified by |gainmap|, and |gainmapOptions| controls the encoding
     *  behavior for the gainmap image.
     *
     *  The rendering behavior of the gainmap image is provided in |gainmapInfo|. Not all gainmap
     *  based images are compatible with JpegR. If the image is not compatible with JpegR, then
     *  convert the gainmap to a format that is capable with JpegR. This conversion may result in
     *  less precise quantization of the gainmap image.
     *
     *  Returns true on success.  Returns false on an invalid or unsupported |src|.
     */
    static bool EncodeJpegR(SkWStream* dst,
                            const SkPixmap& base,
                            const SkJpegEncoder::Options& baseOptions,
                            const SkPixmap& gainmap,
                            const SkJpegEncoder::Options& gainmapOptions,
                            const SkGainmapInfo& gainmapInfo);

    /**
     *  Encode an HDRGM image to |dst|.
     *
     *  The base image is specified by |base|, and |baseOptions| controls the encoding behavior for
     *  the base image.
     *
     *  The gainmap image is specified by |gainmap|, and |gainmapOptions| controls the encoding
     *  behavior for the gainmap image.
     *
     *  The rendering behavior of the gainmap image is provided in |gainmapInfo|.
     *
     *  Returns true on success.  Returns false on an invalid or unsupported |src|.
     */
    static bool EncodeHDRGM(SkWStream* dst,
                            const SkPixmap& base,
                            const SkJpegEncoder::Options& baseOptions,
                            const SkPixmap& gainmap,
                            const SkJpegEncoder::Options& gainmapOptions,
                            const SkGainmapInfo& gainmapInfo);

private:
    // Helper function to encode an image with optional XMP and MPF to an SkData.
    static sk_sp<SkData> EncodeToData(const SkPixmap& pm,
                                      const SkJpegEncoder::Options& options,
                                      SkData* xmpSegmentParameters,
                                      SkData* mpfSegmentParameters);
};

#endif
