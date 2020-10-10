#include "inode_manager.h"
#include <string>
// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM)
  {
    printf("\tdisk: block id out of range！！: %d\n", id);
    return;
  }

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM)
  {
    printf("\tdisk: block id out of range！！: %d\n", id);
    return;
  }

  // printf("\twrite_block : %d\n",id);
  memcpy(blocks[id], buf, BLOCK_SIZE); // ??? 不能是sizeof(buf)
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */

  // 为什么要从sb.nblocks/BPB +sb.ninodes+2开始呢
  // +2 ： 一个引导块（但是文档中好像没有引导块所以此处写+1） 一个superblock
  // sb.nblocks/BPB ： bit_map 用了这么多块
  // sb.ninodes ：inode_table 用了这么多块 （因为IPB==1 所以这里没有写 /IBP）
  for (int i = IBLOCK(INODE_NUM, sb.nblocks) + 1; i < BLOCK_NUM; i++)
    if (!using_blocks[i])
    {
      using_blocks[i] = 1;
      // printf("\talloc_block : %d\n",i);
      return i;
    }
  printf(" error : alloc_block failed!\n");

  return 0;
}
// IBLOCK(INODE_NUM, sb.nblocks) + 1
void block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */

  using_blocks[id] = 0;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
}

void block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  printf("\t new inode_manager\n");
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1)
  {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */

  // 第一个可用inode在2号
  uint32_t inode_num = 1;

  for (; inode_num <= INODE_NUM; inode_num++)
  {
    inode_t *i = get_inode(inode_num);
    if (i == NULL)
    {
      i = (inode_t *)malloc(sizeof(inode_t));
      bzero(i, sizeof(inode_t));
      i->type = type;
      i->atime = time(NULL);
      i->mtime = time(NULL);
      i->ctime = time(NULL);
      put_inode(inode_num, i);
      free(i);
      break;
    }
    free(i);
  }
  if (inode_num <= INODE_NUM)
  {
    return inode_num;
  }
  else
  {
    printf(" error : alloc_inode failed!\n");
    exit(-1);
    return 0;
  }
}

void inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  struct inode *ino;
  if ((ino = get_inode(inum)) == NULL)
  {
    // already freed
    return;
  }

  // ino type =0 则为未分配
  ino->type = 0;

  // write back to disk
  put_inode(inum, ino);
  free(ino);

  return;
}

