// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_flatten.h"

#include <algorithm>

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfdoc/cpdf_annot.h"
#include "fpdfsdk/fsdk_define.h"

typedef CFX_ArrayTemplate<CPDF_Dictionary*> CPDF_ObjectArray;
typedef CFX_ArrayTemplate<CFX_FloatRect> CPDF_RectArray;

enum FPDF_TYPE { MAX, MIN };
enum FPDF_VALUE { TOP, LEFT, RIGHT, BOTTOM };

FX_BOOL IsValiableRect(CFX_FloatRect rect, CFX_FloatRect rcPage) {
  if (rect.left - rect.right > 0.000001f || rect.bottom - rect.top > 0.000001f)
    return FALSE;

  if (rect.left == 0.0f && rect.top == 0.0f && rect.right == 0.0f &&
      rect.bottom == 0.0f)
    return FALSE;

  if (!rcPage.IsEmpty()) {
    if (rect.left - rcPage.left < -10.000001f ||
        rect.right - rcPage.right > 10.000001f ||
        rect.top - rcPage.top > 10.000001f ||
        rect.bottom - rcPage.bottom < -10.000001f)
      return FALSE;
  }

  return TRUE;
}

void GetContentsRect(CPDF_Document* pDoc,
                     CPDF_Dictionary* pDict,
                     CPDF_RectArray* pRectArray) {
  std::unique_ptr<CPDF_Page> pPDFPage(new CPDF_Page(pDoc, pDict, false));
  pPDFPage->ParseContent();

  for (const auto& pPageObject : *pPDFPage->GetPageObjectList()) {
    CFX_FloatRect rc;
    rc.left = pPageObject->m_Left;
    rc.right = pPageObject->m_Right;
    rc.bottom = pPageObject->m_Bottom;
    rc.top = pPageObject->m_Top;
    if (IsValiableRect(rc, pDict->GetRectFor("MediaBox")))
      pRectArray->Add(rc);
  }
}

void ParserStream(CPDF_Dictionary* pPageDic,
                  CPDF_Dictionary* pStream,
                  CPDF_RectArray* pRectArray,
                  CPDF_ObjectArray* pObjectArray) {
  if (!pStream)
    return;
  CFX_FloatRect rect;
  if (pStream->KeyExist("Rect"))
    rect = pStream->GetRectFor("Rect");
  else if (pStream->KeyExist("BBox"))
    rect = pStream->GetRectFor("BBox");

  if (IsValiableRect(rect, pPageDic->GetRectFor("MediaBox")))
    pRectArray->Add(rect);

  pObjectArray->Add(pStream);
}

int ParserAnnots(CPDF_Document* pSourceDoc,
                 CPDF_Dictionary* pPageDic,
                 CPDF_RectArray* pRectArray,
                 CPDF_ObjectArray* pObjectArray,
                 int nUsage) {
  if (!pSourceDoc || !pPageDic)
    return FLATTEN_FAIL;

  GetContentsRect(pSourceDoc, pPageDic, pRectArray);
  CPDF_Array* pAnnots = pPageDic->GetArrayFor("Annots");
  if (!pAnnots)
    return FLATTEN_NOTHINGTODO;

  uint32_t dwSize = pAnnots->GetCount();
  for (int i = 0; i < (int)dwSize; i++) {
    CPDF_Dictionary* pAnnotDic = ToDictionary(pAnnots->GetDirectObjectAt(i));
    if (!pAnnotDic)
      continue;

    CFX_ByteString sSubtype = pAnnotDic->GetStringFor("Subtype");
    if (sSubtype == "Popup")
      continue;

    int nAnnotFlag = pAnnotDic->GetIntegerFor("F");
    if (nAnnotFlag & ANNOTFLAG_HIDDEN)
      continue;

    if (nUsage == FLAT_NORMALDISPLAY) {
      if (nAnnotFlag & ANNOTFLAG_INVISIBLE)
        continue;

      ParserStream(pPageDic, pAnnotDic, pRectArray, pObjectArray);
    } else {
      if (nAnnotFlag & ANNOTFLAG_PRINT)
        ParserStream(pPageDic, pAnnotDic, pRectArray, pObjectArray);
    }
  }
  return FLATTEN_SUCCESS;
}

