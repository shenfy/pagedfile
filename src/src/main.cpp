#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <pagedfile/PagedFile.h>
#include "version.h"

using namespace pagedfile;
namespace po = boost::program_options;
namespace fs = std::filesystem;

class PFArchiver {
public:

  struct FileEntry {
    std::string absolute_path;
    std::string relative_path;
    int type {PagedFile::kFile};
  };

  void SetProgramOptions(po::variables_map vm) {
    vm_ = std::move(vm);
  }

  int Pack() {
    auto archive_fn = vm_["archive"].as<std::string>();
    fs::path archive_path(archive_fn);

    if (!vm_.count("input-files")) {
      std::cerr << "Error: no input files specified!" << std::endl;
      return 1;
    }

    // get all file and dir names
    auto &cli_fns = vm_["input-files"].as<std::vector<std::string>>();
    if (cli_fns.size() == 0) {
      std::cerr << "Error: no input files specified!" << std::endl;
      return 1;
    }

    bool recurse = vm_["recurse"].as<bool>();

    std::vector<FileEntry> filenames;
    for (const std::string &fn : cli_fns) {
      fs::path p = fs::canonical(fn);
      if (fs::exists(p)) {
        CollectFiles(p, recurse, filenames);
      } else {
        std::cerr << "Error: " << fn << " not found!" << std::endl;
        continue;
      }
    }

    // check if pf already exists
    bool appending = false;
    int32_t open_mode = PagedFile::kCreate;
    uint32_t idx_shift = 0;
    if (fs::exists(archive_path)) {
      appending = true;
      open_mode = PagedFile::kReadWrite;
    }

    // do actual packing
    PagedFile pf;
    if (!pf.Open(archive_fn.c_str(), open_mode)) {
      std::cerr << "Error: failed to open archive file!" << std::endl;
      return 1;
    }

    if (appending) {
      std::vector<uint32_t> indices = pf.Header().ListPages();
      if (indices.size() != 0) {
        auto max_iter = std::max_element(indices.begin(), indices.end());
        idx_shift = *max_iter + 1;
      }
    }

    std::ifstream infile;
    std::vector<char> input_buffer;

    bool print = vm_["verbose"].as<bool>();
    bool compress = vm_["compress"].as<bool>();

    for (uint32_t idx = 0; idx < filenames.size(); ++idx) {
      auto &entry = filenames[idx];
      uint32_t new_idx = idx + idx_shift;

      if (entry.type == PagedFile::kDirectory) {
        // is directory
        if (print) {
          std::cout << entry.absolute_path << " [dir]" << std::endl;
        }
        pf.NewMetaPage(new_idx, PagedFile::kDirectory, entry.relative_path);

      } else if (entry.type == PagedFile::kFile) {
        // is file
        infile.open(entry.absolute_path, std::ios::binary);
        if (!infile.good())
          continue;

        // check input file length and resize buffer
        infile.seekg(0, std::ios::end);
        uint64_t input_length = infile.tellg();
        if (input_length > input_buffer.size()) {
          input_buffer.resize(input_length);
        }

        // read content to buffer
        infile.seekg(0, std::ios::beg);
        infile.read(&input_buffer[0], input_length);
        infile.close();

        auto relative_path = entry.relative_path;
        std::replace(relative_path.begin(), relative_path.end(), '\\', '/');
        // write input buffer to a page
        if (print) {
          std::cout << entry.absolute_path << std::endl;
        }

        if (compress) {
          auto format = PagedFile::ChooseCompressionFormat(input_length);
          pf.AppendPage(new_idx, relative_path, format | PagedFile::kFile,
            &input_buffer[0], input_length, print);
        } else {
          pf.NewPage(new_idx, relative_path);
          pf.Write(&input_buffer[0], input_length);
          pf.EndNewPage();
        }
      }
    }

    pf.Close(true);
    std::cout << "Done." << std::endl;

    return 0;
  }

  int Unpack() {
    // input file
    auto archive_fn = vm_["extract"].as<std::string>();
    fs::path archive_path(archive_fn);
    if (!fs::exists(archive_path) || !fs::is_regular_file(archive_path)) {
      std::cerr << "Error: archive does not exist!" << std::endl;
      return 1;
    }

    // output folders
    fs::path output_base(vm_["output"].as<std::string>());

    // create if output path does not exist
    try {
      if (fs::exists(output_base)) {
        if (!fs::is_directory(output_base)) {
          std::cerr << "Error: output path is a not a directory!" << std::endl;
          return 1;
        }
      } else {
        if (!create_directories(output_base)) {
          std::cerr << "Error: failed to create output directory!" << std::endl;
          return 1;
        }
      }
    } catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
      return 1;
    }

