// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/src/fpdfapi/fpdf_page/pageint.h"

#include <limits.h>

#include <memory>
#include <utility>
#include <vector>

#include "core/include/fpdfapi/fpdf_module.h"
#include "core/include/fpdfapi/fpdf_page.h"
#include "core/include/fxcrt/fx_safe_types.h"
#include "third_party/base/numerics/safe_conversions_impl.h"

namespace {

enum PDF_PSOP {
  PSOP_ADD,
  PSOP_SUB,
  PSOP_MUL,
  PSOP_DIV,
  PSOP_IDIV,
  PSOP_MOD,
  PSOP_NEG,
  PSOP_ABS,
  PSOP_CEILING,
  PSOP_FLOOR,
  PSOP_ROUND,
  PSOP_TRUNCATE,
  PSOP_SQRT,
  PSOP_SIN,
  PSOP_COS,
  PSOP_ATAN,
  PSOP_EXP,
  PSOP_LN,
  PSOP_LOG,
  PSOP_CVI,
  PSOP_CVR,
  PSOP_EQ,
  PSOP_NE,
  PSOP_GT,
  PSOP_GE,
  PSOP_LT,
  PSOP_LE,
  PSOP_AND,
  PSOP_OR,
  PSOP_XOR,
  PSOP_NOT,
  PSOP_BITSHIFT,
  PSOP_TRUE,
  PSOP_FALSE,
  PSOP_IF,
  PSOP_IFELSE,
  PSOP_POP,
  PSOP_EXCH,
  PSOP_DUP,
  PSOP_COPY,
  PSOP_INDEX,
  PSOP_ROLL,
  PSOP_PROC,
  PSOP_CONST
};

class CPDF_PSEngine;
class CPDF_PSProc;

class CPDF_PSOP {
 public:
  explicit CPDF_PSOP(PDF_PSOP op) : m_op(op), m_value(0) {
    ASSERT(m_op != PSOP_CONST);
    ASSERT(m_op != PSOP_PROC);
  }
  explicit CPDF_PSOP(FX_FLOAT value) : m_op(PSOP_CONST), m_value(value) {}
  explicit CPDF_PSOP(std::unique_ptr<CPDF_PSProc> proc)
      : m_op(PSOP_PROC), m_value(0), m_proc(std::move(proc)) {}

  FX_FLOAT GetFloatValue() const {
    if (m_op == PSOP_CONST)
      return m_value;

    ASSERT(false);
    return 0;
  }
  CPDF_PSProc* GetProc() const {
    if (m_op == PSOP_PROC)
      return m_proc.get();
    ASSERT(false);
    return nullptr;
  }

  PDF_PSOP GetOp() const { return m_op; }

 private:
  const PDF_PSOP m_op;
  const FX_FLOAT m_value;
  std::unique_ptr<CPDF_PSProc> m_proc;
};

class CPDF_PSProc {
 public:
  CPDF_PSProc() {}
  ~CPDF_PSProc() {}

  FX_BOOL Parse(CPDF_SimpleParser* parser);
  FX_BOOL Execute(CPDF_PSEngine* pEngine);

 private:
  std::vector<std::unique_ptr<CPDF_PSOP>> m_Operators;
};

const size_t PSENGINE_STACKSIZE = 100;

class CPDF_PSEngine {
 public:
  CPDF_PSEngine();
  ~CPDF_PSEngine();

  FX_BOOL Parse(const FX_CHAR* string, int size);
  FX_BOOL Execute() { return m_MainProc.Execute(this); }
  FX_BOOL DoOperator(PDF_PSOP op);
  void Reset() { m_StackCount = 0; }
  void Push(FX_FLOAT value);
  void Push(int value) { Push((FX_FLOAT)value); }
  FX_FLOAT Pop();
  int GetStackSize() const { return m_StackCount; }

