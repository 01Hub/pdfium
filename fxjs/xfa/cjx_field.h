// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_FIELD_H_
#define FXJS_XFA_CJX_FIELD_H_

#include "fxjs/xfa/cjx_container.h"
#include "fxjs/xfa/jse_define.h"

class CXFA_Field;

class CJX_Field final : public CJX_Container {
 public:
  explicit CJX_Field(CXFA_Field* field);
  ~CJX_Field() override;

  JSE_METHOD(addItem, CJX_Field);
  JSE_METHOD(boundItem, CJX_Field);
  JSE_METHOD(clearItems, CJX_Field);
  JSE_METHOD(deleteItem, CJX_Field);
  JSE_METHOD(execCalculate, CJX_Field);
  JSE_METHOD(execEvent, CJX_Field);
  JSE_METHOD(execInitialize, CJX_Field);
  JSE_METHOD(execValidate, CJX_Field);
  JSE_METHOD(getDisplayItem, CJX_Field);
  JSE_METHOD(getItemState, CJX_Field);
  JSE_METHOD(getSaveItem, CJX_Field);
  JSE_METHOD(setItemState, CJX_Field);

  JSE_PROP(defaultValue); /* {default} */
  JSE_PROP(borderColor);
  JSE_PROP(borderWidth);
  JSE_PROP(editValue);
  JSE_PROP(fillColor);
  JSE_PROP(fontColor);
  JSE_PROP(formatMessage);
  JSE_PROP(formattedValue);
  JSE_PROP(mandatory);
  JSE_PROP(mandatoryMessage);
  JSE_PROP(parentSubform);
  JSE_PROP(rawValue);
  JSE_PROP(selectedIndex);
  JSE_PROP(validationMessage);

 private:
  static const CJX_MethodSpec MethodSpecs[];
};

#endif  // FXJS_XFA_CJX_FIELD_H_
