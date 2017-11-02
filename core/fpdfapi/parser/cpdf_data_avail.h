// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_DATA_AVAIL_H_
#define CORE_FPDFAPI_PARSER_CPDF_DATA_AVAIL_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/cpdf_syntax_parser.h"
#include "core/fxcrt/unowned_ptr.h"

class CPDF_CrossRefAvail;
class CPDF_Dictionary;
class CPDF_HintTables;
class CPDF_IndirectObjectHolder;
class CPDF_LinearizedHeader;
class CPDF_PageObjectAvail;
class CPDF_Parser;
class CPDF_ReadValidator;

enum PDF_DATAAVAIL_STATUS {
  PDF_DATAAVAIL_HEADER = 0,
  PDF_DATAAVAIL_FIRSTPAGE,
  PDF_DATAAVAIL_HINTTABLE,
  PDF_DATAAVAIL_LOADALLCROSSREF,
  PDF_DATAAVAIL_ROOT,
  PDF_DATAAVAIL_INFO,
  PDF_DATAAVAIL_PAGETREE,
  PDF_DATAAVAIL_PAGE,
  PDF_DATAAVAIL_PAGE_LATERLOAD,
  PDF_DATAAVAIL_RESOURCES,
  PDF_DATAAVAIL_DONE,
  PDF_DATAAVAIL_ERROR,
  PDF_DATAAVAIL_LOADALLFILE,
};

enum PDF_PAGENODE_TYPE {
  PDF_PAGENODE_UNKNOWN = 0,
  PDF_PAGENODE_PAGE,
  PDF_PAGENODE_PAGES,
  PDF_PAGENODE_ARRAY,
};

class CPDF_DataAvail final {
 public:
  // Must match PDF_DATA_* definitions in public/fpdf_dataavail.h, but cannot
  // #include that header. fpdfsdk/fpdf_dataavail.cpp has static_asserts
  // to make sure the two sets of values match.
  enum DocAvailStatus {
    DataError = -1,        // PDF_DATA_ERROR
    DataNotAvailable = 0,  // PDF_DATA_NOTAVAIL
    DataAvailable = 1,     // PDF_DATA_AVAIL
  };

  // Must match PDF_*LINEAR* definitions in public/fpdf_dataavail.h, but cannot
  // #include that header. fpdfsdk/fpdf_dataavail.cpp has static_asserts
  // to make sure the two sets of values match.
  enum DocLinearizationStatus {
    LinearizationUnknown = -1,  // PDF_LINEARIZATION_UNKNOWN
    NotLinearized = 0,          // PDF_NOT_LINEARIZED
    Linearized = 1,             // PDF_LINEARIZED
  };

  // Must match PDF_FORM_* definitions in public/fpdf_dataavail.h, but cannot
  // #include that header. fpdfsdk/fpdf_dataavail.cpp has static_asserts
  // to make sure the two sets of values match.
  enum DocFormStatus {
    FormError = -1,        // PDF_FORM_ERROR
    FormNotAvailable = 0,  // PDF_FORM_NOTAVAIL
    FormAvailable = 1,     // PDF_FORM_AVAIL
    FormNotExist = 2,      // PDF_FORM_NOTEXIST
  };

  class FileAvail {
   public:
    virtual ~FileAvail();
    virtual bool IsDataAvail(FX_FILESIZE offset, size_t size) = 0;
  };

  class DownloadHints {
   public:
    virtual ~DownloadHints();
    virtual void AddSegment(FX_FILESIZE offset, size_t size) = 0;
  };

  CPDF_DataAvail(FileAvail* pFileAvail,
                 const RetainPtr<IFX_SeekableReadStream>& pFileRead,
                 bool bSupportHintTable);
  ~CPDF_DataAvail();

  DocAvailStatus IsDocAvail(DownloadHints* pHints);
  void SetDocument(CPDF_Document* pDoc);
  DocAvailStatus IsPageAvail(uint32_t dwPage, DownloadHints* pHints);
  DocFormStatus IsFormAvail(DownloadHints* pHints);
  DocLinearizationStatus IsLinearizedPDF();
  RetainPtr<IFX_SeekableReadStream> GetFileRead() const;
  int GetPageCount() const;
  CPDF_Dictionary* GetPage(int index);
  RetainPtr<CPDF_ReadValidator> GetValidator() const;

  const CPDF_HintTables* GetHintTables() const { return m_pHintTables.get(); }

 protected:
  class PageNode {
   public:
    PageNode();
    ~PageNode();