FX_FLOAT GetMinMaxValue(CPDF_RectArray& array,
                        FPDF_TYPE type,
                        FPDF_VALUE value) {
  int nRects = array.GetSize();
  FX_FLOAT fRet = 0.0f;

  if (nRects <= 0)
    return 0.0f;

  FX_FLOAT* pArray = new FX_FLOAT[nRects];
  switch (value) {
    case LEFT: {
      for (int i = 0; i < nRects; i++)
        pArray[i] = CFX_FloatRect(array.GetAt(i)).left;

      break;
    }
    case TOP: {
      for (int i = 0; i < nRects; i++)
        pArray[i] = CFX_FloatRect(array.GetAt(i)).top;

      break;
    }
    case RIGHT: {
      for (int i = 0; i < nRects; i++)
        pArray[i] = CFX_FloatRect(array.GetAt(i)).right;

      break;
    }
    case BOTTOM: {
      for (int i = 0; i < nRects; i++)
        pArray[i] = CFX_FloatRect(array.GetAt(i)).bottom;

      break;
    }
    default:
      // Not reachable.
      return 0.0f;
  }
  fRet = pArray[0];
  if (type == MAX) {
    for (int i = 1; i < nRects; i++)
      if (fRet <= pArray[i])
        fRet = pArray[i];
  } else {
    for (int i = 1; i < nRects; i++)
      if (fRet >= pArray[i])
        fRet = pArray[i];
  }
  delete[] pArray;
  return fRet;
}

CFX_FloatRect CalculateRect(CPDF_RectArray* pRectArray) {
  CFX_FloatRect rcRet;

  rcRet.left = GetMinMaxValue(*pRectArray, MIN, LEFT);
  rcRet.top = GetMinMaxValue(*pRectArray, MAX, TOP);
  rcRet.right = GetMinMaxValue(*pRectArray, MAX, RIGHT);
  rcRet.bottom = GetMinMaxValue(*pRectArray, MIN, BOTTOM);

  return rcRet;
}

void SetPageContents(CFX_ByteString key,
                     CPDF_Dictionary* pPage,
                     CPDF_Document* pDocument) {
  CPDF_Object* pContentsObj = pPage->GetStreamFor("Contents");
  if (!pContentsObj) {
    pContentsObj = pPage->GetArrayFor("Contents");
  }

  if (!pContentsObj) {
    // Create a new contents dictionary
    if (!key.IsEmpty()) {
      CPDF_Stream* pNewContents = new CPDF_Stream(
          nullptr, 0, new CPDF_Dictionary(pDocument->GetByteStringPool()));
      CFX_ByteString sStream;
      sStream.Format("q 1 0 0 1 0 0 cm /%s Do Q", key.c_str());
      pNewContents->SetData(sStream.raw_str(), sStream.GetLength());
      pPage->SetReferenceFor("Contents", pDocument,
                             pDocument->AddIndirectObject(pNewContents));
    }
    return;
  }

  CPDF_Array* pContentsArray = nullptr;
  switch (pContentsObj->GetType()) {
    case CPDF_Object::STREAM: {
      pContentsArray = new CPDF_Array;
      CPDF_Stream* pContents = pContentsObj->AsStream();
      uint32_t dwObjNum = pDocument->AddIndirectObject(pContents);
      CPDF_StreamAcc acc;
      acc.LoadAllData(pContents);
      CFX_ByteString sStream = "q\n";
      CFX_ByteString sBody =
          CFX_ByteString((const FX_CHAR*)acc.GetData(), acc.GetSize());
      sStream = sStream + sBody + "\nQ";
      pContents->SetData(sStream.raw_str(), sStream.GetLength());
      pContentsArray->AddReference(pDocument, dwObjNum);
      break;
    }

    case CPDF_Object::ARRAY: {
      pContentsArray = pContentsObj->AsArray();
      break;
    }
    default:
      break;
  }

  if (!pContentsArray)
    return;

  pPage->SetReferenceFor("Contents", pDocument,
                         pDocument->AddIndirectObject(pContentsArray));

  if (!key.IsEmpty()) {
    CPDF_Stream* pNewContents = new CPDF_Stream(
        nullptr, 0, new CPDF_Dictionary(pDocument->GetByteStringPool()));
    CFX_ByteString sStream;
    sStream.Format("q 1 0 0 1 0 0 cm /%s Do Q", key.c_str());
    pNewContents->SetData(sStream.raw_str(), sStream.GetLength());
    pContentsArray->AddReference(pDocument,
                                 pDocument->AddIndirectObject(pNewContents));
  }
}

