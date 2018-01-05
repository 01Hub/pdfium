// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/cxfa_ffcombobox.h"

#include <utility>
#include <vector>

#include "xfa/fwl/cfwl_combobox.h"
#include "xfa/fwl/cfwl_eventselectchanged.h"
#include "xfa/fwl/cfwl_notedriver.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/cxfa_ffdocview.h"
#include "xfa/fxfa/parser/cxfa_para.h"

namespace {

CFWL_ComboBox* ToComboBox(CFWL_Widget* widget) {
  return static_cast<CFWL_ComboBox*>(widget);
}

}  // namespace

CXFA_FFComboBox::CXFA_FFComboBox(CXFA_WidgetAcc* pDataAcc)
    : CXFA_FFField(pDataAcc), m_pOldDelegate(nullptr) {}

CXFA_FFComboBox::~CXFA_FFComboBox() {}

CFX_RectF CXFA_FFComboBox::GetBBox(uint32_t dwStatus, bool bDrawFocus) {
  return bDrawFocus ? CFX_RectF() : CXFA_FFWidget::GetBBox(dwStatus);
}

bool CXFA_FFComboBox::PtInActiveRect(const CFX_PointF& point) {
  auto* pComboBox = ToComboBox(m_pNormalWidget.get());
  return pComboBox && pComboBox->GetBBox().Contains(point);
}

bool CXFA_FFComboBox::LoadWidget() {
  auto pNew = pdfium::MakeUnique<CFWL_ComboBox>(GetFWLApp());
  CFWL_ComboBox* pComboBox = pNew.get();
  m_pNormalWidget = std::move(pNew);
  m_pNormalWidget->SetLayoutItem(this);

  CFWL_NoteDriver* pNoteDriver =
      m_pNormalWidget->GetOwnerApp()->GetNoteDriver();
  pNoteDriver->RegisterEventTarget(m_pNormalWidget.get(),
                                   m_pNormalWidget.get());
  m_pOldDelegate = m_pNormalWidget->GetDelegate();
  m_pNormalWidget->SetDelegate(this);
  m_pNormalWidget->LockUpdate();

  for (const auto& label : m_pDataAcc->GetChoiceListItems(false))
    pComboBox->AddString(label.AsStringView());

  std::vector<int32_t> iSelArray = m_pDataAcc->GetSelectedItems();
  if (iSelArray.empty())
    pComboBox->SetEditText(m_pDataAcc->GetValue(XFA_VALUEPICTURE_Raw));
  else
    pComboBox->SetCurSel(iSelArray.front());

  UpdateWidgetProperty();
  m_pNormalWidget->UnlockUpdate();
  return CXFA_FFField::LoadWidget();
}

void CXFA_FFComboBox::UpdateWidgetProperty() {
  auto* pComboBox = ToComboBox(m_pNormalWidget.get());
  if (!pComboBox)
    return;

  uint32_t dwExtendedStyle = 0;
  uint32_t dwEditStyles = FWL_STYLEEXT_EDT_ReadOnly;
  dwExtendedStyle |= UpdateUIProperty();
  if (m_pDataAcc->IsChoiceListAllowTextEntry()) {
    dwEditStyles &= ~FWL_STYLEEXT_EDT_ReadOnly;
    dwExtendedStyle |= FWL_STYLEEXT_CMB_DropDown;
  }
  if (!m_pDataAcc->IsOpenAccess() || !GetDoc()->GetXFADoc()->IsInteractive()) {
    dwEditStyles |= FWL_STYLEEXT_EDT_ReadOnly;
    dwExtendedStyle |= FWL_STYLEEXT_CMB_ReadOnly;
  }
  dwExtendedStyle |= GetAlignment();
  m_pNormalWidget->ModifyStylesEx(dwExtendedStyle, 0xFFFFFFFF);

  if (!m_pDataAcc->IsHorizontalScrollPolicyOff())
    dwEditStyles |= FWL_STYLEEXT_EDT_AutoHScroll;

  pComboBox->EditModifyStylesEx(dwEditStyles, 0xFFFFFFFF);
}

bool CXFA_FFComboBox::OnRButtonUp(uint32_t dwFlags, const CFX_PointF& point) {
  if (!CXFA_FFField::OnRButtonUp(dwFlags, point))
    return false;

  GetDoc()->GetDocEnvironment()->PopupMenu(this, point);
  return true;
}

bool CXFA_FFComboBox::OnKillFocus(CXFA_FFWidget* pNewWidget) {
  if (!ProcessCommittedData())
    UpdateFWLData();

  CXFA_FFField::OnKillFocus(pNewWidget);
  return true;
}

