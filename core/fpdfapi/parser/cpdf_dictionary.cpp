// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_dictionary.h"

#include <set>
#include <utility>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "third_party/base/logging.h"
#include "third_party/base/stl_util.h"

CPDF_Dictionary::CPDF_Dictionary()
    : CPDF_Dictionary(CFX_WeakPtr<CFX_ByteStringPool>()) {}

CPDF_Dictionary::CPDF_Dictionary(const CFX_WeakPtr<CFX_ByteStringPool>& pPool)
    : m_pPool(pPool) {}

CPDF_Dictionary::~CPDF_Dictionary() {
  // Mark the object as deleted so that it will not be deleted again
  // in case of cyclic references.
  m_ObjNum = kInvalidObjNum;
  for (const auto& it : m_Map) {
    if (it.second)
      it.second->Release();
  }
}

CPDF_Object::Type CPDF_Dictionary::GetType() const {
  return DICTIONARY;
}

CPDF_Dictionary* CPDF_Dictionary::GetDict() const {
  // The method should be made non-const if we want to not be const.
  // See bug #234.
  return const_cast<CPDF_Dictionary*>(this);
}

bool CPDF_Dictionary::IsDictionary() const {
  return true;
}

CPDF_Dictionary* CPDF_Dictionary::AsDictionary() {
  return this;
}

const CPDF_Dictionary* CPDF_Dictionary::AsDictionary() const {
  return this;
}

CPDF_Object* CPDF_Dictionary::Clone() const {
  return CloneObjectNonCyclic(false);
}

CPDF_Object* CPDF_Dictionary::CloneNonCyclic(
    bool bDirect,
    std::set<const CPDF_Object*>* pVisited) const {
  pVisited->insert(this);
  CPDF_Dictionary* pCopy = new CPDF_Dictionary(m_pPool);
  for (const auto& it : *this) {
    CPDF_Object* value = it.second;
    if (!pdfium::ContainsKey(*pVisited, value)) {
      pCopy->m_Map.insert(
          std::make_pair(it.first, value->CloneNonCyclic(bDirect, pVisited)));
    }
  }
  return pCopy;
}

CPDF_Object* CPDF_Dictionary::GetObjectFor(const CFX_ByteString& key) const {
  auto it = m_Map.find(key);
  return it != m_Map.end() ? it->second : nullptr;
}

CPDF_Object* CPDF_Dictionary::GetDirectObjectFor(
    const CFX_ByteString& key) const {
  CPDF_Object* p = GetObjectFor(key);
  return p ? p->GetDirect() : nullptr;
}

CFX_ByteString CPDF_Dictionary::GetStringFor(const CFX_ByteString& key) const {
  CPDF_Object* p = GetObjectFor(key);
  return p ? p->GetString() : CFX_ByteString();
}

CFX_WideString CPDF_Dictionary::GetUnicodeTextFor(
    const CFX_ByteString& key) const {
  CPDF_Object* p = GetObjectFor(key);
  if (CPDF_Reference* pRef = ToReference(p))
    p = pRef->GetDirect();
  return p ? p->GetUnicodeText() : CFX_WideString();
}

CFX_ByteString CPDF_Dictionary::GetStringFor(const CFX_ByteString& key,
                                             const CFX_ByteString& def) const {
  CPDF_Object* p = GetObjectFor(key);
  return p ? p->GetString() : CFX_ByteString(def);
}

int CPDF_Dictionary::GetIntegerFor(const CFX_ByteString& key) const {
  CPDF_Object* p = GetObjectFor(key);
  return p ? p->GetInteger() : 0;
}

int CPDF_Dictionary::GetIntegerFor(const CFX_ByteString& key, int def) const {
  CPDF_Object* p = GetObjectFor(key);
  return p ? p->GetInteger() : def;
}

FX_FLOAT CPDF_Dictionary::GetNumberFor(const CFX_ByteString& key) const {
  CPDF_Object* p = GetObjectFor(key);
  return p ? p->GetNumber() : 0;
}

bool CPDF_Dictionary::GetBooleanFor(const CFX_ByteString& key,
                                    bool bDefault) const {
  CPDF_Object* p = GetObjectFor(key);
  return ToBoolean(p) ? p->GetInteger() != 0 : bDefault;
}

CPDF_Dictionary* CPDF_Dictionary::GetDictFor(const CFX_ByteString& key) const {
  CPDF_Object* p = GetDirectObjectFor(key);
  if (!p)
    return nullptr;
  if (CPDF_Dictionary* pDict = p->AsDictionary())
    return pDict;
  if (CPDF_Stream* pStream = p->AsStream())
    return pStream->GetDict();
  return nullptr;
}

CPDF_Array* CPDF_Dictionary::GetArrayFor(const CFX_ByteString& key) const {
  return ToArray(GetDirectObjectFor(key));
}