/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM)
  {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode *)buf + inum % IPB;
  if (ino_disk->type == 0)
  {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  // get_inode 是新malloc一块空间返回的  用了之后都要Free
  ino = (struct inode *)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode *)buf + inum % IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
  printf("\tput_inode block num : %d\n", IBLOCK(inum, bm->sb.nblocks));
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */

  inode_t *ino = get_inode(inum);
  if (ino == NULL)
    return;
  int pos = 0;

  // 文件总大小
  *size = ino->size;
  printf("\tread_file inum: %d size: %d\n", inum, *size);
  // 如果问价大小等于0 改一下atime直接返回
  if (ino->size == 0)
  {
    printf("\tread_file : size == 0 \n");
    *size = 0;
    ino->atime = time(NULL);
    put_inode(inum, ino);
    free(ino);
    return;
  }

  *buf_out = (char *)malloc(ino->size);

  int block_num = (ino->size - 1) / BLOCK_SIZE + 1;
  // 如果是小文件
  if (ino->size <= NDIRECT * BLOCK_SIZE)
  {
    pos=0;
    printf("\tread_file 3 : direct\n");
    printf("\tread_file block num : \n");
    for (int i = 0; i < block_num; i++)
    {
      char buf[BLOCK_SIZE];
      printf("%d\n", ino->blocks[i]);
      bm->read_block(ino->blocks[i], buf);
      if (i != block_num - 1)
      {
        memcpy(*buf_out + pos, buf, BLOCK_SIZE);
        // if(ino->blocks[i]==63)
        printf("%s\n", buf);
        pos += BLOCK_SIZE;
      }
      else
      {
        memcpy(*buf_out + pos, buf, ((ino->size - 1) % BLOCK_SIZE) + 1);
        pos += ((ino->size - 1) % BLOCK_SIZE) + 1;
        printf("%s\n", buf);
      }
    }
    printf("\n");
    // printf("\tread_file buf_out: %s\n", *buf_out);
  }
  else
  {
    //如果是大文件 用了indirect block
    //把最后一个当成 indirect block
    printf("\tread_file: indirect\n");
    printf("\tread_file block num : \n");
  
    pos=0;
    for (int j = 0; j < NDIRECT; j++)
    {
      char buf[BLOCK_SIZE];
       printf("%d\n", ino->blocks[j]);
      bm->read_block(ino->blocks[j], buf);
      printf("%s\n", buf);
      memcpy(*buf_out +pos, buf, BLOCK_SIZE);
      pos += BLOCK_SIZE;
    }

    char indirect[BLOCK_SIZE];
    bm->read_block(ino->blocks[NDIRECT], indirect);
    
    // printf("\tindirect block : ");
    // for (int i = 0; i<(ino->size - NDIRECT * BLOCK_SIZE - 1) / BLOCK_SIZE + 1; i++)
    // {
    //   printf("\t%d",*((uint32_t *)indirect + i));
      
    // }
    // printf("\n");
    // printf("\tread_file indirect block: %s\n",indirect);
    // 看看剩下还要几块
    for (unsigned int i = 0; i<(ino->size - NDIRECT * BLOCK_SIZE - 1) / BLOCK_SIZE + 1; i++)
    {
      char buf[BLOCK_SIZE];
       printf("%d\n", *((uint32_t *)indirect + i));
      bm->read_block(*((uint32_t *)indirect + i), buf);
      printf("%s\n", buf);
      if ((unsigned int)i != (ino->size - NDIRECT * BLOCK_SIZE - 1) / BLOCK_SIZE)
      {
        // not last i
        memcpy(*buf_out + pos, buf, BLOCK_SIZE);
        pos += BLOCK_SIZE;
      }
      else
      {
        // last i
        memcpy(*buf_out +pos, buf, ((ino->size - 1) % BLOCK_SIZE) + 1);
      }
    }
    printf("\n");
  }

  ino->atime = (unsigned int)time(NULL);
  put_inode(inum, ino);
  free(ino);
  return;
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  printf("\twrite_file 1: %d \n", inum);
  inode_t *ino = get_inode(inum);
  if (ino == NULL)
    return;

  //文件不能写太大 超过最大大小
  if ((unsigned int)size > MAXFILE * BLOCK_SIZE)
  {
    printf("\twrite_file error: size \n");
    return;
  }

  // 原来的文件用了 old_block_num 个块
  int old_block_num = ino->size == 0 ? 0 : (ino->size - 1) / BLOCK_SIZE + 1;
  // 现在的文件用了 new_block_num 个块
  int new_block_num = size == 0 ? 0 : (size - 1) / BLOCK_SIZE + 1;

  // 释放原来的块
  if (ino->size > 0)
  {
    // 小文件
    if (ino->size <= NDIRECT * BLOCK_SIZE)
    {
      printf("\twrite_file: direct\n");
      for (int i = 0; i < old_block_num; i++)
      {
        bm->free_block(ino->blocks[i]);
      }
    }
    else
    {
      printf("\twrite_file: indirect\n");
      for (int i = 0; i < NDIRECT; i++)
      {
        bm->free_block(ino->blocks[i]);
      }

      // indirect block 在ino->blocks 的最后一个？
      char indirect[BLOCK_SIZE];
      bm->read_block(ino->blocks[NDIRECT], indirect);

      // 写在indirect block 里的block也要free
      for (unsigned int i = 0; i < (ino->size - NDIRECT * BLOCK_SIZE - 1) / BLOCK_SIZE + 1; i++)
      {
        printf("write_file indirect block num : %d ", *((uint32_t *)indirect + i));
        bm->free_block(*((uint32_t *)indirect + i));
      }
      bm->free_block(ino->blocks[NDIRECT]);
    }
  }
  else
  {
    printf("\twrite_file : ino->size == 0 \n");
  }

  // alloc
  if (size <= NDIRECT * BLOCK_SIZE)
  {
    printf("\twrite_file new_block_num : %d\n", new_block_num);
    printf("\twrite_file block num : \t");
    // printf("%s",buf );
    for (int i = 0; i < new_block_num; i++)
    {
      blockid_t b = bm->alloc_block();
      printf("%d\t", b);
      bm->write_block(b, buf + i * BLOCK_SIZE);
      ino->blocks[i] = b;
    }
    printf("\n");
  }
  else
  {
    // 先把direct block的块给填了
    printf("\twrite_file new_block_num : %d\n", new_block_num);
    printf("\twrite_file block num : \t");
    for (int i = 0; i < NDIRECT; i++)
    {
      uint32_t b = bm->alloc_block();
      printf("%d\t", b);
      bm->write_block(b, buf + i * BLOCK_SIZE);
      ino->blocks[i] = b;
    }

    printf("\n");

    // 为indirect block 分配空间
    ino->blocks[NDIRECT] = bm->alloc_block();

    char indirect[BLOCK_SIZE];
    printf("\twrite_file block num in indirect block: \t");
    for (int i = 0; i < (size - NDIRECT * BLOCK_SIZE - 1) / BLOCK_SIZE + 1; i++)
    {
      uint32_t b = bm->alloc_block();
      printf("%d\t", b);
      bm->write_block(b, buf + (NDIRECT + i) * BLOCK_SIZE);
      *((uint32_t *)indirect + i) = b;
    }
    printf("\n");

    printf("\tindirect block : ");
    for (int i = 0; i < (size - NDIRECT * BLOCK_SIZE - 1) / BLOCK_SIZE + 1; i++)
    {
      printf("\t%d",*((uint32_t *)indirect + i));
      
    }
    printf("\n");
    bm->write_block(ino->blocks[NDIRECT], indirect);
  }
  //debug
  // printf("\twrite_file debug\n");
  // char buf1[BLOCK_SIZE];
  // bm->read_block(63, buf1);
  // printf("\t63 : %s\n",buf1);
  ino->size = size;
  ino->atime = (unsigned int)time(NULL);
  ino->mtime = (unsigned int)time(NULL);
  ino->ctime = (unsigned int)time(NULL);
  put_inode(inum, ino);
  free(ino);

  //debug
  // printf("\twrite_file debug\n");
  // char buf1[BLOCK_SIZE];
  // bm->read_block(63, buf1);
  // printf("\n 63 : %s\n",buf1);
  // debug
  // char* buf_out=NULL;
  // int size_out=0;
  // read_file(inum,&buf_out,&size_out);

  return;
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */

  inode_t *i = get_inode(inum);
  if (!i)
    return;
  a.type = i->type;
  a.atime = i->atime;
  a.mtime = i->mtime;
  a.ctime = i->ctime;
  a.size = i->size;
  free(i);
  return;
}

void inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */

  struct inode *ino = get_inode(inum);
  if (ino == NULL)
    return;

  int block_num=ino->size == 0 ? 0 : (ino->size - 1) / BLOCK_SIZE + 1;
  for(int i=0;i<block_num;i++)
  {
    bm->free_block(ino->blocks[i]);
  }

  ino->type=0;
  put_inode(inum,ino);

  return;
}
