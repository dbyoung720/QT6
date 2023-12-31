// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/native_pixmap_egl_binding.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/native_pixmap_egl_binding_helper.h"

namespace ui {

namespace {

// Map buffer format to GL type. Return GL_NONE if no sensible mapping.
unsigned BufferFormatToGLDataType(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRA_8888:
      return GL_UNSIGNED_BYTE;
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_1616:
      return GL_UNSIGNED_SHORT;
    case gfx::BufferFormat::BGR_565:
      return GL_UNSIGNED_SHORT_5_6_5;
    case gfx::BufferFormat::RGBA_4444:
      return GL_UNSIGNED_SHORT_4_4_4_4;
    case gfx::BufferFormat::RGBA_1010102:
    case gfx::BufferFormat::BGRA_1010102:
      return GL_UNSIGNED_INT_2_10_10_10_REV;
    case gfx::BufferFormat::RGBA_F16:
      return GL_HALF_FLOAT_OES;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
    case gfx::BufferFormat::P010:
      return GL_NONE;
  }

  NOTREACHED();
  return GL_NONE;
}

}  // namespace

NativePixmapEGLBinding::NativePixmapEGLBinding(
    std::unique_ptr<gl::NativePixmapEGLBindingHelper> binding_helper,
    gfx::BufferFormat format)
    : binding_helper_(std::move(binding_helper)), format_(format) {}
NativePixmapEGLBinding::~NativePixmapEGLBinding() = default;

// static
std::unique_ptr<NativePixmapGLBinding> NativePixmapEGLBinding::Create(
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat plane_format,
    gfx::BufferPlane plane,
    gfx::Size plane_size,
    const gfx::ColorSpace& color_space,
    GLenum target,
    GLuint texture_id) {
  auto binding_helper = gl::NativePixmapEGLBindingHelper::CreateForPlane(
      plane_size, plane_format, plane, std::move(pixmap), color_space, target,
      texture_id);
  if (!binding_helper) {
    LOG(ERROR) << "Unable to initialize binding from pixmap";
    return nullptr;
  }

  auto binding = std::make_unique<NativePixmapEGLBinding>(
      std::move(binding_helper), plane_format);

  return binding;
}

GLuint NativePixmapEGLBinding::GetInternalFormat() {
  return binding_helper_->GetInternalFormat();
}

GLenum NativePixmapEGLBinding::GetDataType() {
  return BufferFormatToGLDataType(format_);
}

}  // namespace ui
