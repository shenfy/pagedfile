#include "stdafx.h"
#include <pagedfile/PagedFile.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <lz4.h>
#include <lz4frame.h>
#include <boost/algorithm/string.hpp>

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID_API__)
#include <unistd.h>
#elif _WIN32
#include <io.h>
#include <errno.h>
#include <fcntl.h>
#endif

namespace {

bool TruncateFile(const char *fn, uint64_t length) {
#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID_API__)
  int ret = truncate(fn, length);
  return (ret == 0);
#elif _WIN32
  int fh = 0;
  if (_sopen_s(&fh, fn, _O_RDWR | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE) == 0) {
    int result = _chsize(fh, length);
    _close(fh);
    if (result == 0) {
      return true;
    }
  }
  return false;
#endif
}

}

namespace pagedfile {

bool PagedFileHeader::ParseFromStream(std::istream &s, std::istream::pos_type &tail_pos) {
  if (!s.good()) {
    return false;
  }

  // check magic number
  uint32_t magic_num = 0;
  s.seekg(0, std::ios::beg);
  s.read((char *)&magic_num, sizeof(uint32_t));
  if (magic_num != PagedFile::kMagicNumber) {
    return false;
  }

  page_table_.clear();
  page_order_.clear();

  // read page table length
  int64_t header_length = 0;
  s.seekg(-(int64_t)sizeof(int64_t), std::ios::end);
  s.read((char*)&header_length, sizeof(int64_t));

  // seek to beginning of page table
  s.seekg(-header_length - sizeof(int64_t), std::ios::end);
  tail_pos = s.tellg();

  // read num_pages
  uint32_t num_pages = 0;
  s.read((char *)&num_pages, sizeof(uint32_t));

  // read all page descs
  PageDesc page_desc;
  uint32_t idx = 0;
  page_order_.resize(num_pages);
  for (uint32_t i = 0; i < num_pages; ++i) {
    s.read((char *)&idx, sizeof(uint32_t));
    s.read((char *)&page_desc.start, sizeof(uint64_t));
    s.read((char *)&page_desc.length, sizeof(uint64_t));

    s.read((char *)&page_desc.format, sizeof(uint16_t));

    if (IsCompressed(page_desc.format)) {
      s.read((char *)&page_desc.uncompressed_length, sizeof(uint64_t));
    }

    uint16_t name_length = 0;
    s.read((char *)&name_length, sizeof(uint16_t));
    if (name_length != 0) {
      page_desc.name.resize(name_length);
      s.read(&page_desc.name[0], name_length);
    }
    page_table_[idx] = page_desc;
    page_order_[i] = idx;
  }

  return true;
}

void PagedFileHeader::Clear() {
  page_table_.clear();
  page_order_.clear();
}

bool PagedFileHeader::WriteToFile(std::fstream::pos_type tail_pos, std::fstream &fs) {
  if (!fs.good()) {
    return false;
  }

  fs.seekp(tail_pos);

  // write num_pages
  uint32_t num_pages = (uint32_t)page_table_.size();
  fs.write((char *)&num_pages, sizeof(uint32_t));

  // write page descs
  for (uint32_t page_idx : page_order_) {
    const auto &desc = page_table_[page_idx];

    fs.write((char *)&page_idx, sizeof(uint32_t));
    fs.write((char *)&desc.start, sizeof(uint64_t));
    fs.write((char *)&desc.length, sizeof(uint64_t));

    fs.write((char *)&desc.format, sizeof(uint16_t));
    if (IsCompressed(desc.format)) {
      fs.write((char *)&desc.uncompressed_length, sizeof(uint64_t));
    }

    uint16_t name_length = (uint16_t)desc.name.size();
    fs.write((char *)&name_length, sizeof(uint16_t));
    if (name_length != 0) {
      fs.write(desc.name.c_str(), name_length);
    }
  }

  auto header_length = fs.tellp() - tail_pos;

  // write page table length
  fs.write((char *)&header_length, sizeof(int64_t));
  return true;
}

bool PagedFileHeader::PageLength(
  uint32_t page_idx, uint64_t &length, uint64_t &uncompressed_length) const {

  auto iter = page_table_.find(page_idx);
  if (iter != page_table_.end()) {
    length = iter->second.length;
    uncompressed_length = iter->second.uncompressed_length;
    return true;
  }
  return false;
}

bool PagedFileHeader::PageOffset(uint32_t page_idx, uint64_t &offset) const {
  auto iter = page_table_.find(page_idx);
  if (iter != page_table_.end()) {
    offset = iter->second.start;
    return true;
  }
  return false;
}

const std::string &PagedFileHeader::PageName(uint32_t page_idx) const {
  auto iter = page_table_.find(page_idx);
  if (iter != page_table_.end()) {
    return iter->second.name;
  }
  static const std::string empty_str;
  return empty_str;
}

uint16_t PagedFileHeader::PageFormat(uint32_t page_idx) const {
  auto iter = page_table_.find(page_idx);
  if (iter != page_table_.end()) {
    return iter->second.format;
  }
  return 0;
}

PagedFileHeader::PageDesc *PagedFileHeader::Desc(uint32_t idx) {
  auto iter = page_table_.find(idx);
  if (iter != page_table_.end()) {
    return &iter->second;
  }
  return nullptr;
}

const PagedFileHeader::PageDesc *PagedFileHeader::Desc(uint32_t idx) const {
  auto iter = page_table_.find(idx);
  if (iter != page_table_.end()) {
    return &iter->second;
  }
  return nullptr;
}

bool PagedFileHeader::Exists(uint32_t idx) const {
  return page_table_.find(idx) != page_table_.end();
}

void PagedFileHeader::AddPage(uint32_t idx, const PageDesc &desc) {
  page_table_[idx] = desc;
  page_order_.push_back(idx);
}

void PagedFileHeader::AddPage(uint32_t idx, PageDesc &&desc) {
  page_table_[idx] = std::move(desc);
  page_order_.push_back(idx);
}

const std::vector<uint32_t> &PagedFileHeader::ListPages() const {
  return page_order_;
}

std::vector<uint32_t> PagedFileHeader::ListPages(const std::string &prefix) const {
  if (prefix.empty()) {
    return ListPages();
  }

  std::vector<uint32_t> result;
  for (const auto &kvp : page_table_) {
    const auto &desc = kvp.second;
    if (boost::starts_with(desc.name, prefix)) {
      result.push_back(kvp.first);
    }
  }
  return result;
}


bool PagedFileHeader::NewMetaPage(uint32_t idx, uint16_t format, const std::string &data) {
  auto iter = page_table_.find(idx);
  if (iter != page_table_.end()) {
    return false;
  }

  page_table_[idx] = {format, 0, 0, 0, data};
  page_order_.push_back(idx);
  return true;
}

bool PagedFileHeader::IsCompressed(uint16_t format) {
  return ((format >> 8) & 0xff) > 0;
}

void PagedFileHeader::PrintPageTable() {
  for (const auto &kvp : page_table_) {
    std::cout << kvp.first << ": " << kvp.second.start << "(" << kvp.second.length << ")\n";
  }
  std::cout << std::endl;
}


///////////////////////////////////////////////
PagedFile::PagedFile() :
  is_open_(false),
  editing_page_(-1),
  old_tail_(0) {
}

PagedFile::~PagedFile() {
  if (is_open_) {
    Close(false);  // close without saving
  }
}

bool PagedFile::Open(const char *fn, int32_t mode) {
  // refuse to open before closing the current file
  if (is_open_ || fn == nullptr) {
    return false;
  }

  mode_ = mode;
  if (mode == kReadOnly) {
    fs_.open(fn, std::ios::binary | std::ios::in);
  } else if (mode == kCreate) {
    fs_.open(fn, std::ios::binary | std::ios::out | std::ios::trunc);
  } else if (mode == kReadWrite) {
    fs_.open(fn, std::ios::binary | std::ios::in | std::ios::out);
  }

  if (fs_.good()) {
    is_open_ = true;
    if (mode == kReadOnly || mode == kReadWrite) {
      if (!header_.ParseFromStream(fs_, tail_pos_)) {
        fs_.close();
        is_open_ = false;
        return false;
      }
      editing_page_ = -1;
    } else if (mode == kCreate) {
      ResetForWriting();
    }
    old_tail_ = tail_pos_;  // record original length

    filename_ = fn;
    return true;
  }
  return false;
}

void PagedFile::Close(bool save_update) {
  if (!is_open_)
    return;

  if (!save_update || mode_ == kReadOnly) {
    fs_.close();
    is_open_ = false;
    return;
  }

  if (editing_page_ >= 0) {
    EndNewPage();
  }

  header_.WriteToFile(tail_pos_, fs_);
  auto file_length = fs_.tellp();
  fs_.close();

  // truncate file if necessary
  if (tail_pos_ < old_tail_) {
    TruncateFile(filename_.c_str(), file_length);
  }

  is_open_ = false;
  return;
}

void PagedFile::ResetForWriting() {
  header_.Clear();
  fs_.seekp(0);
  static const uint32_t magic_num = PagedFile::kMagicNumber;
  fs_.write((char *)&magic_num, sizeof(uint32_t));
  tail_pos_ = fs_.tellp();
  editing_page_ = -1;
}


bool PagedFile::GoToPage(uint32_t page) {
  if (!is_open_)
    return false;

  auto desc = header_.Desc(page);
  if (desc != nullptr) {
    // page does not contain data
    if ((desc->format & 0xff) != 0)
      return false;

    fs_.seekg(desc->start, std::ios::beg);
    fs_.seekp(desc->start, std::ios::beg);
    return true;
  }
  return false;
}

void PagedFile::Read(void *buffer, size_t length) {
  if (!is_open_ || editing_page_ >= 0)
    return;

  fs_.read((char*)buffer, length);
}

void PagedFile::Write(const void *buffer, size_t length) {
  if (!is_open_ || editing_page_ < 0)
    return;

  fs_.write((char*)buffer, length);
}

bool PagedFile::NewPage(uint32_t idx) {
  return NewPage(idx, "");
}

bool PagedFile::NewPage(uint32_t idx, const std::string &name) {
  if (!is_open_)
    return false;

  // check if page with the same idx already exists
  if (header_.Exists(idx)) {
    return false;
  }

  fs_.seekp(tail_pos_);
  uint64_t start = fs_.tellp();

  header_.AddPage(idx, {kFile | kPlain, start, 0, 0, name});
  editing_page_ = (int32_t)idx;
  return true;
}


void PagedFile::EndNewPage() {
  if (!is_open_ || editing_page_ < 0)
    return;

  tail_pos_ = fs_.tellp();
  uint64_t cur_pos = (uint64_t)tail_pos_;
  auto desc = header_.Desc((uint32_t)editing_page_);
  uint64_t offset = cur_pos - desc->start;

  desc->length = offset;

  editing_page_ = -1;
}

uint64_t PagedFile::ReadPage(uint32_t idx, char *buffer, size_t buffer_size) {
  if (!is_open_ || editing_page_ >= 0)
    return 0;

  if (!header_.Exists(idx)) {
    return 0;
  }

  const auto desc = header_.Desc(idx);
  if ((PagedFileHeader::IsCompressed(desc->format) && (buffer_size < desc->uncompressed_length))
    || buffer_size < desc->length) {
    return 0;
  }

  fs_.seekg(desc->start, std::ios::beg);

  if (PagedFileHeader::IsCompressed(desc->format)) {
    // resize work buffer
    if (comp_buffer_.size() < desc->length) {
      comp_buffer_.resize(desc->length);
    }
    fs_.read(&comp_buffer_[0], desc->length);
    // decompress
    if (desc->format & kLZ4Block) {
      int bytes = LZ4_decompress_safe(&comp_buffer_[0], buffer, desc->length, buffer_size);
      if (bytes < 0) {
        return 0;
      }

      return (uint64_t)bytes;
    } else if (desc->format & kLZ4Frame) {
      LZ4F_dctx* ctx = nullptr;
      auto err = LZ4F_createDecompressionContext(&ctx, LZ4F_VERSION);
      if (LZ4F_isError(err)) {
        // lz4 version mismatch? out of memory?
        return 0;
      }

      // set stable dst for lz4 internal optimization
      LZ4F_decompressOptions_t options;
      memset(&options, 0, sizeof options);
      options.stableDst = 1;

      // loop until src buffer is exhausted or frame end
      size_t dst_consumed = 0;
      size_t src_left = comp_buffer_.size();
      char *src_buffer = comp_buffer_.data();
      while (src_left > 0) {
        size_t dst_size = buffer_size - dst_consumed;
        size_t src_size = src_left;
        auto hint = LZ4F_decompress(ctx, buffer, &dst_size, src_buffer, &src_size, &options);

        if (LZ4F_isError(hint)) {
          // something went wrong, maybe data is corrupted
          return 0;
        }

        buffer += dst_size;
        dst_consumed += dst_size;
        src_buffer += src_size;
        src_left -= src_size;
      }

      err = LZ4F_freeDecompressionContext(ctx);
      if (LZ4F_isError(err)) {
        return 0;
      }

      return dst_consumed;
    } else {
      return 0;
    }
  } else {
    fs_.read(buffer, desc->length);
    return desc->length;
  }
}

bool PagedFile::AppendPage(uint32_t idx, const std::string &name, uint16_t format,
  const char *buffer, size_t length, bool verbose) {
  if (!is_open_ || editing_page_ >= 0)
    return false;

  // try compression first
  size_t bytes = 0;
  if (PagedFileHeader::IsCompressed(format)) {
    if (format & kLZ4Block) {
      int max_dst_size = LZ4_compressBound(length);
      if (comp_buffer_.size() < (size_t)max_dst_size) {
        comp_buffer_.resize(max_dst_size);
      }
      bytes = (size_t)LZ4_compress_default(buffer, &comp_buffer_[0], length, max_dst_size);
      if (bytes == 0) {  // compression failed
        return false;
      }
    } else if (format & kLZ4Frame) {
      size_t max_dst_size = LZ4F_compressFrameBound(length, nullptr);
      if (comp_buffer_.size() < max_dst_size) {
        comp_buffer_.resize(max_dst_size);
      }

      LZ4F_preferences_t pref = LZ4F_INIT_PREFERENCES;
      // record contentSize to prevent memory reallocation when using Python binding
      pref.frameInfo.contentSize = length;
      bytes = LZ4F_compressFrame(comp_buffer_.data(), comp_buffer_.size(), buffer, length, &pref);
      if (LZ4F_isError(bytes)) {
        return false;
      }
    } else {
      return false;
    }
  }

  if (bytes >= length) {
    format &= 0xffff00ff;  // clear comrpession flag
  }

  if (!NewPage(idx, name))
    return false;
  auto desc = header_.Desc(idx);
  desc->format = format;

  if (PagedFileHeader::IsCompressed(format)) {
    fs_.write(&comp_buffer_[0], bytes);
    desc->uncompressed_length = length;

    if (verbose) {
      std::cout << name << " [" << (int)((float)bytes / length * 100) << "%]" << std::endl;
    }
  } else {
    fs_.write((char *)buffer, length);
  }
  EndNewPage();

  return true;
}

bool PagedFile::NewMetaPage(uint32_t idx, uint16_t format, const std::string &data) {
  if (!is_open_ || editing_page_ >= 0) {
    return false;
  }
  return header_.NewMetaPage(idx, format, data);
}


bool PagedFile::RemovePages(const std::unordered_set<uint32_t> &pages) {

  uint64_t move_dst = 0;
  bool moving = false;

  std::vector<char> read_buffer;
  std::vector<uint32_t> new_order;

  auto old_order = header_.ListPages();

  for (uint32_t idx : old_order) {
    auto desc = header_.Desc(idx);
    if ((desc->format & 0xff) != kFile) {  // can only remove file
      new_order.push_back(idx);
      continue;
    }

    bool delete_page = (pages.find(idx) != pages.end());

    if (!moving) {  // not moving yet
      if (delete_page) {
        // remove page table entry
        header_.page_table_.erase(idx);

        // set moving head
        move_dst = desc->start;
        moving = true;
      } else {
        new_order.push_back(idx);
      }
    } else {
      if (delete_page) {
        header_.page_table_.erase(idx);
      } else {
        // move page
        // read to memory
        if (read_buffer.size() < desc->length) {
          read_buffer.resize(desc->length);
        }
        fs_.seekg(desc->start, std::ios::beg);
        fs_.read(&read_buffer[0], desc->length);
        // write to move_dst
        fs_.seekp(move_dst, std::ios::beg);
        fs_.write(&read_buffer[0], desc->length);
        // modify table entry
        desc->start = move_dst;
        new_order.push_back(idx);

        // push forward move head
        move_dst += desc->length;
      }
    }
  }

  using std::swap;
  swap(header_.page_order_, new_order);

  // set tail pos
  if (moving) {
    tail_pos_ = move_dst;
  }

  return true;
}

PagedFileHeader &PagedFile::Header() {
  return header_;
}

/////////////////////////////
// input stream related

PagedFile::PageInputStream::PageInputStream() :
  std::istream(nullptr), buffer_(nullptr, nullptr) {
}

PagedFile::PageInputStream::PageInputStream(std::shared_ptr<uint8_t> data, size_t size) :
  std::istream(nullptr),
  data_(data),
  buffer_((char*)data.get(), (char*)data.get() + size) {

  if (data && size) {
    init(&buffer_);
  }
}

PagedFile::PageInputStream::PageInputStream(PageInputStream &&rhs) :
  std::istream(std::move(rhs)), buffer_(nullptr, nullptr) {
  using std::swap;
  swap(data_, rhs.data_);
  swap(buffer_, rhs.buffer_);
  set_rdbuf(&buffer_);
}

PagedFile::PageInputStream PagedFile::CreatePageIStream(uint32_t idx) {
  uint64_t length = 0, uncompressed_length = 0;
  if (!header_.PageLength(idx, length, uncompressed_length) || length == 0)
    return {};

  uint16_t format = header_.PageFormat(idx);
  uint64_t data_length = length;
  if (PagedFileHeader::IsCompressed(format)) {
    data_length = uncompressed_length;
  }
  std::shared_ptr<uint8_t> data(new uint8_t[data_length], std::default_delete<uint8_t[]>());

  ReadPage(idx, (char*)data.get(), data_length);

  return PageInputStream(std::move(data), data_length);
}

uint16_t PagedFile::ChooseCompressionFormat(size_t length) {
  return length <= LZ4_MAX_INPUT_SIZE ? kLZ4Block : kLZ4Frame;
}

}
