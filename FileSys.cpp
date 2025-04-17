// Computing Systems: File System
// Implements the file system commands that are available to the shell.

#include <cstring>
#include <iostream>
using namespace std;

#include "FileSys.h"
#include "BasicFileSys.h"
#include "Blocks.h"

// mounts the file system
void FileSys::mount() {
  bfs.mount();
  curr_dir = 1;
}

// unmounts the file system
void FileSys::unmount() {
  bfs.unmount();
}

// Helper function to check if a block is a directory
bool FileSys::is_directory(short block_num) {
  struct dirblock_t block;
  bfs.read_block(block_num, (void *) &block);
  return block.magic == DIR_MAGIC_NUM;
}

// Helper function to find a file by name in the current directory
// Returns the block number of the file or 0 if not found
// Sets is_dir to true if the found file is a directory
short FileSys::find_file(const char *name, bool &is_dir) {
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void *) &dir_block);
  
  for (int i = 0; i < dir_block.num_entries; i++) {
    if (strcmp(dir_block.dir_entries[i].name, name) == 0) {
      is_dir = is_directory(dir_block.dir_entries[i].block_num);
      return dir_block.dir_entries[i].block_num;
    }
  }
  return 0; // File not found
}

// Helper function to check if filename is valid (not too long)
bool FileSys::check_filename(const char *name) {
  return strlen(name) <= MAX_FNAME_SIZE;
}

// Helper function to reclaim blocks used by a file or directory
void FileSys::reclaim_blocks(short block_num, bool is_dir) {
  if (is_dir) {
    // Directory blocks are single blocks, just reclaim it
    bfs.reclaim_block(block_num);
  } else {
    // For data files, need to reclaim inode and all data blocks
    struct inode_t inode;
    bfs.read_block(block_num, (void *) &inode);
    
    // Reclaim all data blocks first
    for (int i = 0; i < MAX_DATA_BLOCKS; i++) {
      if (inode.blocks[i] != 0) {
        bfs.reclaim_block(inode.blocks[i]);
      }
    }
    
    // Finally reclaim the inode block
    bfs.reclaim_block(block_num);
  }
}

// make a directory
void FileSys::mkdir(const char *name)
{
  // Check if filename is too long
  if (!check_filename(name)) {
    cout << "File name is too long" << endl;
    return;
  }
  
  // Check if file already exists
  bool is_dir;
  if (find_file(name, is_dir) != 0) {
    cout << "File exists" << endl;
    return;
  }
  
  // Check if directory is full
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void *) &dir_block);
  if (dir_block.num_entries >= MAX_DIR_ENTRIES) {
    cout << "Directory is full" << endl;
    return;
  }
  
  // Get a free block for the new directory
  short new_dir_block = bfs.get_free_block();
  if (new_dir_block == 0) {
    cout << "Disk is full" << endl;
    return;
  }
  
  // Initialize the new directory block
  struct dirblock_t new_dir;
  new_dir.magic = DIR_MAGIC_NUM;
  new_dir.num_entries = 0;
  for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
    new_dir.dir_entries[i].block_num = 0;
  }
  
  // Write the new directory block to disk
  bfs.write_block(new_dir_block, (void *) &new_dir);
  
  // Add the new directory to the current directory
  strcpy(dir_block.dir_entries[dir_block.num_entries].name, name);
  dir_block.dir_entries[dir_block.num_entries].block_num = new_dir_block;
  dir_block.num_entries++;
  
  // Write the updated current directory block back to disk
  bfs.write_block(curr_dir, (void *) &dir_block);
}

// switch to a directory
void FileSys::cd(const char *name)
{
  bool is_dir;
  short dir_block = find_file(name, is_dir);
  
  // Check if file exists
  if (dir_block == 0) {
    cout << "File does not exist" << endl;
    return;
  }
  
  // Check if file is a directory
  if (!is_dir) {
    cout << "File is not a directory" << endl;
    return;
  }
  
  // Update current directory
  curr_dir = dir_block;
}

// switch to home directory
void FileSys::home() {
  curr_dir = 1; // Home directory is always block 1
}

