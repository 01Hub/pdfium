// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/cfwl_form.h"

#include <utility>

#include "third_party/base/ptr_util.h"
#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fwl/cfwl_app.h"
#include "xfa/fwl/cfwl_event.h"
#include "xfa/fwl/cfwl_formproxy.h"
#include "xfa/fwl/cfwl_messagemouse.h"
#include "xfa/fwl/cfwl_notedriver.h"
#include "xfa/fwl/cfwl_noteloop.h"
#include "xfa/fwl/cfwl_sysbtn.h"
#include "xfa/fwl/cfwl_themebackground.h"
#include "xfa/fwl/cfwl_themepart.h"
#include "xfa/fwl/cfwl_themetext.h"
#include "xfa/fwl/cfwl_widgetmgr.h"
#include "xfa/fwl/ifwl_themeprovider.h"
#include "xfa/fwl/theme/cfwl_widgettp.h"

namespace {

const int kSystemButtonSize = 21;
const int kSystemButtonMargin = 5;
const int kSystemButtonSpan = 2;

}  // namespace

namespace {

const uint8_t kCornerEnlarge = 10;

}  // namespace

CFWL_Form::CFWL_Form(const CFWL_App* app,
                     std::unique_ptr<CFWL_WidgetProperties> properties,
                     CFWL_Widget* pOuter)
    : CFWL_Widget(app, std::move(properties), pOuter),
#if (_FX_OS_ == _FX_MACOSX_)
      m_bMouseIn(false),
#endif
      m_pCloseBox(nullptr),
      m_pMinBox(nullptr),
      m_pMaxBox(nullptr),
      m_pSubFocus(nullptr),
      m_fCXBorder(0),
      m_fCYBorder(0),
      m_iCaptureBtn(-1),
      m_iSysBox(0),
      m_bLButtonDown(false),
      m_bMaximized(false),
      m_bSetMaximize(false),
      m_bDoModalFlag(false) {
  m_rtRelative.Reset();
  m_rtRestore.Reset();

  RegisterForm();
  RegisterEventTarget(nullptr);
}

CFWL_Form::~CFWL_Form() {
  UnregisterEventTarget();
  UnRegisterForm();
  RemoveSysButtons();
}

FWL_Type CFWL_Form::GetClassID() const {
  return FWL_Type::Form;
}

bool CFWL_Form::IsInstance(const CFX_WideStringC& wsClass) const {
  if (wsClass == CFX_WideStringC(FWL_CLASS_Form))
    return true;
  return CFWL_Widget::IsInstance(wsClass);
}

CFX_RectF CFWL_Form::GetClientRect() {
  CFX_RectF rect = m_pProperties->m_rtWidget;
  rect.Offset(-rect.left, -rect.top);
  return rect;
}

void CFWL_Form::Update() {
  if (m_iLock > 0)
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  Layout();
}

FWL_WidgetHit CFWL_Form::HitTest(FX_FLOAT fx, FX_FLOAT fy) {
  GetAvailableTheme();

  if (m_pCloseBox && m_pCloseBox->m_rtBtn.Contains(fx, fy))
    return FWL_WidgetHit::CloseBox;
  if (m_pMaxBox && m_pMaxBox->m_rtBtn.Contains(fx, fy))
    return FWL_WidgetHit::MaxBox;
  if (m_pMinBox && m_pMinBox->m_rtBtn.Contains(fx, fy))
    return FWL_WidgetHit::MinBox;

  CFX_RectF rtCap;
  rtCap.Set(m_fCYBorder, m_fCXBorder,
            0 - kSystemButtonSize * m_iSysBox - 2 * m_fCYBorder,
            0 - m_fCXBorder);
  if (rtCap.Contains(fx, fy))
    return FWL_WidgetHit::Titlebar;
  if ((m_pProperties->m_dwStyles & FWL_WGTSTYLE_Border) &&
      (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_FRM_Resize)) {
    FX_FLOAT fWidth = m_rtRelative.width - 2 * (m_fCYBorder + kCornerEnlarge);
    FX_FLOAT fHeight = m_rtRelative.height - 2 * (m_fCXBorder + kCornerEnlarge);

    CFX_RectF rt;
    rt.Set(0, m_fCXBorder + kCornerEnlarge, m_fCYBorder, fHeight);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::Left;

    rt.Set(m_rtRelative.width - m_fCYBorder, m_fCXBorder + kCornerEnlarge,
           m_fCYBorder, fHeight);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::Right;

    rt.Set(m_fCYBorder + kCornerEnlarge, 0, fWidth, m_fCXBorder);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::Top;

    rt.Set(m_fCYBorder + kCornerEnlarge, m_rtRelative.height - m_fCXBorder,
           fWidth, m_fCXBorder);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::Bottom;

    rt.Set(0, 0, m_fCYBorder + kCornerEnlarge, m_fCXBorder + kCornerEnlarge);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::LeftTop;

    rt.Set(0, m_rtRelative.height - m_fCXBorder - kCornerEnlarge,
           m_fCYBorder + kCornerEnlarge, m_fCXBorder + kCornerEnlarge);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::LeftBottom;

    rt.Set(m_rtRelative.width - m_fCYBorder - kCornerEnlarge, 0,
           m_fCYBorder + kCornerEnlarge, m_fCXBorder + kCornerEnlarge);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::RightTop;

    rt.Set(m_rtRelative.width - m_fCYBorder - kCornerEnlarge,
           m_rtRelative.height - m_fCXBorder - kCornerEnlarge,
           m_fCYBorder + kCornerEnlarge, m_fCXBorder + kCornerEnlarge);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::RightBottom;
  }
  return FWL_WidgetHit::Client;
}

