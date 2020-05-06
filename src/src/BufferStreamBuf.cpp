#include "stdafx.h"
#include <pagedfile/BufferStreamBuf.h>

namespace pagedfile {

BufferStreamBuf::BufferStreamBuf(char *beg, char *end) {
  if (beg && end) {
    setg(beg, beg, end);
    setp(beg, end);
  }
}

std::streambuf::pos_type BufferStreamBuf::seekpos(std::streambuf::pos_type pos,
  std::ios_base::openmode which) {
  if (which & std::ios_base::in) {
    setg(eback(), eback() + pos, egptr());
  }
  if (which & std::ios_base::out) {
    setp(pbase(), epptr());
    pbump((int)pos);
  }
  return pos;
}

std::streambuf::pos_type BufferStreamBuf::seekoff(std::streambuf::off_type off,
  std::ios_base::seekdir dir, std::ios_base::openmode which) {
  if (dir == std::ios_base::beg) {
    return seekpos(off, which);
  } else if (dir == std::ios_base::end) {
    return seekpos(egptr() - eback() + off, which);
  } else {
    std::streambuf::pos_type pos;
    if (which & std::ios_base::in) {
      pos = seekpos(gptr() - eback() + off, std::ios_base::in);
    }
    if (which & std::ios_base::out) {
      pos = seekpos(pptr() - pbase() + off, std::ios_base::out);
    }
    return pos;
  }
}

}  // namespace