 private:
  FX_FLOAT m_Stack[PSENGINE_STACKSIZE];
  int m_StackCount;
  CPDF_PSProc m_MainProc;
};

FX_BOOL CPDF_PSProc::Execute(CPDF_PSEngine* pEngine) {
  for (size_t i = 0; i < m_Operators.size(); ++i) {
    const PDF_PSOP op = m_Operators[i]->GetOp();
    if (op == PSOP_PROC)
      continue;

    if (op == PSOP_CONST) {
      pEngine->Push(m_Operators[i]->GetFloatValue());
      continue;
    }

    if (op == PSOP_IF) {
      if (i == 0 || m_Operators[i - 1]->GetOp() != PSOP_PROC)
        return FALSE;

      if (static_cast<int>(pEngine->Pop()))
        m_Operators[i - 1]->GetProc()->Execute(pEngine);
    } else if (op == PSOP_IFELSE) {
      if (i < 2 || m_Operators[i - 1]->GetOp() != PSOP_PROC ||
          m_Operators[i - 2]->GetOp() != PSOP_PROC) {
        return FALSE;
      }
      size_t offset = static_cast<int>(pEngine->Pop()) ? 2 : 1;
      m_Operators[i - offset]->GetProc()->Execute(pEngine);
    } else {
      pEngine->DoOperator(op);
    }
  }
  return TRUE;
}

CPDF_PSEngine::CPDF_PSEngine() {
  m_StackCount = 0;
}
CPDF_PSEngine::~CPDF_PSEngine() {}
void CPDF_PSEngine::Push(FX_FLOAT v) {
  if (m_StackCount == PSENGINE_STACKSIZE) {
    return;
  }
  m_Stack[m_StackCount++] = v;
}
FX_FLOAT CPDF_PSEngine::Pop() {
  if (m_StackCount == 0) {
    return 0;
  }
  return m_Stack[--m_StackCount];
}
const struct PDF_PSOpName {
  const FX_CHAR* name;
  PDF_PSOP op;
} PDF_PSOpNames[] = {{"add", PSOP_ADD},         {"sub", PSOP_SUB},
                     {"mul", PSOP_MUL},         {"div", PSOP_DIV},
                     {"idiv", PSOP_IDIV},       {"mod", PSOP_MOD},
                     {"neg", PSOP_NEG},         {"abs", PSOP_ABS},
                     {"ceiling", PSOP_CEILING}, {"floor", PSOP_FLOOR},
                     {"round", PSOP_ROUND},     {"truncate", PSOP_TRUNCATE},
                     {"sqrt", PSOP_SQRT},       {"sin", PSOP_SIN},
                     {"cos", PSOP_COS},         {"atan", PSOP_ATAN},
                     {"exp", PSOP_EXP},         {"ln", PSOP_LN},
                     {"log", PSOP_LOG},         {"cvi", PSOP_CVI},
                     {"cvr", PSOP_CVR},         {"eq", PSOP_EQ},
                     {"ne", PSOP_NE},           {"gt", PSOP_GT},
                     {"ge", PSOP_GE},           {"lt", PSOP_LT},
                     {"le", PSOP_LE},           {"and", PSOP_AND},
                     {"or", PSOP_OR},           {"xor", PSOP_XOR},
                     {"not", PSOP_NOT},         {"bitshift", PSOP_BITSHIFT},
                     {"true", PSOP_TRUE},       {"false", PSOP_FALSE},
                     {"if", PSOP_IF},           {"ifelse", PSOP_IFELSE},
                     {"pop", PSOP_POP},         {"exch", PSOP_EXCH},
                     {"dup", PSOP_DUP},         {"copy", PSOP_COPY},
                     {"index", PSOP_INDEX},     {"roll", PSOP_ROLL}};

FX_BOOL CPDF_PSEngine::Parse(const FX_CHAR* string, int size) {
  CPDF_SimpleParser parser((uint8_t*)string, size);
  CFX_ByteStringC word = parser.GetWord();
  if (word != "{") {
    return FALSE;
  }
  return m_MainProc.Parse(&parser);
}
FX_BOOL CPDF_PSProc::Parse(CPDF_SimpleParser* parser) {
  while (1) {
    CFX_ByteStringC word = parser->GetWord();
    if (word.IsEmpty()) {
      return FALSE;
    }
    if (word == "}") {
      return TRUE;
    }
    if (word == "{") {
      std::unique_ptr<CPDF_PSProc> proc(new CPDF_PSProc);
      std::unique_ptr<CPDF_PSOP> op(new CPDF_PSOP(std::move(proc)));
      m_Operators.push_back(std::move(op));
      if (!m_Operators.back()->GetProc()->Parse(parser)) {
        return FALSE;
      }
    } else {
      bool found = false;
      for (const PDF_PSOpName& op_name : PDF_PSOpNames) {
        if (word == CFX_ByteStringC(op_name.name)) {
          std::unique_ptr<CPDF_PSOP> op(new CPDF_PSOP(op_name.op));
          m_Operators.push_back(std::move(op));
          found = true;
          break;
        }
      }
      if (!found) {
        std::unique_ptr<CPDF_PSOP> op(new CPDF_PSOP(FX_atof(word)));
        m_Operators.push_back(std::move(op));
      }
    }
  }
}

FX_BOOL CPDF_PSEngine::DoOperator(PDF_PSOP op) {
  int i1, i2;
  FX_FLOAT d1, d2;
  switch (op) {
    case PSOP_ADD:
      d1 = Pop();
      d2 = Pop();
      Push(d1 + d2);
      break;
    case PSOP_SUB:
      d2 = Pop();
      d1 = Pop();
      Push(d1 - d2);
      break;
    case PSOP_MUL:
      d1 = Pop();
      d2 = Pop();
      Push(d1 * d2);
      break;
    case PSOP_DIV:
      d2 = Pop();
      d1 = Pop();
      Push(d1 / d2);
      break;
    case PSOP_IDIV:
      i2 = (int)Pop();
      i1 = (int)Pop();
      Push(i1 / i2);
      break;
    case PSOP_MOD:
      i2 = (int)Pop();
      i1 = (int)Pop();
      Push(i1 % i2);
      break;
    case PSOP_NEG:
      d1 = Pop();
      Push(-d1);
      break;
    case PSOP_ABS:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_fabs(d1));
      break;
    case PSOP_CEILING:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_ceil(d1));
      break;
    case PSOP_FLOOR:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_floor(d1));
      break;
    case PSOP_ROUND:
      d1 = Pop();
      Push(FXSYS_round(d1));
      break;
    case PSOP_TRUNCATE:
      i1 = (int)Pop();
      Push(i1);
      break;
    case PSOP_SQRT:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_sqrt(d1));
      break;
    case PSOP_SIN:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_sin(d1 * FX_PI / 180.0f));
      break;
    case PSOP_COS:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_cos(d1 * FX_PI / 180.0f));
      break;
    case PSOP_ATAN:
      d2 = Pop();
      d1 = Pop();
      d1 = (FX_FLOAT)(FXSYS_atan2(d1, d2) * 180.0 / FX_PI);
      if (d1 < 0) {
        d1 += 360;
      }
      Push(d1);
      break;
    case PSOP_EXP:
      d2 = Pop();
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_pow(d1, d2));
      break;
    case PSOP_LN:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_log(d1));
      break;
    case PSOP_LOG:
      d1 = Pop();
      Push((FX_FLOAT)FXSYS_log10(d1));
      break;
    case PSOP_CVI:
      i1 = (int)Pop();
      Push(i1);
      break;
    case PSOP_CVR:
      break;
    case PSOP_EQ:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 == d2));
      break;
    case PSOP_NE:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 != d2));
      break;
    case PSOP_GT:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 > d2));
      break;
    case PSOP_GE:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 >= d2));
      break;
    case PSOP_LT:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 < d2));
      break;
    case PSOP_LE:
      d2 = Pop();
      d1 = Pop();
      Push((int)(d1 <= d2));
      break;
    case PSOP_AND:
      i1 = (int)Pop();
      i2 = (int)Pop();
      Push(i1 & i2);
      break;
    case PSOP_OR:
      i1 = (int)Pop();
      i2 = (int)Pop();
      Push(i1 | i2);
      break;
    case PSOP_XOR:
      i1 = (int)Pop();
      i2 = (int)Pop();
      Push(i1 ^ i2);
      break;
    case PSOP_NOT:
      i1 = (int)Pop();
      Push((int)!i1);
      break;
    case PSOP_BITSHIFT: {
      int shift = (int)Pop();
      int i = (int)Pop();
      if (shift > 0) {
        Push(i << shift);
      } else {
        Push(i >> -shift);
      }
      break;
    }
    case PSOP_TRUE:
      Push(1);
      break;
    case PSOP_FALSE:
      Push(0);
      break;
    case PSOP_POP:
      Pop();
      break;
    case PSOP_EXCH:
      d2 = Pop();
      d1 = Pop();
      Push(d2);
      Push(d1);
      break;
    case PSOP_DUP:
      d1 = Pop();
      Push(d1);
      Push(d1);
      break;
    case PSOP_COPY: {
      int n = (int)Pop();
      if (n < 0 || n > PSENGINE_STACKSIZE ||
          m_StackCount + n > PSENGINE_STACKSIZE || n > m_StackCount) {
        break;
      }
      for (int i = 0; i < n; i++) {
        m_Stack[m_StackCount + i] = m_Stack[m_StackCount + i - n];
      }
      m_StackCount += n;
      break;
    }
    case PSOP_INDEX: {
      int n = (int)Pop();
      if (n < 0 || n >= m_StackCount) {
        break;
      }
      Push(m_Stack[m_StackCount - n - 1]);
      break;
    }
    case PSOP_ROLL: {
      int j = (int)Pop();
      int n = (int)Pop();
      if (m_StackCount == 0) {
        break;
      }
      if (n < 0 || n > m_StackCount) {
        break;
      }
      if (j < 0) {
        for (int i = 0; i < -j; i++) {
          FX_FLOAT first = m_Stack[m_StackCount - n];
          for (int ii = 0; ii < n - 1; ii++) {
            m_Stack[m_StackCount - n + ii] = m_Stack[m_StackCount - n + ii + 1];
          }
          m_Stack[m_StackCount - 1] = first;
        }
      } else {
        for (int i = 0; i < j; i++) {
          FX_FLOAT last = m_Stack[m_StackCount - 1];
          int ii;
          for (ii = 0; ii < n - 1; ii++) {
            m_Stack[m_StackCount - ii - 1] = m_Stack[m_StackCount - ii - 2];
          }
          m_Stack[m_StackCount - ii - 1] = last;
        }
      }
      break;
    }
    default:
      break;
  }
  return TRUE;
}
static FX_FLOAT PDF_Interpolate(FX_FLOAT x,
                                FX_FLOAT xmin,
                                FX_FLOAT xmax,
                                FX_FLOAT ymin,
                                FX_FLOAT ymax) {
  return ((x - xmin) * (ymax - ymin) / (xmax - xmin)) + ymin;
}
static FX_DWORD _GetBits32(const uint8_t* pData, int bitpos, int nbits) {
  int result = 0;
  for (int i = 0; i < nbits; i++)
    if (pData[(bitpos + i) / 8] & (1 << (7 - (bitpos + i) % 8))) {
      result |= 1 << (nbits - i - 1);
    }
  return result;
}
typedef struct {
  FX_FLOAT encode_max, encode_min;
  int sizes;
} SampleEncodeInfo;
typedef struct { FX_FLOAT decode_max, decode_min; } SampleDecodeInfo;

