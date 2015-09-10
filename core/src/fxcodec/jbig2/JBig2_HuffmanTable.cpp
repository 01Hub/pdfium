// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "JBig2_HuffmanTable.h"

#include <string.h>

#include "../../../include/fxcrt/fx_memory.h"
#include "JBig2_BitStream.h"

CJBig2_HuffmanTable::CJBig2_HuffmanTable(const JBig2TableLine* pTable,
                                         int nLines,
                                         FX_BOOL bHTOOB) {
  init();
  m_bOK = parseFromStandardTable(pTable, nLines, bHTOOB);
}

CJBig2_HuffmanTable::CJBig2_HuffmanTable(CJBig2_BitStream* pStream) {
  init();
  m_bOK = parseFromCodedBuffer(pStream);
}

CJBig2_HuffmanTable::~CJBig2_HuffmanTable() {
  FX_Free(CODES);
  FX_Free(PREFLEN);
  FX_Free(RANGELEN);
  FX_Free(RANGELOW);
}
void CJBig2_HuffmanTable::init() {
  HTOOB = FALSE;
  NTEMP = 0;
  CODES = NULL;
  PREFLEN = NULL;
  RANGELEN = NULL;
  RANGELOW = NULL;
}
int CJBig2_HuffmanTable::parseFromStandardTable(const JBig2TableLine* pTable,
                                                int nLines,
                                                FX_BOOL bHTOOB) {
  int CURLEN, LENMAX, CURCODE, CURTEMP, i;
  int* LENCOUNT;
  int* FIRSTCODE;
  HTOOB = bHTOOB;
  NTEMP = nLines;
  CODES = (int*)FX_AllocOrDie(sizeof(int), NTEMP);
  PREFLEN = (int*)FX_AllocOrDie(sizeof(int), NTEMP);
  RANGELEN = (int*)FX_AllocOrDie(sizeof(int), NTEMP);
  RANGELOW = (int*)FX_AllocOrDie(sizeof(int), NTEMP);
  LENMAX = 0;
  for (i = 0; i < NTEMP; i++) {
    PREFLEN[i] = pTable[i].PREFLEN;
    RANGELEN[i] = pTable[i].RANDELEN;
    RANGELOW[i] = pTable[i].RANGELOW;
    if (PREFLEN[i] > LENMAX) {
      LENMAX = PREFLEN[i];
    }
  }
  LENCOUNT = (int*)FX_AllocOrDie(sizeof(int), (LENMAX + 1));
  JBIG2_memset(LENCOUNT, 0, sizeof(int) * (LENMAX + 1));
  FIRSTCODE = (int*)FX_AllocOrDie(sizeof(int), (LENMAX + 1));
  for (i = 0; i < NTEMP; i++) {
    LENCOUNT[PREFLEN[i]]++;
  }
  CURLEN = 1;
  FIRSTCODE[0] = 0;
  LENCOUNT[0] = 0;
  while (CURLEN <= LENMAX) {
    FIRSTCODE[CURLEN] = (FIRSTCODE[CURLEN - 1] + LENCOUNT[CURLEN - 1]) << 1;
    CURCODE = FIRSTCODE[CURLEN];
    CURTEMP = 0;
    while (CURTEMP < NTEMP) {
      if (PREFLEN[CURTEMP] == CURLEN) {
        CODES[CURTEMP] = CURCODE;
        CURCODE = CURCODE + 1;
      }
      CURTEMP = CURTEMP + 1;
    }
    CURLEN = CURLEN + 1;
  }
  FX_Free(LENCOUNT);
  FX_Free(FIRSTCODE);
  return 1;
}

#define HT_CHECK_MEMORY_ADJUST                                           \
  if (NTEMP >= nSize) {                                                  \
    nSize += 16;                                                         \
    PREFLEN = (int*)FX_Realloc(uint8_t, PREFLEN, sizeof(int) * nSize);   \
    RANGELEN = (int*)FX_Realloc(uint8_t, RANGELEN, sizeof(int) * nSize); \
    RANGELOW = (int*)FX_Realloc(uint8_t, RANGELOW, sizeof(int) * nSize); \
  }