void CFWL_Form::DrawWidget(CFX_Graphics* pGraphics, const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    return;

  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  bool bInactive = !IsActive();
  int32_t iState = bInactive ? CFWL_PartState_Inactive : CFWL_PartState_Normal;
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_FRM_NoDrawClient) == 0)
    DrawBackground(pGraphics, pTheme);

#ifdef FWL_UseMacSystemBorder
  return;
#endif
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_dwStates = iState;
  param.m_pGraphics = pGraphics;
  param.m_rtPart = m_rtRelative;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_Border) {
    param.m_iPart = CFWL_Part::Border;
    pTheme->DrawBackground(&param);
  }
  if ((m_pProperties->m_dwStyleExes & FWL_WGTSTYLE_EdgeMask) !=
      FWL_WGTSTYLE_EdgeNone) {
    CFX_RectF rtEdge;
    GetEdgeRect(rtEdge);
    param.m_iPart = CFWL_Part::Edge;
    param.m_rtPart = rtEdge;
    param.m_dwStates = iState;
    pTheme->DrawBackground(&param);
  }

#if (_FX_OS_ == _FX_MACOSX_)
  {
    if (m_pCloseBox) {
      param.m_iPart = CFWL_Part::CloseBox;
      param.m_dwStates = m_pCloseBox->GetPartState();
      if (m_pProperties->m_dwStates & FWL_WGTSTATE_Deactivated)
        param.m_dwStates = CFWL_PartState_Disabled;
      else if (CFWL_PartState_Normal == param.m_dwStates && m_bMouseIn)
        param.m_dwStates = CFWL_PartState_Hovered;
      param.m_rtPart = m_pCloseBox->m_rtBtn;
      pTheme->DrawBackground(&param);
    }
    if (m_pMaxBox) {
      param.m_iPart = CFWL_Part::MaximizeBox;
      param.m_dwStates = m_pMaxBox->GetPartState();
      if (m_pProperties->m_dwStates & FWL_WGTSTATE_Deactivated)
        param.m_dwStates = CFWL_PartState_Disabled;
      else if (CFWL_PartState_Normal == param.m_dwStates && m_bMouseIn)
        param.m_dwStates = CFWL_PartState_Hovered;
      param.m_rtPart = m_pMaxBox->m_rtBtn;
      param.m_bMaximize = m_bMaximized;
      pTheme->DrawBackground(&param);
    }
    if (m_pMinBox) {
      param.m_iPart = CFWL_Part::MinimizeBox;
      param.m_dwStates = m_pMinBox->GetPartState();
      if (m_pProperties->m_dwStates & FWL_WGTSTATE_Deactivated)
        param.m_dwStates = CFWL_PartState_Disabled;
      else if (CFWL_PartState_Normal == param.m_dwStates && m_bMouseIn)
        param.m_dwStates = CFWL_PartState_Hovered;
      param.m_rtPart = m_pMinBox->m_rtBtn;
      pTheme->DrawBackground(&param);
    }
    m_bMouseIn = false;
  }