class CPDF_SampledFunc : public CPDF_Function {
 public:
  CPDF_SampledFunc();
  ~CPDF_SampledFunc() override;

  // CPDF_Function
  FX_BOOL v_Init(CPDF_Object* pObj) override;
  FX_BOOL v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const override;

  SampleEncodeInfo* m_pEncodeInfo;
  SampleDecodeInfo* m_pDecodeInfo;
  FX_DWORD m_nBitsPerSample;
  FX_DWORD m_SampleMax;
  CPDF_StreamAcc* m_pSampleStream;
};

CPDF_SampledFunc::CPDF_SampledFunc() {
  m_pSampleStream = NULL;
  m_pEncodeInfo = NULL;
  m_pDecodeInfo = NULL;
}
CPDF_SampledFunc::~CPDF_SampledFunc() {
  delete m_pSampleStream;
  FX_Free(m_pEncodeInfo);
  FX_Free(m_pDecodeInfo);
}
FX_BOOL CPDF_SampledFunc::v_Init(CPDF_Object* pObj) {
  CPDF_Stream* pStream = pObj->AsStream();
  if (!pStream)
    return false;

  CPDF_Dictionary* pDict = pStream->GetDict();
  CPDF_Array* pSize = pDict->GetArrayBy("Size");
  CPDF_Array* pEncode = pDict->GetArrayBy("Encode");
  CPDF_Array* pDecode = pDict->GetArrayBy("Decode");
  m_nBitsPerSample = pDict->GetIntegerBy("BitsPerSample");
  if (m_nBitsPerSample > 32) {
    return FALSE;
  }
  m_SampleMax = 0xffffffff >> (32 - m_nBitsPerSample);
  m_pSampleStream = new CPDF_StreamAcc;
  m_pSampleStream->LoadAllData(pStream, FALSE);
  m_pEncodeInfo = FX_Alloc(SampleEncodeInfo, m_nInputs);
  FX_SAFE_DWORD nTotalSampleBits = 1;
  for (int i = 0; i < m_nInputs; i++) {
    m_pEncodeInfo[i].sizes = pSize ? pSize->GetIntegerAt(i) : 0;
    if (!pSize && i == 0) {
      m_pEncodeInfo[i].sizes = pDict->GetIntegerBy("Size");
    }
    nTotalSampleBits *= m_pEncodeInfo[i].sizes;
    if (pEncode) {
      m_pEncodeInfo[i].encode_min = pEncode->GetFloatAt(i * 2);
      m_pEncodeInfo[i].encode_max = pEncode->GetFloatAt(i * 2 + 1);
    } else {
      m_pEncodeInfo[i].encode_min = 0;
      if (m_pEncodeInfo[i].sizes == 1) {
        m_pEncodeInfo[i].encode_max = 1;
      } else {
        m_pEncodeInfo[i].encode_max = (FX_FLOAT)m_pEncodeInfo[i].sizes - 1;
      }
    }
  }
  nTotalSampleBits *= m_nBitsPerSample;
  nTotalSampleBits *= m_nOutputs;
  FX_SAFE_DWORD nTotalSampleBytes = nTotalSampleBits;
  nTotalSampleBytes += 7;
  nTotalSampleBytes /= 8;
  if (!nTotalSampleBytes.IsValid() || nTotalSampleBytes.ValueOrDie() == 0 ||
      nTotalSampleBytes.ValueOrDie() > m_pSampleStream->GetSize()) {
    return FALSE;
  }
  m_pDecodeInfo = FX_Alloc(SampleDecodeInfo, m_nOutputs);
  for (int i = 0; i < m_nOutputs; i++) {
    if (pDecode) {
      m_pDecodeInfo[i].decode_min = pDecode->GetFloatAt(2 * i);
      m_pDecodeInfo[i].decode_max = pDecode->GetFloatAt(2 * i + 1);
    } else {
      m_pDecodeInfo[i].decode_min = m_pRanges[i * 2];
      m_pDecodeInfo[i].decode_max = m_pRanges[i * 2 + 1];
    }
  }
  return TRUE;
}
FX_BOOL CPDF_SampledFunc::v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const {
  int pos = 0;
  CFX_FixedBufGrow<FX_FLOAT, 16> encoded_input_buf(m_nInputs);
  FX_FLOAT* encoded_input = encoded_input_buf;
  CFX_FixedBufGrow<int, 32> int_buf(m_nInputs * 2);
  int* index = int_buf;
  int* blocksize = index + m_nInputs;
  for (int i = 0; i < m_nInputs; i++) {
    if (i == 0) {
      blocksize[i] = 1;
    } else {
      blocksize[i] = blocksize[i - 1] * m_pEncodeInfo[i - 1].sizes;
    }
    encoded_input[i] = PDF_Interpolate(
        inputs[i], m_pDomains[i * 2], m_pDomains[i * 2 + 1],
        m_pEncodeInfo[i].encode_min, m_pEncodeInfo[i].encode_max);
    index[i] = (int)encoded_input[i];
    if (index[i] < 0) {
      index[i] = 0;
    } else if (index[i] > m_pEncodeInfo[i].sizes - 1) {
      index[i] = m_pEncodeInfo[i].sizes - 1;
    }
    pos += index[i] * blocksize[i];
  }
  FX_SAFE_INT32 bits_to_output = m_nOutputs;
  bits_to_output *= m_nBitsPerSample;
  if (!bits_to_output.IsValid()) {
    return FALSE;
  }
  FX_SAFE_INT32 bitpos = pos;
  bitpos *= bits_to_output.ValueOrDie();
  if (!bitpos.IsValid()) {
    return FALSE;
  }
  FX_SAFE_INT32 range_check = bitpos;
  range_check += bits_to_output.ValueOrDie();
  if (!range_check.IsValid()) {
    return FALSE;
  }
  const uint8_t* pSampleData = m_pSampleStream->GetData();
  if (!pSampleData) {
    return FALSE;
  }
  for (int j = 0; j < m_nOutputs; j++) {
    FX_DWORD sample =
        _GetBits32(pSampleData, bitpos.ValueOrDie() + j * m_nBitsPerSample,
                   m_nBitsPerSample);
    FX_FLOAT encoded = (FX_FLOAT)sample;
    for (int i = 0; i < m_nInputs; i++) {
      if (index[i] == m_pEncodeInfo[i].sizes - 1) {
        if (index[i] == 0) {
          encoded = encoded_input[i] * (FX_FLOAT)sample;
        }
      } else {
        FX_SAFE_INT32 bitpos2 = blocksize[i];
        bitpos2 += pos;
        bitpos2 *= m_nOutputs;
        bitpos2 += j;
        bitpos2 *= m_nBitsPerSample;
        if (!bitpos2.IsValid()) {
          return FALSE;
        }
        FX_DWORD sample1 =
            _GetBits32(pSampleData, bitpos2.ValueOrDie(), m_nBitsPerSample);
        encoded += (encoded_input[i] - index[i]) *
                   ((FX_FLOAT)sample1 - (FX_FLOAT)sample);
      }
    }
    results[j] = PDF_Interpolate(encoded, 0, (FX_FLOAT)m_SampleMax,
                                 m_pDecodeInfo[j].decode_min,
                                 m_pDecodeInfo[j].decode_max);
  }
  return TRUE;
}

