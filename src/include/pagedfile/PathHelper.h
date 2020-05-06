#ifndef PATHHELPER_H
#define PATHHELPER_H

#include <vector>
#include <string>

namespace pagedfile { namespace path {

std::string Join(const std::string &prefix, const std::string &suffix);

bool Exists(const char *filename);

}}

#endif
