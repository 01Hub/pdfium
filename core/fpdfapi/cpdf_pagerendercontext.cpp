// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/include/cpdf_pagerendercontext.h"

#include "core/fpdfapi/fpdf_render/include/cpdf_progressiverenderer.h"
#include "core/fpdfapi/fpdf_render/include/cpdf_rendercontext.h"
#include "core/fpdfapi/fpdf_render/include/cpdf_renderoptions.h"
#include "core/fpdfdoc/cpdf_annotlist.h"
#include "core/fpdfdoc/include/fpdf_doc.h"
#include "core/fxge/include/fx_ge.h"

CPDF_PageRenderContext::CPDF_PageRenderContext() {}

CPDF_PageRenderContext::~CPDF_PageRenderContext() {
  if (m_pOptions)
    delete m_pOptions->m_pOCContext;
}
