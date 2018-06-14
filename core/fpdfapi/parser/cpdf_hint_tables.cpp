// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_hint_tables.h"

#include <limits>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_data_avail.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_linearized_header.h"
#include "core/fpdfapi/parser/cpdf_read_validator.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcrt/cfx_bitstream.h"
#include "core/fxcrt/fx_safe_types.h"
#include "third_party/base/numerics/safe_conversions.h"
#include "third_party/base/span.h"

namespace {

bool CanReadFromBitStream(const CFX_BitStream* hStream,
                          const FX_SAFE_UINT32& bits) {
  return bits.IsValid() && hStream->BitsRemaining() >= bits.ValueOrDie();
}

// Sanity check values from the page table header. The note in the PDF 1.7
// reference for Table F.3 says the valid range is only 0 through 32. Though 0
// is not useful either.
bool IsValidPageOffsetHintTableBitCount(uint32_t bits) {
  return bits > 0 && bits <= 32;
}

}  // namespace

CPDF_HintTables::CPDF_HintTables(CPDF_ReadValidator* pValidator,
                                 CPDF_LinearizedHeader* pLinearized)
    : m_pValidator(pValidator),
      m_pLinearized(pLinearized),
      m_nFirstPageSharedObjs(0),
      m_szFirstPageObjOffset(0) {
  ASSERT(m_pLinearized);
}

CPDF_HintTables::~CPDF_HintTables() {}

uint32_t CPDF_HintTables::GetItemLength(
    uint32_t index,
    const std::vector<FX_FILESIZE>& szArray) const {
  if (szArray.size() < 2 || index > szArray.size() - 2 ||
      szArray[index] > szArray[index + 1]) {
    return 0;
  }
  return szArray[index + 1] - szArray[index];
}

