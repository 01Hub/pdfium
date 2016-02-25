// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_SRC_FXBARCODE_ONED_BC_ONEDCODABARREADER_H_
#define XFA_SRC_FXBARCODE_ONED_BC_ONEDCODABARREADER_H_

#include "core/include/fxcrt/fx_basic.h"
#include "xfa/src/fxbarcode/oned/BC_OneDReader.h"

class CBC_CommonBitArray;
class CBC_OneDReader;

class CBC_OnedCodaBarReader : public CBC_OneDReader {
 public:
  CBC_OnedCodaBarReader();
  virtual ~CBC_OnedCodaBarReader();
  CFX_ByteString DecodeRow(int32_t rowNumber,
                           CBC_CommonBitArray* row,
                           int32_t hints,
                           int32_t& e);
  CFX_Int32Array* FindAsteriskPattern(CBC_CommonBitArray* row, int32_t& e);
  FX_BOOL ArrayContains(const FX_CHAR array[], FX_CHAR key);
  FX_CHAR ToNarrowWidePattern(CFX_Int32Array* counter);
  static const FX_CHAR* ALPHABET_STRING;

  const static int32_t CHARACTER_ENCODINGS[22];

  const static int32_t minCharacterLength;

  const static FX_CHAR STARTEND_ENCODING[8];
};

#endif  // XFA_SRC_FXBARCODE_ONED_BC_ONEDCODABARREADER_H_
