// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include "extent_protocol.h" // TODO: delete it

#define DISK_SIZE  1024*1024*16
#define BLOCK_SIZE 512
#define BLOCK_NUM  (DISK_SIZE/BLOCK_SIZE)  //总共有32768个block

typedef uint32_t blockid_t;

// disk layer -----------------------------------------

class disk {
 private:
  unsigned char blocks[BLOCK_NUM][BLOCK_SIZE];

 public:
  disk();
  void read_block(uint32_t id, char *buf);
  void write_block(uint32_t id, const char *buf);
};

// block layer -----------------------------------------

typedef struct superblock {
  uint32_t size;
  uint32_t nblocks;
  uint32_t ninodes;
} superblock_t;

class block_manager {
 private:
  disk *d;
  std::map <uint32_t, int> using_blocks;    // bit_map
 public:
  block_manager();
  struct superblock sb;

  uint32_t alloc_block();   //返回分配的块的块号
  void free_block(uint32_t id);
  void read_block(uint32_t id, char *buf);
  void write_block(uint32_t id, const char *buf);
};

// inode layer -----------------------------------------

#define INODE_NUM  1024

// Inodes per block.
#define IPB           1
//(BLOCK_SIZE / sizeof(struct inode))

// Block containing inode i
// i inode 在第 IBLOCK(i, nblocks)block
#define IBLOCK(i, nblocks)     ((nblocks)/BPB + (i)/IPB + 3)
// nblocks 总共有几个block
// (nblocks)/BPB  总共多少个block用来存bit map了
// +3 = +2 +1  = 引导块 + super block + inode table是从1开始算块的

// Bitmap bits per block    一个block有多少个位
#define BPB           (BLOCK_SIZE*8)

// Block containing bit for block b
// bitmap里的位数
// 第0个block在bitmap的第二位
// b从bitmap的block开始算
#define BBLOCK(b) ((b)/BPB + 2)

#define NDIRECT 100    // 总共有多少个直接块
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))  // 一块block可以放多少间接块
#define MAXFILE (NDIRECT + NINDIRECT)  //最大文件可以用多少个块

typedef struct inode {
  short type;
  unsigned int size;
  unsigned int atime;
  unsigned int mtime;
  unsigned int ctime;
  blockid_t blocks[NDIRECT+1];   // Data block addresses  // +1指1个indirect block
} inode_t;

class inode_manager {
 private:
  block_manager *bm;
  struct inode* get_inode(uint32_t inum);
  void put_inode(uint32_t inum, struct inode *ino);

 public:
  inode_manager();
  uint32_t alloc_inode(uint32_t type);
  void free_inode(uint32_t inum);
  void read_file(uint32_t inum, char **buf, int *size);
  void write_file(uint32_t inum, const char *buf, int size);
  void remove_file(uint32_t inum);
  void getattr(uint32_t inum, extent_protocol::attr &a);
};

#endif

