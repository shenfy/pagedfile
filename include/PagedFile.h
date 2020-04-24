#ifndef PFAR_PAGEDFILE_H
#define PFAR_PAGEDFILE_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fstream>
#include <memory>
#include "BufferStreamBuf.h"

namespace pagedfile {

// Header Layout
// (uint32_t) num_pages
// -------page desc 0-------
// (uint32_t) index
// (uint64_t) start
// (uint64_t) length
// (uint16_t) format flags
// [uint64_t] uncompressed length
// (uint16_t) name_length
// (char[]) name
// -------page desc 1-------
// ...

class PagedFileHeader {
public:
  struct PageDesc {
    uint16_t format {0};
    uint64_t start {0};
    uint64_t length {0};
    uint64_t uncompressed_length {0};
    std::string name;
  };

  // build table from serialized source
  bool ParseFromStream(std::istream &s, std::istream::pos_type &tail_pos);
  void Clear();
  bool WriteToFile(std::fstream::pos_type tail_pos, std::fstream &fs);

  PageDesc *Desc(uint32_t idx);
  const PageDesc *Desc(uint32_t idx) const;

  void AddPage(uint32_t idx, const PageDesc &desc);
  void AddPage(uint32_t idx, PageDesc &&desc);

  // page attributes
  static bool IsCompressed(uint16_t format);
  bool PageLength(uint32_t page_idx, uint64_t &length, uint64_t &uncompressed_length) const;
  bool PageOffset(uint32_t page_idx, uint64_t &offset) const;
  const std::string &PageName(uint32_t page_idx) const;
  uint16_t PageFormat(uint32_t page_idx) const;

  // page list
  // return a vector of all page indices
  const std::vector<uint32_t> &ListPages() const;
  // generate a vector of all pages starting with a given prefix
  std::vector<uint32_t> ListPages(const std::string &prefix) const;

  bool Exists(uint32_t idx) const;

  // add meta page
  bool NewMetaPage(uint32_t idx, uint16_t format, const std::string &name);

  // dump
  void PrintPageTable();

  // grant PageFile class access to member variables
  friend class PagedFile;

private:
  std::unordered_map<uint32_t, PageDesc> page_table_;
  std::vector<uint32_t> page_order_;
};

class PagedFile {
public:

  PagedFile();
  ~PagedFile();

  enum { kReadOnly, kCreate, kReadWrite };
  // file type (least significant byte)
  enum { kFile = 0, kDirectory = 0x1, kSymLink = 0x2, kHardLink = 0x3 };
  // compression format (2nd least significant byte)
  enum { kPlain = 0, kLZ4Block = 0x1 << 8, kLZ4Frame = 0x2 << 8 };
  enum { kMagicNumber = 0x52414650 };  // ascii: PFAR

  bool Open(const char *fn, int32_t mode);
  void Close(bool save_update = false);

  // navigation
  bool GoToPage(uint32_t page);

  // high level I/O interface
  // read entire page
  uint64_t ReadPage(uint32_t idx, char *buffer, size_t buffer_size);
  bool AppendPage(uint32_t idx, const std::string &name, uint16_t format,
      const char *buffer, size_t length, bool verbose = false);

  // low level I/O interface
  // read
  void Read(void *buffer, size_t length);

  // write
  bool NewPage(uint32_t idx);
  bool NewPage(uint32_t idx, const std::string &name);
  void Write(const void *buffer, size_t length);
  void EndNewPage();

  bool NewMetaPage(uint32_t idx, uint16_t format, const std::string &name);

  bool RemovePages(const std::unordered_set<uint32_t> &pages);

  PagedFileHeader &Header();

  // inspection

  /**
   * @brief PageInputStream
   * @details A std::istream compliant stream to read content from a single page
   */
  struct PageInputStream : std::istream {
    // constructors
    PageInputStream();
    PageInputStream(std::shared_ptr<uint8_t> data, size_t size);
    PageInputStream(PageInputStream &&rhs);

  private:
    std::shared_ptr<uint8_t> data_;
    BufferStreamBuf buffer_;
  };

  PageInputStream CreatePageIStream(uint32_t idx);

  static uint16_t ChooseCompressionFormat(size_t length);

private:
  PagedFileHeader header_;

  void ResetForWriting();

  int32_t mode_;
  bool is_open_;
  int32_t editing_page_;

  std::fstream fs_;
  std::fstream::pos_type tail_pos_;
  std::fstream::pos_type old_tail_;

  std::vector<char> comp_buffer_;

  std::string filename_;
};

}  // namespace


#endif