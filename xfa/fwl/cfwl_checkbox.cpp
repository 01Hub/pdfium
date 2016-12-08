// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/cfwl_checkbox.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/base/ptr_util.h"
#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fwl/cfwl_app.h"
#include "xfa/fwl/cfwl_event.h"
#include "xfa/fwl/cfwl_messagekey.h"
#include "xfa/fwl/cfwl_messagemouse.h"
#include "xfa/fwl/cfwl_notedriver.h"
#include "xfa/fwl/cfwl_themebackground.h"
#include "xfa/fwl/cfwl_themetext.h"
#include "xfa/fwl/cfwl_widgetmgr.h"
#include "xfa/fwl/ifwl_themeprovider.h"

namespace {

const int kCaptionMargin = 5;

}  // namespace

CFWL_CheckBox::CFWL_CheckBox(const CFWL_App* app)
    : CFWL_Widget(app, pdfium::MakeUnique<CFWL_WidgetProperties>(), nullptr),
      m_dwTTOStyles(FDE_TTOSTYLE_SingleLine),
      m_iTTOAlign(FDE_TTOALIGNMENT_Center),
      m_bBtnDown(false),
      m_fBoxHeight(16.0f) {
  m_rtClient.Reset();
  m_rtBox.Reset();
  m_rtCaption.Reset();
  m_rtFocus.Reset();
}

CFWL_CheckBox::~CFWL_CheckBox() {}

FWL_Type CFWL_CheckBox::GetClassID() const {
  return FWL_Type::CheckBox;
}

void CFWL_CheckBox::SetBoxSize(FX_FLOAT fHeight) {
  m_fBoxHeight = fHeight;
}

void CFWL_CheckBox::Update() {
  if (IsLocked())
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  UpdateTextOutStyles();
  Layout();
}

void CFWL_CheckBox::DrawWidget(CFX_Graphics* pGraphics,
                               const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    return;

  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  if (HasBorder()) {
    DrawBorder(pGraphics, CFWL_Part::Border, m_pProperties->m_pThemeProvider,
               pMatrix);
  }
  if (HasEdge())
    DrawEdge(pGraphics, CFWL_Part::Edge, pTheme, pMatrix);

  int32_t dwStates = GetPartStates();

  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::Background;
  param.m_dwStates = dwStates;
  param.m_pGraphics = pGraphics;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  param.m_rtPart = m_rtClient;
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Focused)
    param.m_pData = &m_rtFocus;
  pTheme->DrawBackground(&param);

  param.m_iPart = CFWL_Part::CheckBox;
  param.m_rtPart = m_rtBox;
  pTheme->DrawBackground(&param);

  CFWL_ThemeText textParam;
  textParam.m_pWidget = this;
  textParam.m_iPart = CFWL_Part::Caption;
  textParam.m_dwStates = dwStates;
  textParam.m_pGraphics = pGraphics;
  if (pMatrix)
    textParam.m_matrix.Concat(*pMatrix);
  textParam.m_rtPart = m_rtCaption;
  textParam.m_wsText = L"Check box";
  textParam.m_dwTTOStyles = m_dwTTOStyles;
  textParam.m_iTTOAlign = m_iTTOAlign;
  pTheme->DrawText(&textParam);
}

void CFWL_CheckBox::SetCheckState(int32_t iCheck) {
  m_pProperties->m_dwStates &= ~FWL_STATE_CKB_CheckMask;
  switch (iCheck) {
    case 1:
      m_pProperties->m_dwStates |= FWL_STATE_CKB_Checked;
      break;
    case 2:
      if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_3State)
        m_pProperties->m_dwStates |= FWL_STATE_CKB_Neutral;
      break;
    default:
      break;
  }
  RepaintRect(m_rtClient);
}