class CPDF_PSFunc : public CPDF_Function {
 public:
  // CPDF_Function
  FX_BOOL v_Init(CPDF_Object* pObj) override;
  FX_BOOL v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const override;

  CPDF_PSEngine m_PS;
};

FX_BOOL CPDF_PSFunc::v_Init(CPDF_Object* pObj) {
  CPDF_Stream* pStream = pObj->AsStream();
  CPDF_StreamAcc acc;
  acc.LoadAllData(pStream, FALSE);
  return m_PS.Parse((const FX_CHAR*)acc.GetData(), acc.GetSize());
}
FX_BOOL CPDF_PSFunc::v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const {
  CPDF_PSEngine& PS = (CPDF_PSEngine&)m_PS;
  PS.Reset();
  int i;
  for (i = 0; i < m_nInputs; i++) {
    PS.Push(inputs[i]);
  }
  PS.Execute();
  if (PS.GetStackSize() < m_nOutputs) {
    return FALSE;
  }
  for (i = 0; i < m_nOutputs; i++) {
    results[m_nOutputs - i - 1] = PS.Pop();
  }
  return TRUE;
}

class CPDF_ExpIntFunc : public CPDF_Function {
 public:
  CPDF_ExpIntFunc();
  ~CPDF_ExpIntFunc() override;

