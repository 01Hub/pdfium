// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_exdata.h"

namespace {

const XFA_Attribute kAttributeData[] = {
    XFA_Attribute::Id,          XFA_Attribute::Name,
    XFA_Attribute::Rid,         XFA_Attribute::Use,
    XFA_Attribute::ContentType, XFA_Attribute::TransferEncoding,
    XFA_Attribute::Usehref,     XFA_Attribute::MaxLength,
    XFA_Attribute::Href,        XFA_Attribute::Unknown};

constexpr wchar_t kName[] = L"exData";

}  // namespace

CXFA_ExData::CXFA_ExData(CXFA_Document* doc, XFA_XDPPACKET packet)
    : CXFA_Node(doc,
                packet,
                (XFA_XDPPACKET_Template | XFA_XDPPACKET_Form),
                XFA_ObjectType::ContentNode,
                XFA_Element::ExData,
                nullptr,
                kAttributeData,
                kName) {}

CXFA_ExData::~CXFA_ExData() {}
