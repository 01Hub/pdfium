// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_keyusage.h"

namespace {

const XFA_Attribute kAttributeData[] = {XFA_Attribute::Id,
                                        XFA_Attribute::Use,
                                        XFA_Attribute::NonRepudiation,
                                        XFA_Attribute::EncipherOnly,
                                        XFA_Attribute::Type,
                                        XFA_Attribute::DigitalSignature,
                                        XFA_Attribute::CrlSign,
                                        XFA_Attribute::KeyAgreement,
                                        XFA_Attribute::KeyEncipherment,
                                        XFA_Attribute::Usehref,
                                        XFA_Attribute::DataEncipherment,
                                        XFA_Attribute::KeyCertSign,
                                        XFA_Attribute::DecipherOnly,
                                        XFA_Attribute::Unknown};

constexpr wchar_t kName[] = L"keyUsage";

}  // namespace

CXFA_KeyUsage::CXFA_KeyUsage(CXFA_Document* doc, XFA_XDPPACKET packet)
    : CXFA_Node(doc,
                packet,
                (XFA_XDPPACKET_Template | XFA_XDPPACKET_Form),
                XFA_ObjectType::Node,
                XFA_Element::KeyUsage,
                nullptr,
                kAttributeData,
                kName) {}

CXFA_KeyUsage::~CXFA_KeyUsage() {}