  // CPDF_Function
  FX_BOOL v_Init(CPDF_Object* pObj) override;
  FX_BOOL v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const override;

  FX_FLOAT m_Exponent;
  FX_FLOAT* m_pBeginValues;
  FX_FLOAT* m_pEndValues;
  int m_nOrigOutputs;
};

CPDF_ExpIntFunc::CPDF_ExpIntFunc() {
  m_pBeginValues = NULL;
  m_pEndValues = NULL;
}
CPDF_ExpIntFunc::~CPDF_ExpIntFunc() {
    FX_Free(m_pBeginValues);
    FX_Free(m_pEndValues);
}
FX_BOOL CPDF_ExpIntFunc::v_Init(CPDF_Object* pObj) {
  CPDF_Dictionary* pDict = pObj->GetDict();
  if (!pDict) {
    return FALSE;
  }
  CPDF_Array* pArray0 = pDict->GetArrayBy("C0");
  if (m_nOutputs == 0) {
    m_nOutputs = 1;
    if (pArray0) {
      m_nOutputs = pArray0->GetCount();
    }
  }
  CPDF_Array* pArray1 = pDict->GetArrayBy("C1");
  m_pBeginValues = FX_Alloc2D(FX_FLOAT, m_nOutputs, 2);
  m_pEndValues = FX_Alloc2D(FX_FLOAT, m_nOutputs, 2);
  for (int i = 0; i < m_nOutputs; i++) {
    m_pBeginValues[i] = pArray0 ? pArray0->GetFloatAt(i) : 0.0f;
    m_pEndValues[i] = pArray1 ? pArray1->GetFloatAt(i) : 1.0f;
  }
  m_Exponent = pDict->GetFloatBy("N");
  m_nOrigOutputs = m_nOutputs;
  if (m_nOutputs && m_nInputs > INT_MAX / m_nOutputs) {
    return FALSE;
  }
  m_nOutputs *= m_nInputs;
  return TRUE;
}
FX_BOOL CPDF_ExpIntFunc::v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const {
  for (int i = 0; i < m_nInputs; i++)
    for (int j = 0; j < m_nOrigOutputs; j++) {
      results[i * m_nOrigOutputs + j] =
          m_pBeginValues[j] +
          (FX_FLOAT)FXSYS_pow(inputs[i], m_Exponent) *
              (m_pEndValues[j] - m_pBeginValues[j]);
    }
  return TRUE;
}

