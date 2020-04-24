# pagedfile
Lightweight multi-file archive format and utilities.

## Dependencies
- C++17 compiler
- Boost >= 1.69.0
- lz4 >= 1.9.1 (github.com/lz4/lz4)

## How to build
```bash
$ mkdir build
$ cd build
$ cmake ..
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