    PDF_PAGENODE_TYPE m_type;
    uint32_t m_dwPageNo;
    std::vector<std::unique_ptr<PageNode>> m_ChildNodes;
  };

  static const int kMaxPageRecursionDepth = 1024;

  bool CheckDocStatus();
  bool CheckHeader();
  bool CheckFirstPage();
  bool CheckHintTables();
  bool CheckRoot();
  bool CheckInfo();
  bool CheckPages();
  bool CheckPage();
  DocAvailStatus CheckResources(const CPDF_Dictionary* page);
  DocFormStatus CheckAcroForm();
  bool CheckPageStatus();

  DocAvailStatus CheckHeaderAndLinearized();
  void SetStartOffset(FX_FILESIZE dwOffset);
  bool GetNextToken(ByteString* token);
  bool GetNextChar(uint8_t& ch);
  std::unique_ptr<CPDF_Object> ParseIndirectObjectAt(
      FX_FILESIZE pos,
      uint32_t objnum,
      CPDF_IndirectObjectHolder* pObjList = nullptr);
  std::unique_ptr<CPDF_Object> GetObject(uint32_t objnum,
                                         bool* pExistInFile);
  bool GetPageKids(CPDF_Parser* pParser, CPDF_Object* pPages);
  bool PreparePageItem();
  bool LoadPages();
  bool CheckAndLoadAllXref();
  bool LoadAllFile();
  DocAvailStatus CheckLinearizedData();

  bool CheckPage(uint32_t dwPage);
  bool LoadDocPages();
  bool LoadDocPage(uint32_t dwPage);
  bool CheckPageNode(const PageNode& pageNode,
                     int32_t iPage,
                     int32_t& iCount,
                     int level);
  bool CheckUnknownPageNode(uint32_t dwPageNo, PageNode* pPageNode);
  bool CheckArrayPageNode(uint32_t dwPageNo, PageNode* pPageNode);
  bool CheckPageCount();
  bool IsFirstCheck(uint32_t dwPage);
  void ResetFirstCheck(uint32_t dwPage);
  bool ValidatePage(uint32_t dwPage);
  CPDF_SyntaxParser* GetSyntaxParser() const;

  FileAvail* const m_pFileAvail;
  RetainPtr<CPDF_ReadValidator> m_pFileRead;
  CPDF_Parser m_parser;
  std::unique_ptr<CPDF_Object> m_pRoot;
  uint32_t m_dwRootObjNum;
  uint32_t m_dwInfoObjNum;
  std::unique_ptr<CPDF_LinearizedHeader> m_pLinearized;
  UnownedPtr<CPDF_Object> m_pTrailer;
  bool m_bDocAvail;
  std::unique_ptr<CPDF_CrossRefAvail> m_pCrossRefAvail;
  PDF_DATAAVAIL_STATUS m_docStatus;
  FX_FILESIZE m_dwFileLen;
  CPDF_Document* m_pDocument;
  FX_FILESIZE m_Pos;
  FX_FILESIZE m_bufferOffset;
  uint32_t m_bufferSize;
  ByteString m_WordBuf;
  uint8_t m_bufferData[512];
  std::vector<uint32_t> m_XRefStreamList;
  std::vector<uint32_t> m_PageObjList;
  uint32_t m_PagesObjNum;
  bool m_bLinearedDataOK;
  bool m_bMainXRefLoadTried;
  bool m_bMainXRefLoadedOK;
  bool m_bPagesTreeLoad;
  bool m_bPagesLoad;
  CPDF_Parser* m_pCurrentParser;
  std::unique_ptr<CPDF_PageObjectAvail> m_pFormAvail;
  std::vector<std::unique_ptr<CPDF_Object>> m_PagesArray;
  uint32_t m_dwEncryptObjNum;
  bool m_bTotalLoadPageTree;
  bool m_bCurPageDictLoadOK;
  PageNode m_PageNode;
  std::set<uint32_t> m_pageMapCheckState;
  std::set<uint32_t> m_pagesLoadState;
  std::set<uint32_t> m_SeenPrevPositions;
  std::unique_ptr<CPDF_HintTables> m_pHintTables;
  bool m_bSupportHintTable;
  std::map<uint32_t, std::unique_ptr<CPDF_PageObjectAvail>> m_PagesObjAvail;
  std::map<const CPDF_Object*, std::unique_ptr<CPDF_PageObjectAvail>>
      m_PagesResourcesAvail;
  bool m_bHeaderAvail;
};

#endif  // CORE_FPDFAPI_PARSER_CPDF_DATA_AVAIL_H_
