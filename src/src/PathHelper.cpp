#include "stdafx.h"
#include <pagedfile/PathHelper.h>
#include <sstream>
#include <fstream>
#include <boost/algorithm/string.hpp>

namespace pagedfile { namespace path {

std::string Join(const std::string &prefix, const std::string &suffix) {
    static auto is_slash = [](const char c) { return c == '\\' || c == '/'; };
    std::string prefix_new = boost::algorithm::trim_right_copy_if(prefix, is_slash);
    std::string suffix_new = boost::algorithm::trim_left_copy_if(suffix, is_slash);
    if (prefix_new.size() == 0)
        return suffix_new;
    if (suffix_new.size() == 0)
        return prefix_new;
    std::stringstream ss;
    ss << prefix_new << "/" << suffix_new;
    return ss.str();
}

bool Exists(const char *filename) {
    std::ifstream in(filename);
    bool result = in.good();
    in.close();
    return result;
}

}}