void CXFA_FFComboBox::OpenDropDownList() {
  ToComboBox(m_pNormalWidget.get())->OpenDropDownList(true);
}

bool CXFA_FFComboBox::CommitData() {
  return m_pDataAcc->SetValue(XFA_VALUEPICTURE_Raw, m_wsNewValue);
}

bool CXFA_FFComboBox::IsDataChanged() {
  auto* pFWLcombobox = ToComboBox(m_pNormalWidget.get());
  WideString wsText = pFWLcombobox->GetEditText();
  int32_t iCursel = pFWLcombobox->GetCurSel();
  if (iCursel >= 0) {
    WideString wsSel = pFWLcombobox->GetTextByIndex(iCursel);
    if (wsSel == wsText)
      wsText = m_pDataAcc->GetChoiceListItem(iCursel, true).value_or(L"");
  }
  if (m_pDataAcc->GetValue(XFA_VALUEPICTURE_Raw) == wsText)
    return false;

  m_wsNewValue = wsText;
  return true;
}

void CXFA_FFComboBox::FWLEventSelChange(CXFA_EventParam* pParam) {
  pParam->m_eType = XFA_EVENT_Change;
  pParam->m_pTarget = m_pDataAcc.Get();
  pParam->m_wsNewText = ToComboBox(m_pNormalWidget.get())->GetEditText();
  m_pDataAcc->ProcessEvent(GetDocView(), XFA_AttributeEnum::Change, pParam);
}

uint32_t CXFA_FFComboBox::GetAlignment() {
  CXFA_Para* para = m_pDataAcc->GetPara();
  if (!para)
    return 0;

  uint32_t dwExtendedStyle = 0;
  switch (para->GetHorizontalAlign()) {
    case XFA_AttributeEnum::Center:
      dwExtendedStyle |=
          FWL_STYLEEXT_CMB_EditHCenter | FWL_STYLEEXT_CMB_ListItemCenterAlign;
      break;
    case XFA_AttributeEnum::Justify:
      dwExtendedStyle |= FWL_STYLEEXT_CMB_EditJustified;
      break;
    case XFA_AttributeEnum::JustifyAll:
      break;
    case XFA_AttributeEnum::Radix:
      break;
    case XFA_AttributeEnum::Right:
      break;
    default:
      dwExtendedStyle |=
          FWL_STYLEEXT_CMB_EditHNear | FWL_STYLEEXT_CMB_ListItemLeftAlign;
      break;
  }

  switch (para->GetVerticalAlign()) {
    case XFA_AttributeEnum::Middle:
      dwExtendedStyle |= FWL_STYLEEXT_CMB_EditVCenter;
      break;
    case XFA_AttributeEnum::Bottom:
      dwExtendedStyle |= FWL_STYLEEXT_CMB_EditVFar;
      break;
    default:
      dwExtendedStyle |= FWL_STYLEEXT_CMB_EditVNear;
      break;
  }
  return dwExtendedStyle;
}

bool CXFA_FFComboBox::UpdateFWLData() {
  auto* pComboBox = ToComboBox(m_pNormalWidget.get());
  if (!pComboBox)
    return false;

  std::vector<int32_t> iSelArray = m_pDataAcc->GetSelectedItems();
  if (!iSelArray.empty()) {
    pComboBox->SetCurSel(iSelArray.front());
  } else {
    pComboBox->SetCurSel(-1);
    pComboBox->SetEditText(m_pDataAcc->GetValue(XFA_VALUEPICTURE_Raw));
  }
  pComboBox->Update();
  return true;
}

bool CXFA_FFComboBox::CanUndo() {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditCanUndo();
}

bool CXFA_FFComboBox::CanRedo() {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditCanRedo();
}

bool CXFA_FFComboBox::Undo() {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditUndo();
}

bool CXFA_FFComboBox::Redo() {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditRedo();
}

bool CXFA_FFComboBox::CanCopy() {
  return ToComboBox(m_pNormalWidget.get())->EditCanCopy();
}

bool CXFA_FFComboBox::CanCut() {
  return m_pDataAcc->IsOpenAccess() &&
         m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditCanCut();
}

bool CXFA_FFComboBox::CanPaste() {
  return m_pDataAcc->IsChoiceListAllowTextEntry() && m_pDataAcc->IsOpenAccess();
}

bool CXFA_FFComboBox::CanSelectAll() {
  return ToComboBox(m_pNormalWidget.get())->EditCanSelectAll();
}

Optional<WideString> CXFA_FFComboBox::Copy() {
  return ToComboBox(m_pNormalWidget.get())->EditCopy();
}

