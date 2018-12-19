// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_XFA_H_
#define FXJS_XFA_CJX_XFA_H_

#include "fxjs/xfa/cjx_model.h"
#include "fxjs/xfa/jse_define.h"

class CXFA_Xfa;

class CJX_Xfa final : public CJX_Model {
 public:
  explicit CJX_Xfa(CXFA_Xfa* node);
  ~CJX_Xfa() override;

  JSE_PROP(thisValue); /* this */
};

#endif  // FXJS_XFA_CJX_XFA_H_
