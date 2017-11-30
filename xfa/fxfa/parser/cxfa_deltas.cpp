// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_deltas.h"

namespace {

constexpr wchar_t kName[] = L"deltas";

}  // namespace

CXFA_Deltas::CXFA_Deltas(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_Form,
                XFA_ObjectType::Object,
                XFA_Element::Deltas,
                nullptr,
                nullptr,
                kName) {}

CXFA_Deltas::~CXFA_Deltas() {}