    output_base = fs::canonical(output_base);

    bool print = vm_["verbose"].as<bool>();

    PagedFile pf;
    if (!pf.Open(archive_fn.c_str(), PagedFile::kReadOnly)) {
      std::cerr << "Error: failed to load paged file. Corrupted?" << std::endl;
      return 1;
    }

    std::string prefix;
    if (vm_.count("prefix")) {
      prefix = vm_["prefix"].as<std::string>();
    }

    auto index_list = pf.Header().ListPages(prefix);

    // create all folders
    fs::path output_path;
    for (uint32_t idx : index_list) {
      uint16_t format = pf.Header().PageFormat(idx);
      if ((format & 0xff) != PagedFile::kDirectory)
        continue;

      output_path = output_base / pf.Header().PageName(idx);
      if (fs::exists(output_path) && fs::is_directory(output_path))
        continue;

      if (print) {
        std::cout << "folder: " << output_path << std::endl;
      }
      if (!create_directories(output_path)) {
        std::cerr << "Error: failed to create " << output_path.string() << std::endl;
        break;
      }
    }

    // extract files
    uint64_t page_length = 0, uncompressed_length = 0;
    std::ofstream outfile;
    std::vector<char> input_buffer;

    for (uint32_t idx : index_list) {
      uint16_t format = pf.Header().PageFormat(idx);
      if ((format & 0xff) != PagedFile::kFile)
        continue;

      output_path = output_base / pf.Header().PageName(idx);
      pf.Header().PageLength(idx, page_length, uncompressed_length);

      if (print) {
        std::cout << "extract file: " << output_path << std::endl;
      }

      size_t output_length = 0;

      // read file content
      if (PagedFileHeader::IsCompressed(format)) {
        auto max_size = std::max(uncompressed_length, page_length);
        if (max_size > input_buffer.size()) {
          input_buffer.resize(max_size);
        }
        pf.ReadPage(idx, &input_buffer[0], max_size);
        output_length = uncompressed_length;
      } else {
        if (page_length > input_buffer.size()) {
          input_buffer.resize(page_length);
        }
        pf.GoToPage(idx);
        pf.Read(&input_buffer[0], page_length);
        output_length = page_length;
      }

      outfile.open(output_path.string().c_str(), std::ios::binary);
      if (!outfile.good()) {
        std::cerr << "Error: failed to write to " << output_path.string() << std::endl;
        continue;
      }
      outfile.write(&input_buffer[0], output_length);
      outfile.close();
    }

    pf.Close();
    std::cout << "Done." << std::endl;

    return 0;
  }

  int List() {
    auto archive_fn = vm_["list"].as<std::string>();
    fs::path archive_path(archive_fn);
    if (!fs::exists(archive_path) || !fs::is_regular_file(archive_path)) {
      std::cerr << "Error: archive does not exist!" << std::endl;
      return 1;
    }

    PagedFile pf;
    if (!pf.Open(archive_fn.c_str(), PagedFile::kReadOnly)) {
      std::cerr << "Error: failed to load paged file. Corrupted?" << std::endl;
      return 1;
    }

    std::string prefix;
    if (vm_.count("prefix")) {
      prefix = vm_["prefix"].as<std::string>();
    }

    auto index_list = pf.Header().ListPages(prefix);
    for (uint32_t idx : index_list) {
      std::cout << pf.Header().PageName(idx);
      uint16_t format = pf.Header().PageFormat(idx);
      if ((format & 0xff) == PagedFile::kDirectory) {
        std::cout << " [dir]";
      } else if ((format & 0xff) == PagedFile::kFile) {
        uint64_t length = 0, uncompressed_length = 0;
        pf.Header().PageLength(idx, length, uncompressed_length);
        std::cout << "\t(" << length;
        if (PagedFileHeader::IsCompressed(format)) {
          std::cout << "/" << uncompressed_length << " "
            << (int)((float)length / uncompressed_length * 100) << "%";
        }
        std::cout << ")";
      }
      std::cout << std::endl;
    }

    pf.Close();

    return 0;
  }

  int Delete() {
    auto archive_fn = vm_["delete"].as<std::string>();
    fs::path archive_path(archive_fn);
    if (!fs::exists(archive_path) || !fs::is_regular_file(archive_path)) {
      std::cerr << "Error: archive does not exist!" << std::endl;
      return 1;
    }

    // get filenames to delete
    auto &cli_fns = vm_["input-files"].as<std::vector<std::string>>();
    if (cli_fns.size() == 0) {
      std::cerr << "Error: please specify filenames to delete!" << std::endl;
      return 1;
    }
    std::unordered_set<std::string> fn_set;
    fn_set.insert(cli_fns.begin(), cli_fns.end());

    // open file to manipulate content
    PagedFile pf;
    if (!pf.Open(archive_fn.c_str(), PagedFile::kReadWrite)) {
      std::cerr << "Error: failed to load paged file. Corrupted?" << std::endl;
      return 1;
    }

    auto index_list = pf.Header().ListPages();
    std::unordered_set<uint32_t> delete_indices;

    for (uint32_t idx : index_list) {
      if (fn_set.find(pf.Header().PageName(idx)) != fn_set.end()) {
        delete_indices.insert(idx);
      }
    }

    if (delete_indices.size() != 0) {
      pf.RemovePages(delete_indices);
    }

    pf.Close(true);
    return 0;
  }


