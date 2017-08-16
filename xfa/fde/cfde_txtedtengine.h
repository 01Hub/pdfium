// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FDE_CFDE_TXTEDTENGINE_H_
#define XFA_FDE_CFDE_TXTEDTENGINE_H_

#include <memory>
#include <vector>

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/cfx_seekablestreamproxy.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxge/fx_dib.h"
#include "xfa/fde/cfde_txtedtbuf.h"
#include "xfa/fde/cfde_txtedtpage.h"
#include "xfa/fde/cfde_txtedtparag.h"

class CFGAS_GEFont;
class CFWL_Edit;
class CFX_TxtBreak;
class IFDE_TxtEdtDoRecord;
class IFX_CharIter;

#define FDE_TEXTEDITMODE_MultiLines (1L << 0)
#define FDE_TEXTEDITMODE_AutoLineWrap (1L << 1)
#define FDE_TEXTEDITMODE_LimitArea_Vert (1L << 3)
#define FDE_TEXTEDITMODE_LimitArea_Horz (1L << 4)
#define FDE_TEXTEDITMODE_Validate (1L << 8)
#define FDE_TEXTEDITMODE_Password (1L << 9)

#define FDE_TEXTEDITALIGN_Left 0
#define FDE_TEXTEDITALIGN_Center (1L << 0)
#define FDE_TEXTEDITALIGN_Right (1L << 1)
#define FDE_TEXTEDITALIGN_Justified (1L << 4)

#define FDE_TEXTEDITLAYOUT_CombText (1L << 4)
#define FDE_TEXTEDITLAYOUT_LastLineHeight (1L << 8)

enum FDE_TXTEDTMOVECARET {
  MC_MoveNone = 0,
  MC_Left,
  MC_Right,
  MC_Up,
  MC_Down,
  MC_LineStart,
  MC_LineEnd,
  MC_Home,
  MC_End,
};

struct FDE_TXTEDTPARAMS {
  FDE_TXTEDTPARAMS();
  ~FDE_TXTEDTPARAMS();

  float fPlateWidth;
  float fPlateHeight;
  int32_t nLineCount;
  uint32_t dwLayoutStyles;
  uint32_t dwAlignment;
  uint32_t dwMode;
  CFX_RetainPtr<CFGAS_GEFont> pFont;
  float fFontSize;
  FX_ARGB dwFontColor;
  float fLineSpace;
  float fTabWidth;
  wchar_t wDefChar;
  wchar_t wLineBreakChar;
  int32_t nLineEnd;
  int32_t nHorzScale;
  float fCharSpace;
  CFWL_Edit* pEventSink;
};

enum FDE_TXTEDT_TEXTCHANGE_TYPE {
  FDE_TXTEDT_TEXTCHANGE_TYPE_Insert = 0,
  FDE_TXTEDT_TEXTCHANGE_TYPE_Delete,
  FDE_TXTEDT_TEXTCHANGE_TYPE_Replace,
};

struct FDE_TXTEDT_TEXTCHANGE_INFO {
  FDE_TXTEDT_TEXTCHANGE_INFO();
  ~FDE_TXTEDT_TEXTCHANGE_INFO();

  int32_t nChangeType;
  CFX_WideString wsInsert;
  CFX_WideString wsDelete;
  CFX_WideString wsPrevText;
};

class CFDE_TxtEdtEngine {
 public:
  CFDE_TxtEdtEngine();
  ~CFDE_TxtEdtEngine();

  void SetEditParams(const FDE_TXTEDTPARAMS& params);
  FDE_TXTEDTPARAMS* GetEditParams() { return &m_Param; }

  int32_t CountPages() const {
    return m_nLineCount == 0 ? 0 : ((m_nLineCount - 1) / m_nPageLineCount) + 1;
  }
  CFDE_TxtEdtPage* GetPage(int32_t nIndex);

  void SetTextByStream(const CFX_RetainPtr<CFX_SeekableStreamProxy>& pStream);
  void SetText(const CFX_WideString& wsText);
  int32_t GetTextLength() const { return GetTextBufLength(); }
  CFX_WideString GetText(int32_t nStart, int32_t nCount) const;
  void ClearText();

  CFX_RectF GetCaretRect() const { return m_rtCaret; }
  int32_t GetCaretPos() const {
    return IsLocked() ? 0 : m_nCaret + (m_bBefore ? 0 : 1);
  }
  int32_t SetCaretPos(int32_t nIndex, bool bBefore);
  int32_t MoveCaretPos(FDE_TXTEDTMOVECARET eMoveCaret, bool bShift, bool bCtrl);
  bool IsLocked() const { return m_bLock; }

  int32_t Insert(int32_t nStart, const wchar_t* lpText, int32_t nLength);
  int32_t Delete(int32_t nStart, bool bBackspace);
  int32_t DeleteRange(int32_t nStart, size_t nCount);
  int32_t Replace(int32_t nStart,
                  int32_t nLength,
                  const CFX_WideString& wsReplace);

  void SetLimit(int32_t nLimit) { m_nLimit = nLimit; }
  int32_t GetLimit() const { return m_nLimit; }
  void SetAliasChar(wchar_t wcAlias) { m_wcAliasChar = wcAlias; }

  void RemoveSelRange(int32_t nStart, int32_t nCount);

  void AddSelRange(int32_t nStart, int32_t nCount);
  int32_t CountSelRanges() const {
    return pdfium::CollectionSize<int32_t>(m_SelRangePtrArr);
  }
  int32_t GetSelRange(int32_t nIndex, int32_t* nStart) const;
  void ClearSelection();