// remove a directory
void FileSys::rmdir(const char *name)
{
  bool is_dir;
  short dir_block =  find_file(name, is_dir);
  
  // Check if file exists
  if (dir_block == 0) {
    cout << "File does not exist" << endl;
    return;
  }
  
  // Check if file is a directory
  if (!is_dir) {
    cout << "File is not a directory" << endl;
    return;
  }
  
  // Check if directory is empty
  struct dirblock_t dir;
  bfs.read_block(dir_block, (void *) &dir);
  if (dir.num_entries > 0) {
    cout << "Directory is not empty" << endl;
    return;
  }
  
  // Remove the directory entry from the current directory
  struct dirblock_t curr_dir_block;
  bfs.read_block(curr_dir, (void *) &curr_dir_block);
  
  int entry_idx = -1;
  for (int i = 0; i < curr_dir_block.num_entries; i++) {
    if (strcmp(curr_dir_block.dir_entries[i].name, name) == 0) {
      entry_idx = i;
      break;
    }
  }
  
  // Shift remaining entries to fill the gap
  for (int i = entry_idx; i < curr_dir_block.num_entries - 1; i++) {
    strcpy(curr_dir_block.dir_entries[i].name, curr_dir_block.dir_entries[i+1].name);
    curr_dir_block.dir_entries[i].block_num = curr_dir_block.dir_entries[i+1].block_num;
  }
  
  // Update the entry count and clear the last entry
  curr_dir_block.num_entries--;
  curr_dir_block.dir_entries[curr_dir_block.num_entries].block_num = 0;
  
  // Write the updated current directory block back to disk
  bfs.write_block(curr_dir, (void *) &curr_dir_block);
  
  // Reclaim the directory block
  bfs.reclaim_block(dir_block);
}

// list the contents of current directory
void FileSys::ls()
{
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void *) &dir_block);
  
  for (int i = 0; i < dir_block.num_entries; i++) {
    cout << dir_block.dir_entries[i].name;
    
    // Add a "/" suffix for directories
    if (is_directory(dir_block.dir_entries[i].block_num)) {
      cout << "/";
    }
    
    cout << endl;
  }
}

// create an empty data file
void FileSys::create(const char *name)
{
  // Check if filename is too long
  if (!check_filename(name)) {
    cout << "File name is too long" << endl;
    return;
  }
  
  // Check if file already exists
  bool is_dir;
  if (find_file(name, is_dir) != 0) {
    cout << "File exists" << endl;
    return;
  }
  
  // Check if directory is full
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void *) &dir_block);
  if (dir_block.num_entries >= MAX_DIR_ENTRIES) {
    cout << "Directory is full" << endl;
    return;
  }
  
  // Get a free block for the inode
  short inode_block = bfs.get_free_block();
  if (inode_block == 0) {
    cout << "Disk is full" << endl;
    return;
  }
  
  // Initialize the inode
  struct inode_t inode;
  inode.magic = INODE_MAGIC_NUM;
  inode.size = 0;
  for (int i = 0; i < MAX_DATA_BLOCKS; i++) {
    inode.blocks[i] = 0;
  }
  
  // Write the inode to disk
  bfs.write_block(inode_block, (void *) &inode);
  
  // Add the file to the current directory
  strcpy(dir_block.dir_entries[dir_block.num_entries].name, name);
  dir_block.dir_entries[dir_block.num_entries].block_num = inode_block;
  dir_block.num_entries++;
  
  // Write the updated current directory block back to disk
  bfs.write_block(curr_dir, (void *) &dir_block);
}