private:
  po::variables_map vm_;

  void CollectFiles(const fs::path &p, bool recurse, std::vector<FileEntry> &filenames) {
    if (fs::is_regular_file(p)) {
      // remove path and only keeps filename
      filenames.push_back({p.string(), p.filename().string(), PagedFile::kFile});
    } else if (fs::is_directory(p)) {
      fs::path base = p.has_parent_path() ? p.parent_path() : p;

      if (recurse) {
        ExpandDir(p, base, filenames);
      } else {
        filenames.push_back({p.string(),
          p.lexically_relative(base).string(), PagedFile::kDirectory});
      }
    }
  }

  void ExpandDir(const fs::path &current, const fs::path &base,
    std::vector<FileEntry> &filenames) {

    // add dir entry
    filenames.push_back({current.string(),
      current.lexically_relative(base).string(),
      PagedFile::kDirectory});

    for (const auto &p : fs::directory_iterator(current)) {
      if (fs::is_regular_file(p.path())) {
        fs::path relative = p.path().lexically_relative(base);
        filenames.push_back({p.path().string(), relative.string(), PagedFile::kFile});
      } else if (fs::is_directory(p.path())) {
        ExpandDir(p.path(), base, filenames);
      }
    }
  }

};

int main(int argc, char *argv[]) {

  // parse command line options
  po::options_description actions("Actions");
  actions.add_options()
    ("version", "print version")
    ("help,h", "print help message")
    ("archive,a", po::value<std::string>()->value_name("ARCHIVE_PATH"), "create archive")
    ("extract,x", po::value<std::string>()->value_name("ARCHIVE_PATH"), "unpack archive")
    ("list,l", po::value<std::string>()->value_name("ARCHIVE_PATH"), "list files/dirs in pf")
    ("delete,d", po::value<std::string>()->value_name("ARCHIVE_PATH"), "delete files from pf");

  po::options_description config("Configuration");
  config.add_options()
    ("compress,z", po::bool_switch(), "compress file contents with LZ4")
    ("recurse,r", po::bool_switch(), "recursively add files in subdirectories")
    ("output,o",
      po::value<std::string>()->default_value(".")->value_name("OUTPUT_PATH"), "output path")
    ("verbose,v", po::bool_switch(), "print details")
    ("prefix", po::value<std::string>()->value_name("PATH_PREFIX"), "prefix to query");

  po::options_description hidden("Hidden");
  hidden.add_options()
    ("input-files,i", po::value<std::vector<std::string>>(), "input files");

  po::positional_options_description pd;
  pd.add("input-files", -1);

  po::options_description cli, visible;
  cli.add(actions).add(config).add(hidden);
  visible.add(actions).add(config);

  po::variables_map vm;

  try {
    po::store(po::command_line_parser(argc, argv)
      .options(cli).positional(pd).run(), vm);
    po::notify(vm);
  } catch (std::exception &e) {
    std::cerr << "Error: " <<  e.what() << std::endl;
    return 1;
  }

  // handle simple command line actions
  if (vm.count("help")) {
    std::cout << visible << std::endl;
    return 0;
  }

  if (vm.count("version")) {
    std::cout << "pfar, PagedFile Archiver/Unarchiver, version " CMAKE_PROJECT_VERSION << std::endl;
    return 0;
  }

  if (vm.count("archive") && vm.count("x")) {
    std::cerr << "Error: archive(a) and extract(x) called simultaneously!" << std::endl;
    return -1;
  }

  PFArchiver ar;
  if (vm.count("archive")) {
    ar.SetProgramOptions(std::move(vm));
    return ar.Pack();
  } else if (vm.count("extract")) {
    ar.SetProgramOptions(std::move(vm));
    return ar.Unpack();
  } else if (vm.count("list")) {
    ar.SetProgramOptions(std::move(vm));
    return ar.List();
  } else if (vm.count("delete")) {
    ar.SetProgramOptions(std::move(vm));
    return ar.Delete();
  }

  std::cout << "No action requested!" << std::endl << visible << std::endl;
  return 0;
}