CPDF_Stream* CPDF_Dictionary::GetStreamFor(const CFX_ByteString& key) const {
  return ToStream(GetDirectObjectFor(key));
}

CFX_FloatRect CPDF_Dictionary::GetRectFor(const CFX_ByteString& key) const {
  CFX_FloatRect rect;
  CPDF_Array* pArray = GetArrayFor(key);
  if (pArray)
    rect = pArray->GetRect();
  return rect;
}

CFX_Matrix CPDF_Dictionary::GetMatrixFor(const CFX_ByteString& key) const {
  CFX_Matrix matrix;
  CPDF_Array* pArray = GetArrayFor(key);
  if (pArray)
    matrix = pArray->GetMatrix();
  return matrix;
}

bool CPDF_Dictionary::KeyExist(const CFX_ByteString& key) const {
  return pdfium::ContainsKey(m_Map, key);
}

bool CPDF_Dictionary::IsSignatureDict() const {
  CPDF_Object* pType = GetDirectObjectFor("Type");
  if (!pType)
    pType = GetDirectObjectFor("FT");
  return pType && pType->GetString() == "Sig";
}

void CPDF_Dictionary::SetFor(const CFX_ByteString& key, CPDF_Object* pObj) {
  CHECK(!pObj || pObj->GetObjNum() == 0);
  auto it = m_Map.find(key);
  if (it == m_Map.end()) {
    if (pObj)
      m_Map.insert(std::make_pair(MaybeIntern(key), pObj));
    return;
  }

  if (it->second == pObj)
    return;
  it->second->Release();

  if (pObj)
    it->second = pObj;
  else
    m_Map.erase(it);
}

void CPDF_Dictionary::RemoveFor(const CFX_ByteString& key) {
  auto it = m_Map.find(key);
  if (it == m_Map.end())
    return;

  it->second->Release();
  m_Map.erase(it);
}

void CPDF_Dictionary::ReplaceKey(const CFX_ByteString& oldkey,
                                 const CFX_ByteString& newkey) {
  auto old_it = m_Map.find(oldkey);
  if (old_it == m_Map.end())
    return;

  auto new_it = m_Map.find(newkey);
  if (new_it == old_it)
    return;

  if (new_it != m_Map.end()) {
    new_it->second->Release();
    new_it->second = old_it->second;
  } else {
    m_Map.insert(std::make_pair(MaybeIntern(newkey), old_it->second));
  }
  m_Map.erase(old_it);
}

void CPDF_Dictionary::SetIntegerFor(const CFX_ByteString& key, int i) {
  SetFor(key, new CPDF_Number(i));
}

void CPDF_Dictionary::SetNameFor(const CFX_ByteString& key,
                                 const CFX_ByteString& name) {
  SetFor(key, new CPDF_Name(MaybeIntern(name)));
}

void CPDF_Dictionary::SetStringFor(const CFX_ByteString& key,
                                   const CFX_ByteString& str) {
  SetFor(key, new CPDF_String(MaybeIntern(str), FALSE));
}

void CPDF_Dictionary::SetReferenceFor(const CFX_ByteString& key,
                                      CPDF_IndirectObjectHolder* pDoc,
                                      uint32_t objnum) {
  SetFor(key, new CPDF_Reference(pDoc, objnum));
}

void CPDF_Dictionary::SetNumberFor(const CFX_ByteString& key, FX_FLOAT f) {
  SetFor(key, new CPDF_Number(f));
}

void CPDF_Dictionary::SetBooleanFor(const CFX_ByteString& key, bool bValue) {
  SetFor(key, new CPDF_Boolean(bValue));
}

void CPDF_Dictionary::SetRectFor(const CFX_ByteString& key,
                                 const CFX_FloatRect& rect) {
  CPDF_Array* pArray = new CPDF_Array;
  pArray->AddNumber(rect.left);
  pArray->AddNumber(rect.bottom);
  pArray->AddNumber(rect.right);
  pArray->AddNumber(rect.top);
  SetFor(key, pArray);
}

void CPDF_Dictionary::SetMatrixFor(const CFX_ByteString& key,
                                   const CFX_Matrix& matrix) {
  CPDF_Array* pArray = new CPDF_Array;
  pArray->AddNumber(matrix.a);
  pArray->AddNumber(matrix.b);
  pArray->AddNumber(matrix.c);
  pArray->AddNumber(matrix.d);
  pArray->AddNumber(matrix.e);
  pArray->AddNumber(matrix.f);
  SetFor(key, pArray);
}

CFX_ByteString CPDF_Dictionary::MaybeIntern(const CFX_ByteString& str) {
  return m_pPool ? m_pPool->Intern(str) : str;
}