#else
  {
    if (m_pCloseBox) {
      param.m_iPart = CFWL_Part::CloseBox;
      param.m_dwStates = m_pCloseBox->GetPartState();
      param.m_rtPart = m_pCloseBox->m_rtBtn;
      pTheme->DrawBackground(&param);
    }
    if (m_pMaxBox) {
      param.m_iPart = CFWL_Part::MaximizeBox;
      param.m_dwStates = m_pMaxBox->GetPartState();
      param.m_rtPart = m_pMaxBox->m_rtBtn;
      param.m_bMaximize = m_bMaximized;
      pTheme->DrawBackground(&param);
    }
    if (m_pMinBox) {
      param.m_iPart = CFWL_Part::MinimizeBox;
      param.m_dwStates = m_pMinBox->GetPartState();
      param.m_rtPart = m_pMinBox->m_rtBtn;
      pTheme->DrawBackground(&param);
    }
  }
#endif
}

CFWL_Widget* CFWL_Form::DoModal() {
  const CFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return nullptr;

  CFWL_NoteDriver* pDriver = pApp->GetNoteDriver();
  if (!pDriver)
    return nullptr;

  m_pNoteLoop = pdfium::MakeUnique<CFWL_NoteLoop>();
  m_pNoteLoop->SetMainForm(this);

  pDriver->PushNoteLoop(m_pNoteLoop.get());
  m_bDoModalFlag = true;
  RemoveStates(FWL_WGTSTATE_Invisible);
  pDriver->Run();

#if _FX_OS_ != _FX_MACOSX_
  pDriver->PopNoteLoop();
#endif

  m_pNoteLoop.reset();
  return nullptr;
}

void CFWL_Form::EndDoModal() {
  if (!m_pNoteLoop)
    return;

  m_bDoModalFlag = false;

#if (_FX_OS_ == _FX_MACOSX_)
  m_pNoteLoop->EndModalLoop();
  const CFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  if (!pDriver)
    return;

  pDriver->PopNoteLoop();
  SetStates(FWL_WGTSTATE_Invisible);
#else
  SetStates(FWL_WGTSTATE_Invisible);
  m_pNoteLoop->EndModalLoop();
#endif
}

void CFWL_Form::DrawBackground(CFX_Graphics* pGraphics,
                               IFWL_ThemeProvider* pTheme) {
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::Background;
  param.m_pGraphics = pGraphics;
  param.m_rtPart = m_rtRelative;
  param.m_rtPart.Deflate(m_fCYBorder, m_fCXBorder, m_fCYBorder, m_fCXBorder);
  pTheme->DrawBackground(&param);
}

void CFWL_Form::RemoveSysButtons() {
  delete m_pCloseBox;
  m_pCloseBox = nullptr;
  delete m_pMinBox;
  m_pMinBox = nullptr;
  delete m_pMaxBox;
  m_pMaxBox = nullptr;
}

CFWL_SysBtn* CFWL_Form::GetSysBtnAtPoint(FX_FLOAT fx, FX_FLOAT fy) {
  if (m_pCloseBox && m_pCloseBox->m_rtBtn.Contains(fx, fy))
    return m_pCloseBox;
  if (m_pMaxBox && m_pMaxBox->m_rtBtn.Contains(fx, fy))
    return m_pMaxBox;
  if (m_pMinBox && m_pMinBox->m_rtBtn.Contains(fx, fy))
    return m_pMinBox;
  return nullptr;
}

CFWL_SysBtn* CFWL_Form::GetSysBtnByState(uint32_t dwState) {
  if (m_pCloseBox && (m_pCloseBox->m_dwState & dwState))
    return m_pCloseBox;
  if (m_pMaxBox && (m_pMaxBox->m_dwState & dwState))
    return m_pMaxBox;
  if (m_pMinBox && (m_pMinBox->m_dwState & dwState))
    return m_pMinBox;
  return nullptr;
}

CFWL_SysBtn* CFWL_Form::GetSysBtnByIndex(int32_t nIndex) {
  if (nIndex < 0)
    return nullptr;

  CFX_ArrayTemplate<CFWL_SysBtn*> arrBtn;
  if (m_pMinBox)
    arrBtn.Add(m_pMinBox);
  if (m_pMaxBox)
    arrBtn.Add(m_pMaxBox);
  if (m_pCloseBox)
    arrBtn.Add(m_pCloseBox);
  return arrBtn[nIndex];
}

int32_t CFWL_Form::GetSysBtnIndex(CFWL_SysBtn* pBtn) {
  CFX_ArrayTemplate<CFWL_SysBtn*> arrBtn;
  if (m_pMinBox)
    arrBtn.Add(m_pMinBox);
  if (m_pMaxBox)
    arrBtn.Add(m_pMaxBox);
  if (m_pCloseBox)
    arrBtn.Add(m_pCloseBox);
  return arrBtn.Find(pBtn);
}