int CJBig2_HuffmanTable::parseFromCodedBuffer(CJBig2_BitStream* pStream) {
  unsigned char HTPS, HTRS;
  FX_DWORD HTLOW, HTHIGH;
  FX_DWORD CURRANGELOW;
  FX_DWORD nSize = 16;
  int CURLEN, LENMAX, CURCODE, CURTEMP;
  int* LENCOUNT;
  int* FIRSTCODE;
  unsigned char cTemp;
  if (pStream->read1Byte(&cTemp) == -1) {
    goto failed;
  }
  HTOOB = cTemp & 0x01;
  HTPS = ((cTemp >> 1) & 0x07) + 1;
  HTRS = ((cTemp >> 4) & 0x07) + 1;
  if (pStream->readInteger(&HTLOW) == -1 ||
      pStream->readInteger(&HTHIGH) == -1 || HTLOW > HTHIGH) {
    goto failed;
  }
  PREFLEN = (int*)FX_AllocOrDie(sizeof(int), nSize);
  RANGELEN = (int*)FX_AllocOrDie(sizeof(int), nSize);
  RANGELOW = (int*)FX_AllocOrDie(sizeof(int), nSize);
  CURRANGELOW = HTLOW;
  NTEMP = 0;
  do {
    HT_CHECK_MEMORY_ADJUST
    if ((pStream->readNBits(HTPS, &PREFLEN[NTEMP]) == -1) ||
        (pStream->readNBits(HTRS, &RANGELEN[NTEMP]) == -1)) {
      goto failed;
    }
    RANGELOW[NTEMP] = CURRANGELOW;
    CURRANGELOW = CURRANGELOW + (1 << RANGELEN[NTEMP]);
    NTEMP = NTEMP + 1;
  } while (CURRANGELOW < HTHIGH);
  HT_CHECK_MEMORY_ADJUST
  if (pStream->readNBits(HTPS, &PREFLEN[NTEMP]) == -1) {
    goto failed;
  }
  RANGELEN[NTEMP] = 32;
  RANGELOW[NTEMP] = HTLOW - 1;
  NTEMP = NTEMP + 1;
  HT_CHECK_MEMORY_ADJUST
  if (pStream->readNBits(HTPS, &PREFLEN[NTEMP]) == -1) {
    goto failed;
  }
  RANGELEN[NTEMP] = 32;
  RANGELOW[NTEMP] = HTHIGH;
  NTEMP = NTEMP + 1;
  if (HTOOB) {
    HT_CHECK_MEMORY_ADJUST
    if (pStream->readNBits(HTPS, &PREFLEN[NTEMP]) == -1) {
      goto failed;
    }
    NTEMP = NTEMP + 1;
  }
  CODES = (int*)FX_AllocOrDie(sizeof(int), NTEMP);
  LENMAX = 0;
  for (int i = 0; i < NTEMP; i++) {
    if (PREFLEN[i] > LENMAX) {
      LENMAX = PREFLEN[i];
    }
  }
  LENCOUNT = (int*)FX_AllocOrDie(sizeof(int), (LENMAX + 1));
  JBIG2_memset(LENCOUNT, 0, sizeof(int) * (LENMAX + 1));
  FIRSTCODE = (int*)FX_AllocOrDie(sizeof(int), (LENMAX + 1));
  for (int i = 0; i < NTEMP; i++) {
    LENCOUNT[PREFLEN[i]]++;
  }
  CURLEN = 1;
  FIRSTCODE[0] = 0;
  LENCOUNT[0] = 0;
  while (CURLEN <= LENMAX) {
    FIRSTCODE[CURLEN] = (FIRSTCODE[CURLEN - 1] + LENCOUNT[CURLEN - 1]) << 1;
    CURCODE = FIRSTCODE[CURLEN];
    CURTEMP = 0;
    while (CURTEMP < NTEMP) {
      if (PREFLEN[CURTEMP] == CURLEN) {
        CODES[CURTEMP] = CURCODE;
        CURCODE = CURCODE + 1;
      }
      CURTEMP = CURTEMP + 1;
    }
    CURLEN = CURLEN + 1;
  }
  FX_Free(LENCOUNT);
  FX_Free(FIRSTCODE);
  return TRUE;
failed:
  return FALSE;
}
