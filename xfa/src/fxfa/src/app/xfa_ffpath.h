// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_SRC_FXFA_SRC_APP_XFA_FFPATH_H_
#define XFA_SRC_FXFA_SRC_APP_XFA_FFPATH_H_

class CXFA_FFLine : public CXFA_FFDraw {
 public:
  CXFA_FFLine(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  virtual ~CXFA_FFLine();
  virtual void RenderWidget(CFX_Graphics* pGS,
                            CFX_Matrix* pMatrix = NULL,
                            FX_DWORD dwStatus = 0,
                            int32_t iRotate = 0);

 private:
  void GetRectFromHand(CFX_RectF& rect, int32_t iHand, FX_FLOAT fLineWidth);
};
class CXFA_FFArc : public CXFA_FFDraw {
 public:
  CXFA_FFArc(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  virtual ~CXFA_FFArc();
  virtual void RenderWidget(CFX_Graphics* pGS,
                            CFX_Matrix* pMatrix = NULL,
                            FX_DWORD dwStatus = 0,
                            int32_t iRotate = 0);
};
class CXFA_FFRectangle : public CXFA_FFDraw {
 public:
  CXFA_FFRectangle(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  virtual ~CXFA_FFRectangle();
  virtual void RenderWidget(CFX_Graphics* pGS,
                            CFX_Matrix* pMatrix = NULL,
                            FX_DWORD dwStatus = 0,
                            int32_t iRotate = 0);
};

#endif  // XFA_SRC_FXFA_SRC_APP_XFA_FFPATH_H_
