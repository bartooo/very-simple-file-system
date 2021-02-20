# Very Simple File System

## Table of Contents

- [About](#about)
- [Usage](#usage)

## About <a name = "about"></a>

Project for SOI (Operating Systems) course at Warsaw University of Technology. Implemented in C language, realizing basic functions of file system, such as:

- creating virtual disk - `create_vfs()`
- copying file on virtual disk - `copy_on_vfs()`
- copying file from virtual disk - `copy_from_vfs()`
- showing folder of virtual disk - `list_vfs()`
- removing file from virtual disk - `remove_from_vfs()`
- removing virtual disk - `remove_vfs()`
- mapping usage of virtual disk as list with informations about address of block, type of block, size of block, busy/free - `map_vfs()`

### Structure of file system
- Virtual disk contains blocks of size 4096B
- First block in virtual disk is called Superblock and contains informations about:
    - Number of free data blocks
    - Number of free iNode slots
    - Offset to iNode blocks bitmap
    - Offset to data blocks bitmap
    - Offset to array of iNodes
    - Offset to data blocks
- Next two blocks are bitmaps of iNode slots and data blocks
- For iNode structures 5 blocks are allocated
- For data blocks 56 blocks are allocated
## Usage <a name = "usage"></a>

In `main()` function many operations are performed to check if they are completed successfully. Main goal of testing was to see how file system handles with fragmentation, check if virtual disk is correctly created and removed, and to see if operations on files are performed correctly.

After every operation folder of virtual disk is listed and information about current state of virtual disk are printed out.

To run tests:
```
gcc -o main main.c
```
```
./main
```