void CFWL_Form::GetEdgeRect(CFX_RectF& rtEdge) {
  rtEdge = m_rtRelative;
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_Border) {
    FX_FLOAT fCX = GetBorderSize(true);
    FX_FLOAT fCY = GetBorderSize(false);
    rtEdge.Deflate(fCX, fCY, fCX, fCY);
  }
}

void CFWL_Form::SetWorkAreaRect() {
  m_rtRestore = m_pProperties->m_rtWidget;
  CFWL_WidgetMgr* pWidgetMgr = GetOwnerApp()->GetWidgetMgr();
  if (!pWidgetMgr)
    return;

  m_bSetMaximize = true;
  RepaintRect(m_rtRelative);
}

void CFWL_Form::Layout() {
  m_rtRelative = GetRelativeRect();

#ifndef FWL_UseMacSystemBorder
  ResetSysBtn();
#endif
}

void CFWL_Form::ResetSysBtn() {
  m_fCXBorder =
      *static_cast<FX_FLOAT*>(GetThemeCapacity(CFWL_WidgetCapacity::CXBorder));
  m_fCYBorder =
      *static_cast<FX_FLOAT*>(GetThemeCapacity(CFWL_WidgetCapacity::CYBorder));
  RemoveSysButtons();

  m_iSysBox = 0;
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_CloseBox) {
    m_pCloseBox = new CFWL_SysBtn;
    m_pCloseBox->m_rtBtn.Set(
        m_rtRelative.right() - kSystemButtonMargin - kSystemButtonSize,
        kSystemButtonMargin, kSystemButtonSize, kSystemButtonSize);
    m_iSysBox++;
  }
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_MaximizeBox) {
    m_pMaxBox = new CFWL_SysBtn;
    if (m_pCloseBox) {
      m_pMaxBox->m_rtBtn.Set(
          m_pCloseBox->m_rtBtn.left - kSystemButtonSpan - kSystemButtonSize,
          m_pCloseBox->m_rtBtn.top, kSystemButtonSize, kSystemButtonSize);
    } else {
      m_pMaxBox->m_rtBtn.Set(
          m_rtRelative.right() - kSystemButtonMargin - kSystemButtonSize,
          kSystemButtonMargin, kSystemButtonSize, kSystemButtonSize);
    }
    m_iSysBox++;
  }
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_MinimizeBox) {
    m_pMinBox = new CFWL_SysBtn;
    if (m_pMaxBox) {
      m_pMinBox->m_rtBtn.Set(
          m_pMaxBox->m_rtBtn.left - kSystemButtonSpan - kSystemButtonSize,
          m_pMaxBox->m_rtBtn.top, kSystemButtonSize, kSystemButtonSize);
    } else if (m_pCloseBox) {
      m_pMinBox->m_rtBtn.Set(
          m_pCloseBox->m_rtBtn.left - kSystemButtonSpan - kSystemButtonSize,
          m_pCloseBox->m_rtBtn.top, kSystemButtonSize, kSystemButtonSize);
    } else {
      m_pMinBox->m_rtBtn.Set(
          m_rtRelative.right() - kSystemButtonMargin - kSystemButtonSize,
          kSystemButtonMargin, kSystemButtonSize, kSystemButtonSize);
    }
    m_iSysBox++;
  }
}

void CFWL_Form::RegisterForm() {
  const CFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  if (!pDriver)
    return;

  pDriver->RegisterForm(this);
}

void CFWL_Form::UnRegisterForm() {
  const CFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  if (!pDriver)
    return;

  pDriver->UnRegisterForm(this);
}