// append data to a data file
void FileSys::append(const char *name, const char *data)
{
  bool is_dir;
  short file_block = find_file(name, is_dir);
  
  // Check if file exists
  if (file_block == 0) {
    cout << "File does not exist" << endl;
    return;
  }
  
  // Check if it's a directory
  if (is_dir) {
    cout << "File is a directory" << endl;
    return;
  }
  
  // Read the inode
  struct inode_t inode;
  bfs.read_block(file_block, (void *) &inode);
  
  // Calculate new size after append
  unsigned int data_len = strlen(data);
  unsigned int new_size = inode.size + data_len;
  
  // Check if append would exceed maximum file size
  if (new_size > MAX_FILE_SIZE) {
    cout << "Append exceeds maximum file size" << endl;
    return;
  }
  
  // Calculate which blocks we need to use
  unsigned int start_block = inode.size / BLOCK_SIZE;
  unsigned int start_offset = inode.size % BLOCK_SIZE;
  unsigned int end_block = new_size / BLOCK_SIZE;
  unsigned int end_offset = new_size % BLOCK_SIZE;
  
  // If end_offset is 0 and new_size > 0, adjust end values
  if (end_offset == 0 && new_size > 0) {
    end_block--;
    end_offset = BLOCK_SIZE;
  }
  
  // Check if we need more blocks than available
  if (end_block >= MAX_DATA_BLOCKS) {
    cout << "Append exceeds maximum file size" << endl;
    return;
  }
  
  // Calculate how many new blocks we need
  int new_blocks_needed = 0;
  for (unsigned int i = start_block; i <= end_block; i++) {
    if (inode.blocks[i] == 0) {
      new_blocks_needed++;
    }
  }
  
  // Verify we have enough disk space
  if (new_blocks_needed > 0) {
    // Check available blocks without actually allocating them yet
    struct superblock_t super_block;
    bfs.read_block(0, (void *) &super_block);
    
    int free_blocks = 0;
    for (int byte = 0; byte < BLOCK_SIZE; byte++) {
      for (int bit = 0; bit < 8; bit++) {
        if (!(super_block.bitmap[byte] & (1 << bit))) {
          free_blocks++;
          if (free_blocks >= new_blocks_needed) {
            break;
          }
        }
      }
      if (free_blocks >= new_blocks_needed) {
        break;
      }
    }
    
    if (free_blocks < new_blocks_needed) {
      cout << "Disk is full" << endl;
      return;
    }
  }
  
  // Append data block by block
  unsigned int data_pos = 0;
  
  // Handle first block (may need to read existing data)
  if (start_block == end_block) {
    // All data fits in one block
    struct datablock_t data_block;
    
    // If block doesn't exist yet, allocate it
    if (inode.blocks[start_block] == 0) {
      inode.blocks[start_block] = bfs.get_free_block();
      memset(data_block.data, 0, BLOCK_SIZE);
    } else {
      // Read existing block
      bfs.read_block(inode.blocks[start_block], (void *) &data_block);
    }
    
    // Copy data into block
    memcpy(data_block.data + start_offset, data, data_len);
    
    // Write block back to disk
    bfs.write_block(inode.blocks[start_block], (void *) &data_block);
  } else {
    // Data spans multiple blocks
    
    // Handle first block if it exists
    if (start_offset > 0 || inode.blocks[start_block] != 0) {
      struct datablock_t data_block;
      
      // If block doesn't exist yet, allocate it
      if (inode.blocks[start_block] == 0) {
        inode.blocks[start_block] = bfs.get_free_block();
        memset(data_block.data, 0, BLOCK_SIZE);
      } else {
        // Read existing block
        bfs.read_block(inode.blocks[start_block], (void *) &data_block);
      }
      
      // Copy data into block
      unsigned int bytes_to_copy = BLOCK_SIZE - start_offset;
      memcpy(data_block.data + start_offset, data, bytes_to_copy);
      
      // Write block back to disk
      bfs.write_block(inode.blocks[start_block], (void *) &data_block);
      
      // Update data position
      data_pos += bytes_to_copy;
    }
    
    // Handle middle blocks (full blocks)
    for (unsigned int i = start_block + 1; i < end_block; i++) {
      struct datablock_t data_block;
      
      // Allocate new block if needed
      if (inode.blocks[i] == 0) {
        inode.blocks[i] = bfs.get_free_block();
      }
      
      // Copy full block of data
      memcpy(data_block.data, data + data_pos, BLOCK_SIZE);
      
      // Write block to disk
      bfs.write_block(inode.blocks[i], (void *) &data_block);
      
      // Update data position
      data_pos += BLOCK_SIZE;
    }
    
    // Handle last block if needed
    if (end_offset > 0) {
      struct datablock_t data_block;
      
      // Allocate new block if needed
      if (inode.blocks[end_block] == 0) {
        inode.blocks[end_block] = bfs.get_free_block();
        memset(data_block.data, 0, BLOCK_SIZE);
      } else {
        // Read existing block
        bfs.read_block(inode.blocks[end_block], (void *) &data_block);
      }
      
      // Copy remaining data
      memcpy(data_block.data, data + data_pos, end_offset);
      
      // Write block to disk
      bfs.write_block(inode.blocks[end_block], (void *) &data_block);
    }
  }
  
  // Update inode size and write back to disk
  inode.size = new_size;
  bfs.write_block(file_block, (void *) &inode);
}

// display the contents of a data file
void FileSys::cat(const char *name)
{
  bool is_dir;
  short file_block = find_file(name, is_dir);
  
  // Check if file exists
  if (file_block == 0) {
    cout << "File does not exist" << endl;
    return;
  }
  
  // Check if it's a directory
  if (is_dir) {
    cout << "File is a directory" << endl;
    return;
  }
  
  // Read the inode
  struct inode_t inode;
  bfs.read_block(file_block, (void *) &inode);
  
  // Display file contents block by block
  unsigned int bytes_remaining = inode.size;
  unsigned int block_index = 0;
  
  while (bytes_remaining > 0) {
    // Read the data block
    struct datablock_t data_block;
    bfs.read_block(inode.blocks[block_index], (void *) &data_block);
    
    // Determine how many bytes to display from this block
    unsigned int bytes_to_display = (bytes_remaining < BLOCK_SIZE) ? bytes_remaining : BLOCK_SIZE;
    
    // Display the data one character at a time
    for (unsigned int i = 0; i < bytes_to_display; i++) {
      cout << data_block.data[i];
    }
    
    // Update remaining bytes and block index
    bytes_remaining -= bytes_to_display;
    block_index++;
  }
  
  cout << endl;
}

