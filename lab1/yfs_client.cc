// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client()
{
    ec = new extent_client();
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::isfile(inum inum)
{
    printf("\tisfile\n");
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE)
    {
        printf("isfile: %lld is a file\n", inum);
        return true;
    }
    printf("isfile: %lld is not a file\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    printf("\tisdir\n");
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR)
    {
        printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    printf("isdir: %lld is not a dir\n", inum);
    return false;
}

bool yfs_client::issymlink(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK)
    {
        printf("issymlink: %lld is a symlink\n", inum);
        return true;
    }
    printf("issymlink: %lld is not a synlink\n", inum);
    return false;
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}

#define EXT_RPC(xx)                                                \
    do                                                             \
    {                                                              \
        if ((xx) != extent_protocol::OK)                           \
        {                                                          \
            printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
            r = IOERR;                                             \
            goto release;                                          \
        }                                                          \
    } while (0)

// Only support set size of attr
int yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    std::string buf;
    ec->get(ino, buf);
    buf.resize(size);

    // put 这步会更新inode的size
    ec->put(ino, buf);
    return r;
}

/*
 * parent : 在parent inode 创建
 * name : 需要创建的文件名
 * mode:
 * ino_out :  name 文件的inode num
 */
int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    printf("\tcreate\n");
    bool found = false;
    inum tmp;
    lookup(parent, name, found, tmp);
    if (found)
    {
        return EXIST;
    }

    ec->create(extent_protocol::T_FILE, ino_out);

    // modify the parent infomation
    std::string buf;
    ec->get(parent, buf);
    buf.append(std::string(name) + ":" + filename(ino_out) + "/");
    ec->put(parent, buf);

    return r;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    printf("\tmkdir - name : %s\n ",name);
    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    // check if directory exist
    bool found = false;
    inum tmp; 
    lookup(parent, name, found, tmp);
    if (found)
    {
        // has existed
        return EXIST;
    }

    ec->create(extent_protocol::T_DIR, ino_out);

    // modify the parent infomation
    std::string buf;
    ec->get(parent, buf);
    buf.append(std::string(name) + ":" + filename(ino_out) + "/");
    ec->put(parent, buf);

    return r;
}

/*
 * parent : 在parent inode 底下找
 * name : 需要查找的文件名
 * found : 是否找到
 * ino_out :  name 文件的inode num
 */
int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    printf("\tlookup - name : %s\n ",name);
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    std::list<dirent> list;
    readdir(parent, list);

    if (list.empty())
    {
        printf("\tparent is empty\n");
        found = false;
        return r;
    }

    printf("\tlookup - list : ");
    for (std::list<dirent>::iterator it = list.begin(); it != list.end(); it++)
    {
        printf("\t%s",it->name.c_str());
        if (it->name.compare(name) == 0)
        {
            found = true;
            ino_out = it->inum;
            return r;
        }
    }
    found = false;
    return r;
}

int yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    // format : "name:inum/name:inum/name:inum/"
    printf("\treaddir\n");
    std::string buf;
    ec->get(dir, buf);
    printf("\treaddir buf : %s\n",buf.c_str());
    // traverse directory content
    int name_start = 0;

    
    int name_end = buf.find(':');
    if(name_end==-1) 
    {
        printf("\tno file\n");
        return r;
    }
    printf("\treaddir first name_end : %d\n",name_end);
    printf("\treaddir dir: ");
    while (name_end != -1)
    {
        std::string name = buf.substr(name_start, name_end - name_start);
        printf("\tname : %s",name.c_str());
        int inum_start = name_end + 1;
        int inum_end = buf.find('/', inum_start);
        std::string inum = buf.substr(inum_start, inum_end - inum_start);
        printf("\tinum : %s",inum.c_str());
        struct dirent entry;
        entry.name = name;
        entry.inum = n2i(inum);

        list.push_back(entry);

        name_start = inum_end + 1;
        name_end = buf.find(':', name_start);
    }
    printf("\n");
    return r;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    printf("\tread\n");
    std::string buf;
    ec->get(ino, buf);  

    // off > file size
    if ((unsigned int)off > buf.size()) {
        data = "";
        return r;
    }

    // off + read size > file size
    if (off + size > buf.size()) {
        data = buf.substr(off);
        return r;
    }

    // normal
    data = buf.substr(off, size);
    return r;
}

int yfs_client::write(inum ino, size_t size, off_t off, const char *data,
                      size_t &bytes_written)
{
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    printf("\twrite\n");
    std::string buf;
    ec->get(ino, buf);
    
    // off + write size <= file size
    if (off + size <= buf.size()) {
        for (unsigned int i = off; i < off + size; i++) {
            buf[i] = data[i - off];
        }
        bytes_written = size;
        ec->put(ino, buf);
        return r;
    }

    // off + write size > file size
    buf.resize(off + size);
    for (unsigned int i = off; i < off + size; i++) {
        buf[i] = data[i - off];
    }
    bytes_written = size;
    ec->put(ino, buf);

    return r;
}

int yfs_client::unlink(inum parent, const char *name)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    printf("\tunlink\n");
    bool found = false; 
    inum inum;
    lookup(parent, name, found, inum);

    ec->remove(inum);

    // update parent directory content
    std::string buf;
    ec->get(parent, buf);
    int erase_start = buf.find(name);
    int erase_after = buf.find('/', erase_start);
    buf.erase(erase_start, erase_after - erase_start + 1);
    ec->put(parent, buf);
    return r;
}

int yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out)
{
    int r = OK;

    printf("\tsymlink\n");
    bool found = false;
    inum tmp;  
    // 看看你重名
    lookup(parent, name, found, tmp);
    if (found) {
        return EXIST;
    }

    // pick an inum and init the symlink
    ec->create(extent_protocol::T_SYMLINK, ino_out);
    ec->put(ino_out, std::string(link));

    // add an entry into parent
    std::string buf;
    ec->get(parent, buf);
    buf.append(std::string(name) + ":" + filename(ino_out) + "/");
    ec->put(parent, buf);
    return r;
}

int yfs_client::readlink(inum ino, std::string &data)
{
    int r = OK;
     printf("\treadlink\n");
    std::string buf;
    ec->get(ino, buf);

    data = buf;
    
    return r;
}