void CFWL_CheckBox::Layout() {
  m_pProperties->m_rtWidget.width =
      FXSYS_round(m_pProperties->m_rtWidget.width);
  m_pProperties->m_rtWidget.height =
      FXSYS_round(m_pProperties->m_rtWidget.height);
  m_rtClient = GetClientRect();

  FX_FLOAT fBoxTop = m_rtClient.top;
  FX_FLOAT fClientBottom = m_rtClient.bottom();

  switch (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_VLayoutMask) {
    case FWL_STYLEEXT_CKB_Top:
      break;
    case FWL_STYLEEXT_CKB_Bottom: {
      fBoxTop = fClientBottom - m_fBoxHeight;
      break;
    }
    case FWL_STYLEEXT_CKB_VCenter:
    default: {
      fBoxTop = m_rtClient.top + (m_rtClient.height - m_fBoxHeight) / 2;
      fBoxTop = FXSYS_floor(fBoxTop);
      break;
    }
  }

  FX_FLOAT fBoxLeft = m_rtClient.left;
  FX_FLOAT fTextLeft = 0.0;
  FX_FLOAT fTextRight = 0.0;
  FX_FLOAT fClientRight = m_rtClient.right();
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_LeftText) {
    fBoxLeft = fClientRight - m_fBoxHeight;
    fTextLeft = m_rtClient.left;
    fTextRight = fBoxLeft;
  } else {
    fTextLeft = fBoxLeft + m_fBoxHeight;
    fTextRight = fClientRight;
  }
  m_rtBox.Set(fBoxLeft, fBoxTop, m_fBoxHeight, m_fBoxHeight);
  m_rtCaption.Set(fTextLeft, m_rtClient.top, fTextRight - fTextLeft,
                  m_rtClient.height);
  m_rtCaption.Inflate(-kCaptionMargin, -kCaptionMargin);

  CFX_RectF rtFocus;
  rtFocus.Set(m_rtCaption.left, m_rtCaption.top, m_rtCaption.width,
              m_rtCaption.height);

  CalcTextRect(L"Check box", m_pProperties->m_pThemeProvider, m_dwTTOStyles,
               m_iTTOAlign, rtFocus);
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_MultiLine) == 0) {
    FX_FLOAT fWidth = std::max(m_rtCaption.width, rtFocus.width);
    FX_FLOAT fHeight = std::min(m_rtCaption.height, rtFocus.height);
    FX_FLOAT fLeft = m_rtCaption.left;
    FX_FLOAT fTop = m_rtCaption.top;
    if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_HLayoutMask) ==
        FWL_STYLEEXT_CKB_Center) {
      fLeft = m_rtCaption.left + (m_rtCaption.width - fWidth) / 2;
    } else if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_HLayoutMask) ==
               FWL_STYLEEXT_CKB_Right) {
      fLeft = m_rtCaption.right() - fWidth;
    }
    if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_VLayoutMask) ==
        FWL_STYLEEXT_CKB_VCenter) {
      fTop = m_rtCaption.top + (m_rtCaption.height - fHeight) / 2;
    } else if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_VLayoutMask) ==
               FWL_STYLEEXT_CKB_Bottom) {
      fTop = m_rtCaption.bottom() - fHeight;
    }
    m_rtFocus.Set(fLeft, fTop, fWidth, fHeight);
  } else {
    m_rtFocus.Set(rtFocus.left, rtFocus.top, rtFocus.width, rtFocus.height);
  }
  m_rtFocus.Inflate(1, 1);
}

uint32_t CFWL_CheckBox::GetPartStates() const {
  int32_t dwStates = CFWL_PartState_Normal;
  if ((m_pProperties->m_dwStates & FWL_STATE_CKB_CheckMask) ==
      FWL_STATE_CKB_Neutral) {
    dwStates = CFWL_PartState_Neutral;
  } else if ((m_pProperties->m_dwStates & FWL_STATE_CKB_CheckMask) ==
             FWL_STATE_CKB_Checked) {
    dwStates = CFWL_PartState_Checked;
  }
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)
    dwStates |= CFWL_PartState_Disabled;
  else if (m_pProperties->m_dwStates & FWL_STATE_CKB_Hovered)
    dwStates |= CFWL_PartState_Hovered;
  else if (m_pProperties->m_dwStates & FWL_STATE_CKB_Pressed)
    dwStates |= CFWL_PartState_Pressed;
  else
    dwStates |= CFWL_PartState_Normal;
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Focused)
    dwStates |= CFWL_PartState_Focused;
  return dwStates;
}