bool CPDF_HintTables::ReadPageHintTable(CFX_BitStream* hStream) {
  if (!hStream || hStream->IsEOF())
    return false;

  const uint32_t kHeaderSize = 288;
  if (hStream->BitsRemaining() < kHeaderSize)
    return false;

  // Item 1: The least number of objects in a page.
  const uint32_t dwObjLeastNum = hStream->GetBits(32);
  if (!dwObjLeastNum)
    return false;

  // Item 2: The location of the first page's page object.
  const FX_FILESIZE szFirstObjLoc =
      HintsOffsetToFileOffset(hStream->GetBits(32));
  if (!szFirstObjLoc)
    return false;

  m_szFirstPageObjOffset = szFirstObjLoc;

  // Item 3: The number of bits needed to represent the difference
  // between the greatest and least number of objects in a page.
  const uint32_t dwDeltaObjectsBits = hStream->GetBits(16);
  if (!IsValidPageOffsetHintTableBitCount(dwDeltaObjectsBits))
    return false;

  // Item 4: The least length of a page in bytes.
  const uint32_t dwPageLeastLen = hStream->GetBits(32);
  if (!dwPageLeastLen)
    return false;

  // Item 5: The number of bits needed to represent the difference
  // between the greatest and least length of a page, in bytes.
  const uint32_t dwDeltaPageLenBits = hStream->GetBits(16);
  if (!IsValidPageOffsetHintTableBitCount(dwDeltaPageLenBits))
    return false;

  // Skip Item 6, 7, 8, 9 total 96 bits.
  hStream->SkipBits(96);

  // Item 10: The number of bits needed to represent the greatest
  // number of shared object references.
  const uint32_t dwSharedObjBits = hStream->GetBits(16);
  if (!IsValidPageOffsetHintTableBitCount(dwSharedObjBits))
    return false;

  // Item 11: The number of bits needed to represent the numerically
  // greatest shared object identifier used by the pages.
  const uint32_t dwSharedIdBits = hStream->GetBits(16);
  if (!IsValidPageOffsetHintTableBitCount(dwSharedIdBits))
    return false;

  // Item 12: The number of bits needed to represent the numerator of
  // the fractional position for each shared object reference. For each
  // shared object referenced from a page, there is an indication of
  // where in the page's content stream the object is first referenced.
  const uint32_t dwSharedNumeratorBits = hStream->GetBits(16);
  if (!IsValidPageOffsetHintTableBitCount(dwSharedNumeratorBits))
    return false;

  // Item 13: Skip Item 13 which has 16 bits.
  hStream->SkipBits(16);

  const uint32_t nPages = m_pLinearized->GetPageCount();
  if (nPages < 1 || nPages >= CPDF_Document::kPageMaxNum)
    return false;

  const uint32_t dwPages = pdfium::base::checked_cast<uint32_t>(nPages);
  FX_SAFE_UINT32 required_bits = dwDeltaObjectsBits;
  required_bits *= dwPages;
  if (!CanReadFromBitStream(hStream, required_bits))
    return false;

  for (uint32_t i = 0; i < nPages; ++i) {
    FX_SAFE_UINT32 safeDeltaObj = hStream->GetBits(dwDeltaObjectsBits);
    safeDeltaObj += dwObjLeastNum;
    if (!safeDeltaObj.IsValid())
      return false;
    m_dwDeltaNObjsArray.push_back(safeDeltaObj.ValueOrDie());
  }
  hStream->ByteAlign();

  required_bits = dwDeltaPageLenBits;
  required_bits *= dwPages;
  if (!CanReadFromBitStream(hStream, required_bits))
    return false;

  std::vector<uint32_t> dwPageLenArray;
  for (uint32_t i = 0; i < nPages; ++i) {
    FX_SAFE_UINT32 safePageLen = hStream->GetBits(dwDeltaPageLenBits);
    safePageLen += dwPageLeastLen;
    if (!safePageLen.IsValid())
      return false;

    dwPageLenArray.push_back(safePageLen.ValueOrDie());
  }

  const uint32_t nFirstPageNum = m_pLinearized->GetFirstPageNo();
  if (nFirstPageNum >= nPages)
    return false;

  m_szPageOffsetArray.resize(nPages, 0);
  ASSERT(m_szFirstPageObjOffset);
  m_szPageOffsetArray[nFirstPageNum] = m_szFirstPageObjOffset;
  FX_FILESIZE prev_page_offset = m_pLinearized->GetFirstPageEndOffset();
  for (uint32_t i = 0; i < nPages; ++i) {
    if (i == nFirstPageNum)
      continue;

    m_szPageOffsetArray[i] = prev_page_offset;
    prev_page_offset += dwPageLenArray[i];
  }
  m_szPageOffsetArray.push_back(m_szPageOffsetArray[nPages - 1] +
                                dwPageLenArray[nPages - 1]);
  hStream->ByteAlign();

  // Number of shared objects.
  required_bits = dwSharedObjBits;
  required_bits *= dwPages;
  if (!CanReadFromBitStream(hStream, required_bits))
    return false;

  for (uint32_t i = 0; i < nPages; i++)
    m_dwNSharedObjsArray.push_back(hStream->GetBits(dwSharedObjBits));
  hStream->ByteAlign();

  // Array of identifiers, size = nshared_objects.
  for (uint32_t i = 0; i < nPages; i++) {
    required_bits = dwSharedIdBits;
    required_bits *= m_dwNSharedObjsArray[i];
    if (!CanReadFromBitStream(hStream, required_bits))
      return false;

    for (uint32_t j = 0; j < m_dwNSharedObjsArray[i]; j++)
      m_dwIdentifierArray.push_back(hStream->GetBits(dwSharedIdBits));
  }
  hStream->ByteAlign();

  for (uint32_t i = 0; i < nPages; i++) {
    FX_SAFE_UINT32 safeSize = m_dwNSharedObjsArray[i];
    safeSize *= dwSharedNumeratorBits;
    if (!CanReadFromBitStream(hStream, safeSize))
      return false;

    hStream->SkipBits(safeSize.ValueOrDie());
  }
  hStream->ByteAlign();

  FX_SAFE_UINT32 safeTotalPageLen = dwPages;
  safeTotalPageLen *= dwDeltaPageLenBits;
  if (!CanReadFromBitStream(hStream, safeTotalPageLen))
    return false;

  hStream->SkipBits(safeTotalPageLen.ValueOrDie());
  hStream->ByteAlign();
  return true;
}

