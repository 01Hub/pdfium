// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_defaultappearance.h"

#include <algorithm>

#include "core/fpdfapi/parser/cpdf_simple_parser.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fxge/cfx_color.h"

bool CPDF_DefaultAppearance::HasFont() {
  if (m_csDA.IsEmpty())
    return false;

  CPDF_SimpleParser syntax(m_csDA.AsStringView());
  return syntax.FindTagParamFromStart("Tf", 2);
}

ByteString CPDF_DefaultAppearance::GetFont(float* fFontSize) {
  *fFontSize = 0.0f;
  if (m_csDA.IsEmpty())
    return ByteString();

  ByteString csFontNameTag;
  CPDF_SimpleParser syntax(m_csDA.AsStringView());
  if (syntax.FindTagParamFromStart("Tf", 2)) {
    csFontNameTag = ByteString(syntax.GetWord());
    csFontNameTag.Delete(0, 1);
    *fFontSize = FX_atof(syntax.GetWord());
  }
  return PDF_NameDecode(csFontNameTag);
}

bool CPDF_DefaultAppearance::HasColor() {
  if (m_csDA.IsEmpty())
    return false;

  CPDF_SimpleParser syntax(m_csDA.AsStringView());
  if (syntax.FindTagParamFromStart("g", 1))
    return true;
  if (syntax.FindTagParamFromStart("rg", 3))
    return true;
  return syntax.FindTagParamFromStart("k", 4);
}

void CPDF_DefaultAppearance::GetColor(int& iColorType, float fc[4]) {
  iColorType = CFX_Color::kTransparent;
  for (int c = 0; c < 4; c++)
    fc[c] = 0;

  if (m_csDA.IsEmpty())
    return;

  CPDF_SimpleParser syntax(m_csDA.AsStringView());
  if (syntax.FindTagParamFromStart("g", 1)) {
    iColorType = CFX_Color::kGray;
    fc[0] = FX_atof(syntax.GetWord());
    return;
  }
  if (syntax.FindTagParamFromStart("rg", 3)) {
    iColorType = CFX_Color::kRGB;
    fc[0] = FX_atof(syntax.GetWord());
    fc[1] = FX_atof(syntax.GetWord());
    fc[2] = FX_atof(syntax.GetWord());
    return;
  }
  if (syntax.FindTagParamFromStart("k", 4)) {
    iColorType = CFX_Color::kCMYK;
    fc[0] = FX_atof(syntax.GetWord());
    fc[1] = FX_atof(syntax.GetWord());
    fc[2] = FX_atof(syntax.GetWord());
    fc[3] = FX_atof(syntax.GetWord());
  }
}

void CPDF_DefaultAppearance::GetColor(FX_ARGB& color, int& iColorType) {
  color = 0;
  float values[4];
  GetColor(iColorType, values);
  if (iColorType == CFX_Color::kTransparent)
    return;

  if (iColorType == CFX_Color::kGray) {
    int g = static_cast<int>(values[0] * 255 + 0.5f);
    color = ArgbEncode(255, g, g, g);
    return;
  }
  if (iColorType == CFX_Color::kRGB) {
    int r = static_cast<int>(values[0] * 255 + 0.5f);
    int g = static_cast<int>(values[1] * 255 + 0.5f);
    int b = static_cast<int>(values[2] * 255 + 0.5f);
    color = ArgbEncode(255, r, g, b);
    return;
  }
  if (iColorType == CFX_Color::kCMYK) {
    float r = 1.0f - std::min(1.0f, values[0] + values[3]);
    float g = 1.0f - std::min(1.0f, values[1] + values[3]);
    float b = 1.0f - std::min(1.0f, values[2] + values[3]);
    color = ArgbEncode(255, static_cast<int>(r * 255 + 0.5f),
                       static_cast<int>(g * 255 + 0.5f),
                       static_cast<int>(b * 255 + 0.5f));
  }
}
