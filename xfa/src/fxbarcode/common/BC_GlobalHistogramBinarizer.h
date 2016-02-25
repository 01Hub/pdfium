// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_SRC_FXBARCODE_COMMON_BC_GLOBALHISTOGRAMBINARIZER_H_
#define XFA_SRC_FXBARCODE_COMMON_BC_GLOBALHISTOGRAMBINARIZER_H_

#include "core/include/fxcrt/fx_basic.h"
#include "xfa/src/fxbarcode/BC_Binarizer.h"

class CBC_CommonBinarizer;
class CBC_CommonBitArray;
class CBC_CommonBitMatrix;
class CBC_LuminanceSource;

class CBC_GlobalHistogramBinarizer : public CBC_Binarizer {
 public:
  CBC_GlobalHistogramBinarizer(CBC_LuminanceSource* source);
  virtual ~CBC_GlobalHistogramBinarizer();

  void InitArrays(int32_t luminanceSize);
  CBC_CommonBitMatrix* GetBlackMatrix(int32_t& e);
  CBC_CommonBitArray* GetBlackRow(int32_t y,
                                  CBC_CommonBitArray* row,
                                  int32_t& e);
  static int32_t EstimateBlackPoint(CFX_Int32Array& buckets, int32_t& e);

 private:
  CFX_ByteArray m_luminance;
  CFX_Int32Array m_buckets;
};

#endif  // XFA_SRC_FXBARCODE_COMMON_BC_GLOBALHISTOGRAMBINARIZER_H_