bool CPDF_HintTables::ReadSharedObjHintTable(CFX_BitStream* hStream,
                                             uint32_t offset) {
  if (!hStream || hStream->IsEOF())
    return false;

  FX_SAFE_UINT32 bit_offset = offset;
  bit_offset *= 8;
  if (!bit_offset.IsValid() || hStream->GetPos() > bit_offset.ValueOrDie())
    return false;
  hStream->SkipBits((bit_offset - hStream->GetPos()).ValueOrDie());

  const uint32_t kHeaderSize = 192;
  if (hStream->BitsRemaining() < kHeaderSize)
    return false;

  // Item 1: The object number of the first object in the shared objects
  // section.
  uint32_t dwFirstSharedObjNum = hStream->GetBits(32);

  // Item 2: The location of the first object in the shared objects section.
  const FX_FILESIZE szFirstSharedObjLoc =
      HintsOffsetToFileOffset(hStream->GetBits(32));
  if (!szFirstSharedObjLoc)
    return false;

  // Item 3: The number of shared object entries for the first page.
  m_nFirstPageSharedObjs = hStream->GetBits(32);

  // Item 4: The number of shared object entries for the shared objects
  // section, including the number of shared object entries for the first page.
  uint32_t dwSharedObjTotal = hStream->GetBits(32);

  // Item 5: The number of bits needed to represent the greatest number of
  // objects in a shared object group. Skipped.
  hStream->SkipBits(16);

  // Item 6: The least length of a shared object group in bytes.
  uint32_t dwGroupLeastLen = hStream->GetBits(32);

  // Item 7: The number of bits needed to represent the difference between the
  // greatest and least length of a shared object group, in bytes.
  uint32_t dwDeltaGroupLen = hStream->GetBits(16);

  // Trying to decode more than 32 bits isn't going to work when we write into
  // a uint32_t. Decoding 0 bits also makes no sense.
  if (!IsValidPageOffsetHintTableBitCount(dwDeltaGroupLen))
    return false;

  if (dwFirstSharedObjNum >= CPDF_Parser::kMaxObjectNumber ||
      m_nFirstPageSharedObjs >= CPDF_Parser::kMaxObjectNumber ||
      dwSharedObjTotal >= CPDF_Parser::kMaxObjectNumber) {
    return false;
  }

  const uint32_t nFirstPageObjNum = m_pLinearized->GetFirstPageObjNum();

  uint32_t dwPrevObjLen = 0;
  uint32_t dwCurObjLen = 0;
  FX_SAFE_UINT32 required_bits = dwSharedObjTotal;
  required_bits *= dwDeltaGroupLen;
  if (!CanReadFromBitStream(hStream, required_bits))
    return false;

  for (uint32_t i = 0; i < dwSharedObjTotal; ++i) {
    dwPrevObjLen = dwCurObjLen;
    FX_SAFE_UINT32 safeObjLen = hStream->GetBits(dwDeltaGroupLen);
    safeObjLen += dwGroupLeastLen;
    if (!safeObjLen.IsValid())
      return false;

    dwCurObjLen = safeObjLen.ValueOrDie();
    if (i < m_nFirstPageSharedObjs) {
      m_dwSharedObjNumArray.push_back(nFirstPageObjNum + i);
      if (i == 0)
        m_szSharedObjOffsetArray.push_back(m_szFirstPageObjOffset);
    } else {
      FX_SAFE_UINT32 safeObjNum = dwFirstSharedObjNum;
      safeObjNum += i - m_nFirstPageSharedObjs;
      if (!safeObjNum.IsValid())
        return false;

      m_dwSharedObjNumArray.push_back(safeObjNum.ValueOrDie());
      if (i == m_nFirstPageSharedObjs) {
        FX_SAFE_FILESIZE safeLoc = szFirstSharedObjLoc;
        if (!safeLoc.IsValid())
          return false;

        m_szSharedObjOffsetArray.push_back(safeLoc.ValueOrDie());
      }
    }

    if (i != 0 && i != m_nFirstPageSharedObjs) {
      FX_SAFE_FILESIZE safeLoc = dwPrevObjLen;
      safeLoc += m_szSharedObjOffsetArray[i - 1];
      if (!safeLoc.IsValid())
        return false;

      m_szSharedObjOffsetArray.push_back(safeLoc.ValueOrDie());
    }
  }

  if (dwSharedObjTotal > 0) {
    FX_SAFE_FILESIZE safeLoc = dwCurObjLen;
    safeLoc += m_szSharedObjOffsetArray[dwSharedObjTotal - 1];
    if (!safeLoc.IsValid())
      return false;

    m_szSharedObjOffsetArray.push_back(safeLoc.ValueOrDie());
  }

  hStream->ByteAlign();
  if (hStream->BitsRemaining() < dwSharedObjTotal)
    return false;

  hStream->SkipBits(dwSharedObjTotal);
  hStream->ByteAlign();
  return true;
}

bool CPDF_HintTables::GetPagePos(uint32_t index,
                                 FX_FILESIZE* szPageStartPos,
                                 FX_FILESIZE* szPageLength,
                                 uint32_t* dwObjNum) const {
  if (index >= m_pLinearized->GetPageCount())
    return false;

  *szPageStartPos = m_szPageOffsetArray[index];
  *szPageLength = GetItemLength(index, m_szPageOffsetArray);

  const uint32_t nFirstPageObjNum = m_pLinearized->GetFirstPageObjNum();

  const uint32_t dwFirstPageNum = m_pLinearized->GetFirstPageNo();
  if (index == dwFirstPageNum) {
    *dwObjNum = nFirstPageObjNum;
    return true;
  }

  // The object number of remaining pages starts from 1.
  *dwObjNum = 1;
  for (uint32_t i = 0; i < index; ++i) {
    if (i == dwFirstPageNum)
      continue;
    *dwObjNum += m_dwDeltaNObjsArray[i];
  }
  return true;
}