  bool Redo(const IFDE_TxtEdtDoRecord* pRecord);
  bool Undo(const IFDE_TxtEdtDoRecord* pRecord);

  void Layout();

  int32_t CountParags() const {
    return pdfium::CollectionSize<int32_t>(m_ParagPtrArray);
  }
  CFDE_TxtEdtParag* GetParag(int32_t nParagIndex) const {
    return m_ParagPtrArray[nParagIndex].get();
  }
  CFDE_TxtEdtBuf* GetTextBuf() const { return m_pTxtBuf.get(); }
  int32_t GetTextBufLength() const { return m_pTxtBuf->GetTextLength() - 1; }
  CFX_TxtBreak* GetTextBreak() const { return m_pTextBreak.get(); }
  int32_t GetLineCount() const { return m_nLineCount; }
  int32_t GetPageLineCount() const { return m_nPageLineCount; }

  int32_t Line2Parag(int32_t nStartParag,
                     int32_t nStartLineofParag,
                     int32_t nLineIndex,
                     int32_t& nStartLine) const;
  wchar_t GetAliasChar() const { return m_wcAliasChar; }

  bool IsSelect() const { return !m_SelRangePtrArr.empty(); }
  void Inner_DeleteRange(int32_t nStart, int32_t nCount);
  void Inner_Insert(int32_t nStart, const wchar_t* lpText, int32_t nLength);
  const FDE_TXTEDTPARAMS* GetParams() const { return &m_Param; }
  FDE_TXTEDT_TEXTCHANGE_INFO* GetChangeInfo() { return &m_ChangeInfo; }

  void UpdateChangeInfoInsert(int32_t type, const CFX_WideString& insertValue) {
    m_ChangeInfo.nChangeType = type;
    m_ChangeInfo.wsInsert = insertValue;
  }

  void UpdateChangeInfoDelete(int32_t type, const CFX_WideString& deleteValue) {
    m_ChangeInfo.nChangeType = type;
    m_ChangeInfo.wsDelete = deleteValue;
  }

 private:
  struct FDE_TXTEDTSELRANGE {
    int32_t nStart;
    int32_t nCount;
  };

  struct FDE_TXTEDTPARAGPOS {
    int32_t nParagIndex;
    int32_t nCharIndex;
  };

  CFX_WideString GetPreDeleteText(int32_t nIndex, int32_t nLength);
  CFX_WideString GetPreInsertText(int32_t nIndex,
                                  const wchar_t* lpText,
                                  int32_t nLength);
  CFX_WideString GetPreReplaceText(int32_t nIndex,
                                   int32_t nOriginLength,
                                   const wchar_t* lpText,
                                   int32_t nLength);

  void DeleteRange_DoRecord(int32_t nStart, int32_t nCount, bool bSel);
  void ResetEngine();
  void RebuildParagraphs();
  void RemoveAllParags() { m_ParagPtrArray.clear(); }
  void RemoveAllPages() { m_PagePtrArray.clear(); }
  void UpdateLineCounts();
  void UpdatePages();
  void UpdateTxtBreak();

  bool ReplaceParagEnd(wchar_t*& lpText, int32_t& nLength, bool bPreIsCR);
  void RecoverParagEnd(CFX_WideString& wsText) const;
  int32_t MovePage2Char(int32_t nIndex);
  void TextPos2ParagPos(int32_t nIndex, FDE_TXTEDTPARAGPOS& ParagPos) const;
  int32_t MoveForward(bool& bBefore);
  int32_t MoveBackward(bool& bBefore);
  bool MoveUp(CFX_PointF& ptCaret);
  bool MoveDown(CFX_PointF& ptCaret);
  bool MoveLineStart();
  bool MoveLineEnd();
  bool MoveHome();
  bool MoveEnd();
  bool IsFitArea(CFX_WideString& wsText);
  void UpdateCaretRect(int32_t nIndex, bool bBefore);
  void GetCaretRect(CFX_RectF& rtCaret,
                    int32_t nPageIndex,
                    int32_t nCaret,
                    bool bBefore);
  void UpdateCaretIndex(const CFX_PointF& ptCaret);

  void DeleteSelect();

  std::unique_ptr<CFDE_TxtEdtBuf> m_pTxtBuf;
  std::unique_ptr<CFX_TxtBreak> m_pTextBreak;
  FDE_TXTEDTPARAMS m_Param;
  std::vector<std::unique_ptr<CFDE_TxtEdtPage>> m_PagePtrArray;
  std::vector<std::unique_ptr<CFDE_TxtEdtParag>> m_ParagPtrArray;
  std::vector<std::unique_ptr<FDE_TXTEDTSELRANGE>> m_SelRangePtrArr;
  int32_t m_nPageLineCount;
  int32_t m_nLineCount;
  int32_t m_nAnchorPos;
  float m_fCaretPosReserve;
  int32_t m_nCaret;
  bool m_bBefore;
  int32_t m_nCaretPage;
  CFX_RectF m_rtCaret;
  bool m_bLock;
  int32_t m_nLimit;
  wchar_t m_wcAliasChar;
  int32_t m_nFirstLineEnd;
  bool m_bAutoLineEnd;
  wchar_t m_wLineEnd;
  FDE_TXTEDT_TEXTCHANGE_INFO m_ChangeInfo;
};

#endif  // XFA_FDE_CFDE_TXTEDTENGINE_H_
