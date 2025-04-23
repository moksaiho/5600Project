# File System Implementation

Collaborated by Mu Xihe, Liuyi Yang and Wang Yange


## Compilation Instructions
Use the provided Makefile to compile the project:

```
make
```

This will generate the executable file `filesys`.

## Execution Instructions
Run the program with:

```
./filesys
```

## Features
The file system implementation supports the following operations:
- Directory operations: mkdir, cd, home, rmdir, ls
- File operations: create, append, cat, tail, rm
- Statistics: stat (displays information about files/directories)

## Implementation Details
This program implements a simple file system with:
- Block-based storage architecture
- Hierarchical directory structure
- File operations with inode-based file management
- Error handling for various edge cases

## Testing
Complete testing has been performed on all file and directory operations.