void CFWL_Form::OnProcessMessage(CFWL_Message* pMessage) {
#ifndef FWL_UseMacSystemBorder
  if (!pMessage)
    return;

  switch (pMessage->GetType()) {
    case CFWL_Message::Type::Mouse: {
      CFWL_MessageMouse* pMsg = static_cast<CFWL_MessageMouse*>(pMessage);
      switch (pMsg->m_dwCmd) {
        case FWL_MouseCommand::LeftButtonDown:
          OnLButtonDown(pMsg);
          break;
        case FWL_MouseCommand::LeftButtonUp:
          OnLButtonUp(pMsg);
          break;
        case FWL_MouseCommand::Move:
          OnMouseMove(pMsg);
          break;
        case FWL_MouseCommand::Leave:
          OnMouseLeave(pMsg);
          break;
        case FWL_MouseCommand::LeftButtonDblClk:
          OnLButtonDblClk(pMsg);
          break;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
#endif  // FWL_UseMacSystemBorder
}

void CFWL_Form::OnDrawWidget(CFX_Graphics* pGraphics,
                             const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

void CFWL_Form::OnLButtonDown(CFWL_MessageMouse* pMsg) {
  SetGrab(true);
  m_bLButtonDown = true;

  CFWL_SysBtn* pPressBtn = GetSysBtnAtPoint(pMsg->m_fx, pMsg->m_fy);
  m_iCaptureBtn = GetSysBtnIndex(pPressBtn);

  if (!pPressBtn)
    return;

  pPressBtn->SetPressed();
  RepaintRect(pPressBtn->m_rtBtn);
}

void CFWL_Form::OnLButtonUp(CFWL_MessageMouse* pMsg) {
  SetGrab(false);
  m_bLButtonDown = false;
  CFWL_SysBtn* pPointBtn = GetSysBtnAtPoint(pMsg->m_fx, pMsg->m_fy);
  CFWL_SysBtn* pPressedBtn = GetSysBtnByIndex(m_iCaptureBtn);
  if (!pPressedBtn || pPointBtn != pPressedBtn)
    return;
  if (pPressedBtn == GetSysBtnByState(FWL_SYSBUTTONSTATE_Pressed))
    pPressedBtn->SetNormal();
  if (pPressedBtn == m_pMaxBox) {
    if (m_bMaximized) {
      SetWidgetRect(m_rtRestore);
      Update();
      Repaint();
    } else {
      SetWorkAreaRect();
      Update();
    }
    m_bMaximized = !m_bMaximized;
  } else if (pPressedBtn != m_pMinBox) {
    CFWL_Event eClose(CFWL_Event::Type::Close, this);
    DispatchEvent(&eClose);
  }
}

void CFWL_Form::OnMouseMove(CFWL_MessageMouse* pMsg) {
  if (m_bLButtonDown)
    return;

  CFX_RectF rtInvalidate;
  rtInvalidate.Reset();
  CFWL_SysBtn* pPointBtn = GetSysBtnAtPoint(pMsg->m_fx, pMsg->m_fy);
  CFWL_SysBtn* pOldHover = GetSysBtnByState(FWL_SYSBUTTONSTATE_Hover);

#if _FX_OS_ == _FX_MACOSX_
  {
    if (pOldHover && pPointBtn != pOldHover)
      pOldHover->SetNormal();
    if (pPointBtn && pPointBtn != pOldHover)
      pPointBtn->SetHover();
    if (m_pCloseBox)
      rtInvalidate = m_pCloseBox->m_rtBtn;
    if (m_pMaxBox) {
      if (rtInvalidate.IsEmpty())
        rtInvalidate = m_pMaxBox->m_rtBtn;
      else
        rtInvalidate.Union(m_pMaxBox->m_rtBtn);
    }
    if (m_pMinBox) {
      if (rtInvalidate.IsEmpty())
        rtInvalidate = m_pMinBox->m_rtBtn;
      else
        rtInvalidate.Union(m_pMinBox->m_rtBtn);
    }
    if (!rtInvalidate.IsEmpty() &&
        rtInvalidate.Contains(pMsg->m_fx, pMsg->m_fy)) {
      m_bMouseIn = true;
    }
  }
#else
  {
    if (pOldHover && pPointBtn != pOldHover) {
      pOldHover->SetNormal();
      rtInvalidate = pOldHover->m_rtBtn;
    }
    if (pPointBtn && pPointBtn != pOldHover) {
      pPointBtn->SetHover();
      if (rtInvalidate.IsEmpty())
        rtInvalidate = pPointBtn->m_rtBtn;
      else
        rtInvalidate.Union(pPointBtn->m_rtBtn);
    }
  }
#endif

  if (!rtInvalidate.IsEmpty())
    RepaintRect(rtInvalidate);
}

void CFWL_Form::OnMouseLeave(CFWL_MessageMouse* pMsg) {
  CFWL_SysBtn* pHover = GetSysBtnByState(FWL_SYSBUTTONSTATE_Hover);
  if (!pHover)
    return;

  pHover->SetNormal();
  RepaintRect(pHover->m_rtBtn);
}

void CFWL_Form::OnLButtonDblClk(CFWL_MessageMouse* pMsg) {
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_FRM_Resize) &&
      HitTest(pMsg->m_fx, pMsg->m_fy) == FWL_WidgetHit::Titlebar) {
    if (m_bMaximized)
      SetWidgetRect(m_rtRestore);
    else
      SetWorkAreaRect();

    Update();
    m_bMaximized = !m_bMaximized;
  }
}
