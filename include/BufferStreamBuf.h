#ifndef BUFFERSTREAMBUF_H
#define BUFFERSTREAMBUF_H

#include <streambuf>

namespace pagedfile {

class BufferStreamBuf : public std::streambuf {
public:
  BufferStreamBuf(char *beg, char *end);

protected:
  virtual std::streambuf::pos_type seekpos(std::streambuf::pos_type pos,
    std::ios_base::openmode which = std::ios_base::in | std::ios_base::out);
  virtual std::streambuf::pos_type seekoff(std::streambuf::off_type off,
    std::ios_base::seekdir dir,
    std::ios_base::openmode which = std::ios_base::in | std::ios_base::out);
};

}  // namespace

#endif