CFX_Matrix GetMatrix(CFX_FloatRect rcAnnot,
                     CFX_FloatRect rcStream,
                     const CFX_Matrix& matrix) {
  if (rcStream.IsEmpty())
    return CFX_Matrix();

  matrix.TransformRect(rcStream);
  rcStream.Normalize();

  FX_FLOAT a = rcAnnot.Width() / rcStream.Width();
  FX_FLOAT d = rcAnnot.Height() / rcStream.Height();

  FX_FLOAT e = rcAnnot.left - rcStream.left * a;
  FX_FLOAT f = rcAnnot.bottom - rcStream.bottom * d;
  return CFX_Matrix(a, 0, 0, d, e, f);
}

void GetOffset(FX_FLOAT& fa,
               FX_FLOAT& fd,
               FX_FLOAT& fe,
               FX_FLOAT& ff,
               CFX_FloatRect rcAnnot,
               CFX_FloatRect rcStream,
               const CFX_Matrix& matrix) {
  FX_FLOAT fStreamWidth = 0.0f;
  FX_FLOAT fStreamHeight = 0.0f;

  if (matrix.a != 0 && matrix.d != 0) {
    fStreamWidth = rcStream.right - rcStream.left;
    fStreamHeight = rcStream.top - rcStream.bottom;
  } else {
    fStreamWidth = rcStream.top - rcStream.bottom;
    fStreamHeight = rcStream.right - rcStream.left;
  }

  FX_FLOAT x1 =
      matrix.a * rcStream.left + matrix.c * rcStream.bottom + matrix.e;
  FX_FLOAT y1 =
      matrix.b * rcStream.left + matrix.d * rcStream.bottom + matrix.f;
  FX_FLOAT x2 = matrix.a * rcStream.left + matrix.c * rcStream.top + matrix.e;
  FX_FLOAT y2 = matrix.b * rcStream.left + matrix.d * rcStream.top + matrix.f;
  FX_FLOAT x3 =
      matrix.a * rcStream.right + matrix.c * rcStream.bottom + matrix.e;
  FX_FLOAT y3 =
      matrix.b * rcStream.right + matrix.d * rcStream.bottom + matrix.f;
  FX_FLOAT x4 = matrix.a * rcStream.right + matrix.c * rcStream.top + matrix.e;
  FX_FLOAT y4 = matrix.b * rcStream.right + matrix.d * rcStream.top + matrix.f;

  FX_FLOAT left = std::min(std::min(x1, x2), std::min(x3, x4));
  FX_FLOAT bottom = std::min(std::min(y1, y2), std::min(y3, y4));

  fa = (rcAnnot.right - rcAnnot.left) / fStreamWidth;
  fd = (rcAnnot.top - rcAnnot.bottom) / fStreamHeight;
  fe = rcAnnot.left - left * fa;
  ff = rcAnnot.bottom - bottom * fd;
}