class CPDF_StitchFunc : public CPDF_Function {
 public:
  CPDF_StitchFunc();
  ~CPDF_StitchFunc() override;

  // CPDF_Function
  FX_BOOL v_Init(CPDF_Object* pObj) override;
  FX_BOOL v_Call(FX_FLOAT* inputs, FX_FLOAT* results) const override;

  std::vector<CPDF_Function*> m_pSubFunctions;
  FX_FLOAT* m_pBounds;
  FX_FLOAT* m_pEncode;

  static const int kRequiredNumInputs = 1;
};

CPDF_StitchFunc::CPDF_StitchFunc() {
  m_pBounds = NULL;
  m_pEncode = NULL;
}
CPDF_StitchFunc::~CPDF_StitchFunc() {
  for (auto& sub : m_pSubFunctions) {
    delete sub;
  }
  FX_Free(m_pBounds);
  FX_Free(m_pEncode);
}
FX_BOOL CPDF_StitchFunc::v_Init(CPDF_Object* pObj) {
  CPDF_Dictionary* pDict = pObj->GetDict();
  if (!pDict) {
    return FALSE;
  }
  if (m_nInputs != kRequiredNumInputs) {
    return FALSE;
  }
  CPDF_Array* pArray = pDict->GetArrayBy("Functions");
  if (!pArray) {
    return FALSE;
  }
  FX_DWORD nSubs = pArray->GetCount();
  if (nSubs == 0) {
    return FALSE;
  }
  m_nOutputs = 0;
  for (FX_DWORD i = 0; i < nSubs; i++) {
    CPDF_Object* pSub = pArray->GetElementValue(i);
    if (pSub == pObj) {
      return FALSE;
    }
    std::unique_ptr<CPDF_Function> pFunc(CPDF_Function::Load(pSub));
    if (!pFunc) {
      return FALSE;
    }
    // Check that the input dimensionality is 1, and that all output
    // dimensionalities are the same.
    if (pFunc->CountInputs() != kRequiredNumInputs) {
      return FALSE;
    }
    if (pFunc->CountOutputs() != m_nOutputs) {
      if (m_nOutputs)
        return FALSE;

      m_nOutputs = pFunc->CountOutputs();
    }

    m_pSubFunctions.push_back(pFunc.release());
  }
  m_pBounds = FX_Alloc(FX_FLOAT, nSubs + 1);
  m_pBounds[0] = m_pDomains[0];
  pArray = pDict->GetArrayBy("Bounds");
  if (!pArray) {
    return FALSE;
  }
  for (FX_DWORD i = 0; i < nSubs - 1; i++) {
    m_pBounds[i + 1] = pArray->GetFloatAt(i);
  }
  m_pBounds[nSubs] = m_pDomains[1];
  m_pEncode = FX_Alloc2D(FX_FLOAT, nSubs, 2);
  pArray = pDict->GetArrayBy("Encode");
  if (!pArray) {
    return FALSE;
  }
  for (FX_DWORD i = 0; i < nSubs * 2; i++) {
    m_pEncode[i] = pArray->GetFloatAt(i);
  }
  return TRUE;
}
FX_BOOL CPDF_StitchFunc::v_Call(FX_FLOAT* inputs, FX_FLOAT* outputs) const {
  FX_FLOAT input = inputs[0];
  size_t i;
  for (i = 0; i < m_pSubFunctions.size() - 1; i++)
    if (input < m_pBounds[i + 1]) {
      break;
    }
  if (!m_pSubFunctions[i]) {
    return FALSE;
  }
  input = PDF_Interpolate(input, m_pBounds[i], m_pBounds[i + 1],
                          m_pEncode[i * 2], m_pEncode[i * 2 + 1]);
  int nresults;
  m_pSubFunctions[i]->Call(&input, kRequiredNumInputs, outputs, nresults);
  return TRUE;
}

}  // namespace

