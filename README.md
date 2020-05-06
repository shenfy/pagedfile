# pagedfile
Lightweight multi-file archive format and utilities.

## Dependencies
- C++17 compiler
- Boost >= 1.69.0
- lz4 >= 1.9.2 (github.com/lz4/lz4)

## Installation
### from Conan Bintray
The suggested way to use this library and command-line utility is through [Conan](https://conan.io)

Please add the following bintray to your conan remote:
```bash
$ conan remote add bintray-shenfy https://api.bintray.com/conan/shenfy/oss
```
And ask for the latest pagedfile in your project's conanfile.txt:
```
[requires]
pagedfile/1.3.0@shenfy/testing
```
or conanfile.py:
```python
requires = "pagedfile/1.3.0@shenfy/testing"
```

### Build from source w/ Conan
Clone the repository, then install dependencies with conan:
```bash
$ mkdir build && cd build
$ conan install ..
```
You can then either build the library to be exported to your local conan cache:
```bash
$ conan build ..
$ conan create .. (user)/(channel)
```
or build it with cmake and install to your system library folder (default to /usr/local)
```bash
$ cmake ../src
$ make -j4
$ make install
```

If you built with conan, you might want to manually copy the executable pfar to /usr/local/bin.

### Build from source w/o Conan
This is highly discouraged, but should work for now.
Please install boost (>=1.69.0) and liblz4 (>=1.9.2) first. Then
```bash
$ mkdir build
$ cd build
$ cmake ../src
$ make -j4
$ make install
```

## How to use the command line tool
### Create archive
pfar -a (ARCHIVE_NAME) (INPUT_FILES_AND_FOLDERS)

Example:
```bash
$ ls
1.txt  2.txt  3.txt
$ pfar -a test.pf *
Done.
```

### Inspect archive content
pfar -l (ARCHIVE_NAME)
```bash
$ pfar -l test.pf
1.txt   (11)
2.txt   (11)
3.txt   (11)
```

### Delete file from archive
pfar -d (ARCHIVE_NAME) (FILES_TO_DELETE)
```bash
$ pfar -d test.pf 2.txt 3.txt
$ pfar -l test.pf
1.txt   (11)
```

### Add file to archive
pfar -a (ARCHIVE_NAME) (INPUT_FILES_TO_ADD)
```bash
$ pfar -a test.pf 2.txt
$ pfar -l test.pf
1.txt   (11)
2.txt   (11)
```

### Unpack archive
pfar -x (ARCHIVE_NAME) [-o OUTPUT_PATH]
```bash
$ mkdir out
$ pfar -x test.pf -o out
Done.
$ ls out/
1.txt  2.txt
```