Optional<WideString> CXFA_FFComboBox::Cut() {
  if (!m_pDataAcc->IsChoiceListAllowTextEntry())
    return {};

  return ToComboBox(m_pNormalWidget.get())->EditCut();
}

bool CXFA_FFComboBox::Paste(const WideString& wsPaste) {
  return m_pDataAcc->IsChoiceListAllowTextEntry() &&
         ToComboBox(m_pNormalWidget.get())->EditPaste(wsPaste);
}

void CXFA_FFComboBox::SelectAll() {
  ToComboBox(m_pNormalWidget.get())->EditSelectAll();
}

void CXFA_FFComboBox::Delete() {
  ToComboBox(m_pNormalWidget.get())->EditDelete();
}

void CXFA_FFComboBox::DeSelect() {
  ToComboBox(m_pNormalWidget.get())->EditDeSelect();
}

void CXFA_FFComboBox::SetItemState(int32_t nIndex, bool bSelected) {
  ToComboBox(m_pNormalWidget.get())->SetCurSel(bSelected ? nIndex : -1);
  m_pNormalWidget->Update();
  AddInvalidateRect();
}

void CXFA_FFComboBox::InsertItem(const WideStringView& wsLabel,
                                 int32_t nIndex) {
  ToComboBox(m_pNormalWidget.get())->AddString(wsLabel);
  m_pNormalWidget->Update();
  AddInvalidateRect();
}

void CXFA_FFComboBox::DeleteItem(int32_t nIndex) {
  if (nIndex < 0)
    ToComboBox(m_pNormalWidget.get())->RemoveAll();
  else
    ToComboBox(m_pNormalWidget.get())->RemoveAt(nIndex);

  m_pNormalWidget->Update();
  AddInvalidateRect();
}

void CXFA_FFComboBox::OnTextChanged(CFWL_Widget* pWidget,
                                    const WideString& wsChanged) {
  CXFA_EventParam eParam;
  eParam.m_wsPrevText = m_pDataAcc->GetValue(XFA_VALUEPICTURE_Raw);
  eParam.m_wsChange = wsChanged;
  FWLEventSelChange(&eParam);
}

void CXFA_FFComboBox::OnSelectChanged(CFWL_Widget* pWidget, bool bLButtonUp) {
  CXFA_EventParam eParam;
  eParam.m_wsPrevText = m_pDataAcc->GetValue(XFA_VALUEPICTURE_Raw);
  FWLEventSelChange(&eParam);
  if (m_pDataAcc->IsChoiceListCommitOnSelect() && bLButtonUp)
    m_pDocView->SetFocusWidgetAcc(nullptr);
}

void CXFA_FFComboBox::OnPreOpen(CFWL_Widget* pWidget) {
  CXFA_EventParam eParam;
  eParam.m_eType = XFA_EVENT_PreOpen;
  eParam.m_pTarget = m_pDataAcc.Get();
  m_pDataAcc->ProcessEvent(GetDocView(), XFA_AttributeEnum::PreOpen, &eParam);
}

void CXFA_FFComboBox::OnPostOpen(CFWL_Widget* pWidget) {
  CXFA_EventParam eParam;
  eParam.m_eType = XFA_EVENT_PostOpen;
  eParam.m_pTarget = m_pDataAcc.Get();
  m_pDataAcc->ProcessEvent(GetDocView(), XFA_AttributeEnum::PostOpen, &eParam);
}

void CXFA_FFComboBox::OnProcessMessage(CFWL_Message* pMessage) {
  m_pOldDelegate->OnProcessMessage(pMessage);
}

void CXFA_FFComboBox::OnProcessEvent(CFWL_Event* pEvent) {
  CXFA_FFField::OnProcessEvent(pEvent);
  switch (pEvent->GetType()) {
    case CFWL_Event::Type::SelectChanged: {
      auto* postEvent = static_cast<CFWL_EventSelectChanged*>(pEvent);
      OnSelectChanged(m_pNormalWidget.get(), postEvent->bLButtonUp);
      break;
    }
    case CFWL_Event::Type::EditChanged: {
      WideString wsChanged;
      OnTextChanged(m_pNormalWidget.get(), wsChanged);
      break;
    }
    case CFWL_Event::Type::PreDropDown: {
      OnPreOpen(m_pNormalWidget.get());
      break;
    }
    case CFWL_Event::Type::PostDropDown: {
      OnPostOpen(m_pNormalWidget.get());
      break;
    }
    default:
      break;
  }
  m_pOldDelegate->OnProcessEvent(pEvent);
}

void CXFA_FFComboBox::OnDrawWidget(CXFA_Graphics* pGraphics,
                                   const CFX_Matrix& matrix) {
  m_pOldDelegate->OnDrawWidget(pGraphics, matrix);
}
