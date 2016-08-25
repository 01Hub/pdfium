// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/include/cpdfsdk_annot.h"

#include <algorithm>

#include "fpdfsdk/include/fsdk_mgr.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/include/fpdfxfa_doc.h"
#endif  // PDF_ENABLE_XFA

namespace {

const float kMinWidth = 1.0f;
const float kMinHeight = 1.0f;

}  // namespace

CPDFSDK_Annot::CPDFSDK_Annot(CPDFSDK_PageView* pPageView)
    : m_pPageView(pPageView), m_bSelected(FALSE) {}

CPDFSDK_Annot::~CPDFSDK_Annot() {}

#ifdef PDF_ENABLE_XFA

FX_BOOL CPDFSDK_Annot::IsXFAField() {
  return FALSE;
}

CXFA_FFWidget* CPDFSDK_Annot::GetXFAWidget() const {
  return nullptr;
}

CPDFXFA_Page* CPDFSDK_Annot::GetPDFXFAPage() {
  return m_pPageView ? m_pPageView->GetPDFXFAPage() : nullptr;
}

#endif  // PDF_ENABLE_XFA

FX_FLOAT CPDFSDK_Annot::GetMinWidth() const {
  return kMinWidth;
}

FX_FLOAT CPDFSDK_Annot::GetMinHeight() const {
  return kMinHeight;
}

int CPDFSDK_Annot::GetLayoutOrder() const {
  return 5;
}

CPDF_Annot* CPDFSDK_Annot::GetPDFAnnot() const {
  return nullptr;
}

CFX_ByteString CPDFSDK_Annot::GetType() const {
  return "";
}

CFX_ByteString CPDFSDK_Annot::GetSubType() const {
  return "";
}

void CPDFSDK_Annot::SetRect(const CFX_FloatRect& rect) {}

CFX_FloatRect CPDFSDK_Annot::GetRect() const {
  return CFX_FloatRect();
}

void CPDFSDK_Annot::Annot_OnDraw(CFX_RenderDevice* pDevice,
                                 CFX_Matrix* pUser2Device,
                                 CPDF_RenderOptions* pOptions) {}

FX_BOOL CPDFSDK_Annot::IsSelected() {
  return m_bSelected;
}

void CPDFSDK_Annot::SetSelected(FX_BOOL bSelected) {
  m_bSelected = bSelected;
}

UnderlyingPageType* CPDFSDK_Annot::GetUnderlyingPage() {
#ifdef PDF_ENABLE_XFA
  return GetPDFXFAPage();
#else   // PDF_ENABLE_XFA
  return GetPDFPage();
#endif  // PDF_ENABLE_XFA
}

CPDF_Page* CPDFSDK_Annot::GetPDFPage() {
  return m_pPageView ? m_pPageView->GetPDFPage() : nullptr;
}