DLLEXPORT int STDCALL FPDFPage_Flatten(FPDF_PAGE page, int nFlag) {
  CPDF_Page* pPage = CPDFPageFromFPDFPage(page);
  if (!page) {
    return FLATTEN_FAIL;
  }

  CPDF_Document* pDocument = pPage->m_pDocument;
  CPDF_Dictionary* pPageDict = pPage->m_pFormDict;

  if (!pDocument || !pPageDict) {
    return FLATTEN_FAIL;
  }

  CPDF_ObjectArray ObjectArray;
  CPDF_RectArray RectArray;

  int iRet = FLATTEN_FAIL;
  iRet = ParserAnnots(pDocument, pPageDict, &RectArray, &ObjectArray, nFlag);
  if (iRet == FLATTEN_NOTHINGTODO || iRet == FLATTEN_FAIL)
    return iRet;

  CFX_FloatRect rcOriginalCB;
  CFX_FloatRect rcMerger = CalculateRect(&RectArray);
  CFX_FloatRect rcOriginalMB = pPageDict->GetRectFor("MediaBox");

  if (pPageDict->KeyExist("CropBox"))
    rcOriginalMB = pPageDict->GetRectFor("CropBox");

  if (rcOriginalMB.IsEmpty()) {
    rcOriginalMB = CFX_FloatRect(0.0f, 0.0f, 612.0f, 792.0f);
  }

  rcMerger.left =
      rcMerger.left < rcOriginalMB.left ? rcOriginalMB.left : rcMerger.left;
  rcMerger.right =
      rcMerger.right > rcOriginalMB.right ? rcOriginalMB.right : rcMerger.right;
  rcMerger.top =
      rcMerger.top > rcOriginalMB.top ? rcOriginalMB.top : rcMerger.top;
  rcMerger.bottom = rcMerger.bottom < rcOriginalMB.bottom ? rcOriginalMB.bottom
                                                          : rcMerger.bottom;

  if (pPageDict->KeyExist("ArtBox"))
    rcOriginalCB = pPageDict->GetRectFor("ArtBox");
  else
    rcOriginalCB = rcOriginalMB;

  if (!rcOriginalMB.IsEmpty()) {
    CPDF_Array* pMediaBox = new CPDF_Array();
    pMediaBox->Add(new CPDF_Number(rcOriginalMB.left));
    pMediaBox->Add(new CPDF_Number(rcOriginalMB.bottom));
    pMediaBox->Add(new CPDF_Number(rcOriginalMB.right));
    pMediaBox->Add(new CPDF_Number(rcOriginalMB.top));
    pPageDict->SetFor("MediaBox", pMediaBox);
  }

  if (!rcOriginalCB.IsEmpty()) {
    CPDF_Array* pCropBox = new CPDF_Array();
    pCropBox->Add(new CPDF_Number(rcOriginalCB.left));
    pCropBox->Add(new CPDF_Number(rcOriginalCB.bottom));
    pCropBox->Add(new CPDF_Number(rcOriginalCB.right));
    pCropBox->Add(new CPDF_Number(rcOriginalCB.top));
    pPageDict->SetFor("ArtBox", pCropBox);
  }

  CPDF_Dictionary* pRes = pPageDict->GetDictFor("Resources");
  if (!pRes) {
    pRes = new CPDF_Dictionary(pDocument->GetByteStringPool());
    pPageDict->SetFor("Resources", pRes);
  }

  CPDF_Stream* pNewXObject = new CPDF_Stream(
      nullptr, 0, new CPDF_Dictionary(pDocument->GetByteStringPool()));

  uint32_t dwObjNum = pDocument->AddIndirectObject(pNewXObject);
  CPDF_Dictionary* pPageXObject = pRes->GetDictFor("XObject");
  if (!pPageXObject) {
    pPageXObject = new CPDF_Dictionary(pDocument->GetByteStringPool());
    pRes->SetFor("XObject", pPageXObject);
  }

  CFX_ByteString key = "";
  int nStreams = ObjectArray.GetSize();

  if (nStreams > 0) {
    for (int iKey = 0; /*iKey < 100*/; iKey++) {
      char sExtend[5] = {};
      FXSYS_itoa(iKey, sExtend, 10);
      key = CFX_ByteString("FFT") + CFX_ByteString(sExtend);
      if (!pPageXObject->KeyExist(key))
        break;
    }
  }

  SetPageContents(key, pPageDict, pDocument);

  CPDF_Dictionary* pNewXORes = nullptr;

  if (!key.IsEmpty()) {
    pPageXObject->SetReferenceFor(key, pDocument, dwObjNum);
    CPDF_Dictionary* pNewOXbjectDic = pNewXObject->GetDict();
    pNewXORes = new CPDF_Dictionary(pDocument->GetByteStringPool());
    pNewOXbjectDic->SetFor("Resources", pNewXORes);
    pNewOXbjectDic->SetNameFor("Type", "XObject");
    pNewOXbjectDic->SetNameFor("Subtype", "Form");
    pNewOXbjectDic->SetIntegerFor("FormType", 1);
    pNewOXbjectDic->SetNameFor("Name", "FRM");
    CFX_FloatRect rcBBox = pPageDict->GetRectFor("ArtBox");
    pNewOXbjectDic->SetRectFor("BBox", rcBBox);
  }

  for (int i = 0; i < nStreams; i++) {
    CPDF_Dictionary* pAnnotDic = ObjectArray.GetAt(i);
    if (!pAnnotDic)
      continue;

    CFX_FloatRect rcAnnot = pAnnotDic->GetRectFor("Rect");
    rcAnnot.Normalize();

    CFX_ByteString sAnnotState = pAnnotDic->GetStringFor("AS");
    CPDF_Dictionary* pAnnotAP = pAnnotDic->GetDictFor("AP");
    if (!pAnnotAP)
      continue;

    CPDF_Stream* pAPStream = pAnnotAP->GetStreamFor("N");
    if (!pAPStream) {
      CPDF_Dictionary* pAPDic = pAnnotAP->GetDictFor("N");
      if (!pAPDic)
        continue;

      if (!sAnnotState.IsEmpty()) {
        pAPStream = pAPDic->GetStreamFor(sAnnotState);
      } else {
        auto it = pAPDic->begin();
        if (it != pAPDic->end()) {
          CPDF_Object* pFirstObj = it->second;
          if (pFirstObj) {
            if (pFirstObj->IsReference())
              pFirstObj = pFirstObj->GetDirect();
            if (!pFirstObj->IsStream())
              continue;
            pAPStream = pFirstObj->AsStream();
          }
        }
      }
    }
    if (!pAPStream)
      continue;

    CPDF_Dictionary* pAPDic = pAPStream->GetDict();
    CFX_Matrix matrix = pAPDic->GetMatrixFor("Matrix");

    CFX_FloatRect rcStream;
    if (pAPDic->KeyExist("Rect"))
      rcStream = pAPDic->GetRectFor("Rect");
    else if (pAPDic->KeyExist("BBox"))
      rcStream = pAPDic->GetRectFor("BBox");

    if (rcStream.IsEmpty())
      continue;

    CPDF_Object* pObj = pAPStream;

    if (pObj) {
      CPDF_Dictionary* pObjDic = pObj->GetDict();
      if (pObjDic) {
        pObjDic->SetNameFor("Type", "XObject");
        pObjDic->SetNameFor("Subtype", "Form");
      }
    }

    CPDF_Dictionary* pXObject = pNewXORes->GetDictFor("XObject");
    if (!pXObject) {
      pXObject = new CPDF_Dictionary(pDocument->GetByteStringPool());
      pNewXORes->SetFor("XObject", pXObject);
    }

    CFX_ByteString sFormName;
    sFormName.Format("F%d", i);
    pXObject->SetReferenceFor(sFormName, pDocument,
                              pDocument->AddIndirectObject(pObj));

    CPDF_StreamAcc acc;
    acc.LoadAllData(pNewXObject);

    const uint8_t* pData = acc.GetData();
    CFX_ByteString sStream(pData, acc.GetSize());

    if (matrix.IsIdentity()) {
      matrix.a = 1.0f;
      matrix.b = 0.0f;
      matrix.c = 0.0f;
      matrix.d = 1.0f;
      matrix.e = 0.0f;
      matrix.f = 0.0f;
    }

    CFX_ByteString sTemp;
    CFX_Matrix m = GetMatrix(rcAnnot, rcStream, matrix);
    sTemp.Format("q %f 0 0 %f %f %f cm /%s Do Q\n", m.a, m.d, m.e, m.f,
                 sFormName.c_str());
    sStream += sTemp;
    pNewXObject->SetData(sStream.raw_str(), sStream.GetLength());
  }
  pPageDict->RemoveFor("Annots");

  ObjectArray.RemoveAll();
  RectArray.RemoveAll();

  return FLATTEN_SUCCESS;
}