void CFWL_CheckBox::UpdateTextOutStyles() {
  switch (m_pProperties->m_dwStyleExes &
          (FWL_STYLEEXT_CKB_HLayoutMask | FWL_STYLEEXT_CKB_VLayoutMask)) {
    case FWL_STYLEEXT_CKB_Left | FWL_STYLEEXT_CKB_Top: {
      m_iTTOAlign = FDE_TTOALIGNMENT_TopLeft;
      break;
    }
    case FWL_STYLEEXT_CKB_Center | FWL_STYLEEXT_CKB_Top: {
      m_iTTOAlign = FDE_TTOALIGNMENT_TopCenter;
      break;
    }
    case FWL_STYLEEXT_CKB_Right | FWL_STYLEEXT_CKB_Top: {
      m_iTTOAlign = FDE_TTOALIGNMENT_TopRight;
      break;
    }
    case FWL_STYLEEXT_CKB_Left | FWL_STYLEEXT_CKB_VCenter: {
      m_iTTOAlign = FDE_TTOALIGNMENT_CenterLeft;
      break;
    }
    case FWL_STYLEEXT_CKB_Right | FWL_STYLEEXT_CKB_VCenter: {
      m_iTTOAlign = FDE_TTOALIGNMENT_CenterRight;
      break;
    }
    case FWL_STYLEEXT_CKB_Left | FWL_STYLEEXT_CKB_Bottom: {
      m_iTTOAlign = FDE_TTOALIGNMENT_BottomLeft;
      break;
    }
    case FWL_STYLEEXT_CKB_Center | FWL_STYLEEXT_CKB_Bottom: {
      m_iTTOAlign = FDE_TTOALIGNMENT_BottomCenter;
      break;
    }
    case FWL_STYLEEXT_CKB_Right | FWL_STYLEEXT_CKB_Bottom: {
      m_iTTOAlign = FDE_TTOALIGNMENT_BottomRight;
      break;
    }
    case FWL_STYLEEXT_CKB_Center | FWL_STYLEEXT_CKB_VCenter:
    default: {
      m_iTTOAlign = FDE_TTOALIGNMENT_Center;
      break;
    }
  }
  m_dwTTOStyles = 0;
  if (m_pProperties->m_dwStyleExes & FWL_WGTSTYLE_RTLReading)
    m_dwTTOStyles |= FDE_TTOSTYLE_RTL;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_MultiLine)
    m_dwTTOStyles |= FDE_TTOSTYLE_LineWrap;
  else
    m_dwTTOStyles |= FDE_TTOSTYLE_SingleLine;
}

void CFWL_CheckBox::NextStates() {
  uint32_t dwFirststate = m_pProperties->m_dwStates;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_RadioButton) {
    if ((m_pProperties->m_dwStates & FWL_STATE_CKB_CheckMask) ==
        FWL_STATE_CKB_Unchecked) {
      CFWL_WidgetMgr* pWidgetMgr = GetOwnerApp()->GetWidgetMgr();
      if (!pWidgetMgr->IsFormDisabled()) {
        CFX_ArrayTemplate<CFWL_Widget*> radioarr;
        pWidgetMgr->GetSameGroupRadioButton(this, radioarr);
        CFWL_CheckBox* pCheckBox = nullptr;
        int32_t iCount = radioarr.GetSize();
        for (int32_t i = 0; i < iCount; i++) {
          pCheckBox = static_cast<CFWL_CheckBox*>(radioarr[i]);
          if (pCheckBox != this &&
              pCheckBox->GetStates() & FWL_STATE_CKB_Checked) {
            pCheckBox->SetCheckState(0);
            CFX_RectF rt = pCheckBox->GetWidgetRect();
            rt.left = rt.top = 0;
            m_pWidgetMgr->RepaintWidget(pCheckBox, rt);
            break;
          }
        }
      }
      m_pProperties->m_dwStates |= FWL_STATE_CKB_Checked;
    }
  } else {
    if ((m_pProperties->m_dwStates & FWL_STATE_CKB_CheckMask) ==
        FWL_STATE_CKB_Neutral) {
      m_pProperties->m_dwStates &= ~FWL_STATE_CKB_CheckMask;
      if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_3State)
        m_pProperties->m_dwStates |= FWL_STATE_CKB_Checked;
    } else if ((m_pProperties->m_dwStates & FWL_STATE_CKB_CheckMask) ==
               FWL_STATE_CKB_Checked) {
      m_pProperties->m_dwStates &= ~FWL_STATE_CKB_CheckMask;
    } else {
      if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CKB_3State)
        m_pProperties->m_dwStates |= FWL_STATE_CKB_Neutral;
      else
        m_pProperties->m_dwStates |= FWL_STATE_CKB_Checked;
    }
  }

  RepaintRect(m_rtClient);
  if (dwFirststate == m_pProperties->m_dwStates)
    return;

  CFWL_Event wmCheckBoxState(CFWL_Event::Type::CheckStateChanged, this);
  DispatchEvent(&wmCheckBoxState);
}