CPDF_Function* CPDF_Function::Load(CPDF_Object* pFuncObj) {
  if (!pFuncObj) {
    return NULL;
  }
  CPDF_Function* pFunc = NULL;
  int type;
  if (CPDF_Stream* pStream = pFuncObj->AsStream()) {
    type = pStream->GetDict()->GetIntegerBy("FunctionType");
  } else if (CPDF_Dictionary* pDict = pFuncObj->AsDictionary()) {
    type = pDict->GetIntegerBy("FunctionType");
  } else {
    return NULL;
  }
  if (type == 0) {
    pFunc = new CPDF_SampledFunc;
  } else if (type == 2) {
    pFunc = new CPDF_ExpIntFunc;
  } else if (type == 3) {
    pFunc = new CPDF_StitchFunc;
  } else if (type == 4) {
    pFunc = new CPDF_PSFunc;
  } else {
    return NULL;
  }
  if (!pFunc->Init(pFuncObj)) {
    delete pFunc;
    return NULL;
  }
  return pFunc;
}
CPDF_Function::CPDF_Function() {
  m_pDomains = NULL;
  m_pRanges = NULL;
}
CPDF_Function::~CPDF_Function() {
  FX_Free(m_pDomains);
  FX_Free(m_pRanges);
}
FX_BOOL CPDF_Function::Init(CPDF_Object* pObj) {
  CPDF_Stream* pStream = pObj->AsStream();
  CPDF_Dictionary* pDict = pStream ? pStream->GetDict() : pObj->AsDictionary();

  CPDF_Array* pDomains = pDict->GetArrayBy("Domain");
  if (!pDomains)
    return FALSE;

  m_nInputs = pDomains->GetCount() / 2;
  if (m_nInputs == 0)
    return FALSE;

  m_pDomains = FX_Alloc2D(FX_FLOAT, m_nInputs, 2);
  for (int i = 0; i < m_nInputs * 2; i++) {
    m_pDomains[i] = pDomains->GetFloatAt(i);
  }
  CPDF_Array* pRanges = pDict->GetArrayBy("Range");
  m_nOutputs = 0;
  if (pRanges) {
    m_nOutputs = pRanges->GetCount() / 2;
    m_pRanges = FX_Alloc2D(FX_FLOAT, m_nOutputs, 2);
    for (int i = 0; i < m_nOutputs * 2; i++) {
      m_pRanges[i] = pRanges->GetFloatAt(i);
    }
  }
  FX_DWORD old_outputs = m_nOutputs;
  if (!v_Init(pObj)) {
    return FALSE;
  }
  if (m_pRanges && m_nOutputs > (int)old_outputs) {
    m_pRanges = FX_Realloc(FX_FLOAT, m_pRanges, m_nOutputs * 2);
    if (m_pRanges) {
      FXSYS_memset(m_pRanges + (old_outputs * 2), 0,
                   sizeof(FX_FLOAT) * (m_nOutputs - old_outputs) * 2);
    }
  }
  return TRUE;
}
FX_BOOL CPDF_Function::Call(FX_FLOAT* inputs,
                            int ninputs,
                            FX_FLOAT* results,
                            int& nresults) const {
  if (m_nInputs != ninputs) {
    return FALSE;
  }
  nresults = m_nOutputs;
  for (int i = 0; i < m_nInputs; i++) {
    if (inputs[i] < m_pDomains[i * 2]) {
      inputs[i] = m_pDomains[i * 2];
    } else if (inputs[i] > m_pDomains[i * 2 + 1]) {
      inputs[i] = m_pDomains[i * 2] + 1;
    }
  }
  v_Call(inputs, results);
  if (m_pRanges) {
    for (int i = 0; i < m_nOutputs; i++) {
      if (results[i] < m_pRanges[i * 2]) {
        results[i] = m_pRanges[i * 2];
      } else if (results[i] > m_pRanges[i * 2 + 1]) {
        results[i] = m_pRanges[i * 2 + 1];
      }
    }
  }
  return TRUE;
}