// display the last N bytes of the file
void FileSys::tail(const char *name, unsigned int n)
{
  bool is_dir;
  short file_block = find_file(name, is_dir);
  
  // Check if file exists
  if (file_block == 0) {
    cout << "File does not exist" << endl;
    return;
  }
  
  // Check if it's a directory
  if (is_dir) {
    cout << "File is a directory" << endl;
    return;
  }
  
  // Read the inode
  struct inode_t inode;
  bfs.read_block(file_block, (void *) &inode);
  
  // If n is greater than or equal to file size, just display the whole file
  if (n >= inode.size) {
    cat(name);
    return;
  }
  
  // Calculate starting position
  unsigned int start_pos = inode.size - n;
  unsigned int start_block = start_pos / BLOCK_SIZE;
  unsigned int start_offset = start_pos % BLOCK_SIZE;
  
  // Display last n bytes
  unsigned int bytes_remaining = n;
  unsigned int block_index = start_block;
  
  // Handle first block (may be partial)
  if (start_offset > 0) {
    struct datablock_t data_block;
    bfs.read_block(inode.blocks[block_index], (void *) &data_block);
    
    unsigned int bytes_to_display = (bytes_remaining < (BLOCK_SIZE - start_offset)) ? 
                                     bytes_remaining : (BLOCK_SIZE - start_offset);
    
    for (unsigned int i = 0; i < bytes_to_display; i++) {
      cout << data_block.data[start_offset + i];
    }
    
    bytes_remaining -= bytes_to_display;
    block_index++;
  }
  
  // Handle remaining blocks
  while (bytes_remaining > 0) {
    struct datablock_t data_block;
    bfs.read_block(inode.blocks[block_index], (void *) &data_block);
    
    unsigned int bytes_to_display = (bytes_remaining < BLOCK_SIZE) ? bytes_remaining : BLOCK_SIZE;
    
    for (unsigned int i = 0; i < bytes_to_display; i++) {
      cout << data_block.data[i];
    }
    
    bytes_remaining -= bytes_to_display;
    block_index++;
  }
  
  cout << endl;
}

// delete a data file
void FileSys::rm(const char *name)
{
  bool is_dir;
  short file_block = find_file(name, is_dir);
  
  // Check if file exists
  if (file_block == 0) {
    cout << "File does not exist" << endl;
    return;
  }
  
  // Check if it's a directory
  if (is_dir) {
    cout << "File is a directory" << endl;
    return;
  }
  
  // Remove the file entry from the current directory
  struct dirblock_t dir_block;
  bfs.read_block(curr_dir, (void *) &dir_block);
  
  int entry_idx = -1;
  for (int i = 0; i < dir_block.num_entries; i++) {
    if (strcmp(dir_block.dir_entries[i].name, name) == 0) {
      entry_idx = i;
      break;
    }
  }
  
  // Shift remaining entries to fill the gap
  for (int i = entry_idx; i < dir_block.num_entries - 1; i++) {
    strcpy(dir_block.dir_entries[i].name, dir_block.dir_entries[i+1].name);
    dir_block.dir_entries[i].block_num = dir_block.dir_entries[i+1].block_num;
  }
  
  // Update the entry count and clear the last entry
  dir_block.num_entries--;
  dir_block.dir_entries[dir_block.num_entries].block_num = 0;
  
  // Write the updated directory block back to disk
  bfs.write_block(curr_dir, (void *) &dir_block);
  
  // Reclaim all blocks used by the file
  reclaim_blocks(file_block, false);
}

// display stats about file or directory
void FileSys::stat(const char *name)
{
  bool is_dir;
  short block_num = find_file(name, is_dir);
  
  // Check if file exists
  if (block_num == 0) {
    cout << "File does not exist" << endl;
    return;
  }
  
  if (is_dir) {
    // Directory stats
    struct dirblock_t dir_block;
    bfs.read_block(block_num, (void *) &dir_block);
    
    cout << "Directory name: " << name << "/" << endl;
    cout << "Directory block: " << block_num << endl;
  } else {
    // File stats
    struct inode_t inode;
    bfs.read_block(block_num, (void *) &inode);
    
    // Calculate number of blocks (inode block + data blocks)
    int num_blocks = 1; // Start with 1 for the inode block
    for (int i = 0; i < MAX_DATA_BLOCKS; i++) {
      if (inode.blocks[i] != 0) {
        num_blocks++;
      }
    }
    
    // First data block (0 if empty file)
    short first_block = 0;
    if (inode.size > 0) {
      first_block = inode.blocks[0];
    }
    
    cout << "Inode block: " << block_num << endl;
    cout << "Bytes in file: " << inode.size << endl;
    cout << "Number of blocks: " << num_blocks << endl;
    cout << "First block: " << first_block << endl;
  }
}