void CFWL_CheckBox::OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage)
    return;

  switch (pMessage->GetType()) {
    case CFWL_Message::Type::SetFocus:
      OnFocusChanged(true);
      break;
    case CFWL_Message::Type::KillFocus:
      OnFocusChanged(false);
      break;
    case CFWL_Message::Type::Mouse: {
      CFWL_MessageMouse* pMsg = static_cast<CFWL_MessageMouse*>(pMessage);
      switch (pMsg->m_dwCmd) {
        case FWL_MouseCommand::LeftButtonDown:
          OnLButtonDown();
          break;
        case FWL_MouseCommand::LeftButtonUp:
          OnLButtonUp(pMsg);
          break;
        case FWL_MouseCommand::Move:
          OnMouseMove(pMsg);
          break;
        case FWL_MouseCommand::Leave:
          OnMouseLeave();
          break;
        default:
          break;
      }
      break;
    }
    case CFWL_Message::Type::Key: {
      CFWL_MessageKey* pKey = static_cast<CFWL_MessageKey*>(pMessage);
      if (pKey->m_dwCmd == FWL_KeyCommand::KeyDown)
        OnKeyDown(pKey);
      break;
    }
    default:
      break;
  }

  CFWL_Widget::OnProcessMessage(pMessage);
}

void CFWL_CheckBox::OnDrawWidget(CFX_Graphics* pGraphics,
                                 const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

void CFWL_CheckBox::OnFocusChanged(bool bSet) {
  if (bSet)
    m_pProperties->m_dwStates |= FWL_WGTSTATE_Focused;
  else
    m_pProperties->m_dwStates &= ~FWL_WGTSTATE_Focused;

  RepaintRect(m_rtClient);
}

void CFWL_CheckBox::OnLButtonDown() {
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)
    return;
  if ((m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) == 0)
    SetFocus(true);

  m_bBtnDown = true;
  m_pProperties->m_dwStates &= ~FWL_STATE_CKB_Hovered;
  m_pProperties->m_dwStates |= FWL_STATE_CKB_Pressed;
  RepaintRect(m_rtClient);
}

void CFWL_CheckBox::OnLButtonUp(CFWL_MessageMouse* pMsg) {
  if (!m_bBtnDown)
    return;

  m_bBtnDown = false;
  if (!m_rtClient.Contains(pMsg->m_fx, pMsg->m_fy))
    return;

  m_pProperties->m_dwStates |= FWL_STATE_CKB_Hovered;
  m_pProperties->m_dwStates &= ~FWL_STATE_CKB_Pressed;
  NextStates();
}

void CFWL_CheckBox::OnMouseMove(CFWL_MessageMouse* pMsg) {
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)
    return;

  bool bRepaint = false;
  if (m_bBtnDown) {
    if (m_rtClient.Contains(pMsg->m_fx, pMsg->m_fy)) {
      if ((m_pProperties->m_dwStates & FWL_STATE_CKB_Pressed) == 0) {
        bRepaint = true;
        m_pProperties->m_dwStates |= FWL_STATE_CKB_Pressed;
      }
      if ((m_pProperties->m_dwStates & FWL_STATE_CKB_Hovered)) {
        bRepaint = true;
        m_pProperties->m_dwStates &= ~FWL_STATE_CKB_Hovered;
      }
    } else {
      if (m_pProperties->m_dwStates & FWL_STATE_CKB_Pressed) {
        bRepaint = true;
        m_pProperties->m_dwStates &= ~FWL_STATE_CKB_Pressed;
      }
      if ((m_pProperties->m_dwStates & FWL_STATE_CKB_Hovered) == 0) {
        bRepaint = true;
        m_pProperties->m_dwStates |= FWL_STATE_CKB_Hovered;
      }
    }
  } else {
    if (m_rtClient.Contains(pMsg->m_fx, pMsg->m_fy)) {
      if ((m_pProperties->m_dwStates & FWL_STATE_CKB_Hovered) == 0) {
        bRepaint = true;
        m_pProperties->m_dwStates |= FWL_STATE_CKB_Hovered;
      }
    }
  }
  if (bRepaint)
    RepaintRect(m_rtBox);
}

void CFWL_CheckBox::OnMouseLeave() {
  if (m_bBtnDown)
    m_pProperties->m_dwStates |= FWL_STATE_CKB_Hovered;
  else
    m_pProperties->m_dwStates &= ~FWL_STATE_CKB_Hovered;

  RepaintRect(m_rtBox);
}

void CFWL_CheckBox::OnKeyDown(CFWL_MessageKey* pMsg) {
  if (pMsg->m_dwKeyCode == FWL_VKEY_Tab)
    return;
  if (pMsg->m_dwKeyCode == FWL_VKEY_Return ||
      pMsg->m_dwKeyCode == FWL_VKEY_Space) {
    NextStates();
  }
}