CPDF_DataAvail::DocAvailStatus CPDF_HintTables::CheckPage(uint32_t index) {
  if (index == m_pLinearized->GetFirstPageNo())
    return CPDF_DataAvail::DataAvailable;

  uint32_t dwLength = GetItemLength(index, m_szPageOffsetArray);
  // If two pages have the same offset, it should be treated as an error.
  if (!dwLength)
    return CPDF_DataAvail::DataError;

  if (!m_pValidator->CheckDataRangeAndRequestIfUnavailable(
          m_szPageOffsetArray[index], dwLength)) {
    return CPDF_DataAvail::DataNotAvailable;
  }

  // Download data of shared objects in the page.
  uint32_t offset = 0;
  for (uint32_t i = 0; i < index; ++i)
    offset += m_dwNSharedObjsArray[i];

  const uint32_t nFirstPageObjNum = m_pLinearized->GetFirstPageObjNum();

  uint32_t dwIndex = 0;
  uint32_t dwObjNum = 0;
  for (uint32_t j = 0; j < m_dwNSharedObjsArray[index]; ++j) {
    dwIndex = m_dwIdentifierArray[offset + j];
    if (dwIndex >= m_dwSharedObjNumArray.size())
      continue;

    dwObjNum = m_dwSharedObjNumArray[dwIndex];
    if (dwObjNum >= static_cast<uint32_t>(nFirstPageObjNum) &&
        dwObjNum <
            static_cast<uint32_t>(nFirstPageObjNum) + m_nFirstPageSharedObjs) {
      continue;
    }

    dwLength = GetItemLength(dwIndex, m_szSharedObjOffsetArray);
    // If two objects have the same offset, it should be treated as an error.
    if (!dwLength)
      return CPDF_DataAvail::DataError;

    if (!m_pValidator->CheckDataRangeAndRequestIfUnavailable(
            m_szSharedObjOffsetArray[dwIndex], dwLength)) {
      return CPDF_DataAvail::DataNotAvailable;
    }
  }
  return CPDF_DataAvail::DataAvailable;
}

bool CPDF_HintTables::LoadHintStream(CPDF_Stream* pHintStream) {
  if (!pHintStream || !m_pLinearized->HasHintTable())
    return false;

  CPDF_Dictionary* pDict = pHintStream->GetDict();
  CPDF_Object* pOffset = pDict ? pDict->GetObjectFor("S") : nullptr;
  if (!pOffset || !pOffset->IsNumber())
    return false;

  int shared_hint_table_offset = pOffset->GetInteger();
  if (shared_hint_table_offset <= 0)
    return false;

  auto pAcc = pdfium::MakeRetain<CPDF_StreamAcc>(pHintStream);
  pAcc->LoadAllDataFiltered();

  uint32_t size = pAcc->GetSize();
  // The header section of page offset hint table is 36 bytes.
  // The header section of shared object hint table is 24 bytes.
  // Hint table has at least 60 bytes.
  const uint32_t kMinStreamLength = 60;
  if (size < kMinStreamLength)
    return false;

  FX_SAFE_UINT32 safe_shared_hint_table_offset = shared_hint_table_offset;
  if (!safe_shared_hint_table_offset.IsValid() ||
      size < safe_shared_hint_table_offset.ValueOrDie()) {
    return false;
  }

  CFX_BitStream bs(pdfium::make_span(pAcc->GetData(), size));
  return ReadPageHintTable(&bs) &&
         ReadSharedObjHintTable(&bs, shared_hint_table_offset);
}

FX_FILESIZE CPDF_HintTables::HintsOffsetToFileOffset(
    uint32_t hints_offset) const {
  FX_SAFE_FILESIZE file_offset = hints_offset;
  if (!file_offset.IsValid())
    return 0;

  // The resulting positions shall be interpreted as if the primary hint stream
  // itself were not present. That is, a position greater than the hint stream
  // offset shall have the hint stream length added to it to determine the
  // actual offset relative to the beginning of the file.
  // See specification PDF 32000-1:2008 Annex F.4 (Hint tables).
  if (file_offset.ValueOrDie() > m_pLinearized->GetHintStart())
    file_offset += m_pLinearized->GetHintLength();

  return file_offset.ValueOrDefault(0);
}
