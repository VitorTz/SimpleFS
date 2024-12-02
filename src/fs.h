#ifndef FS_H
#define FS_H

#include "disk.h"
#include <vector>
#include <cmath>


class SimpleFs {

public:
    static const unsigned int FS_MAGIC = 0xf0f03410;
    static const unsigned short int INODES_PER_BLOCK = 128;
    static const unsigned short int POINTERS_PER_INODE = 5;
    static const unsigned short int POINTERS_PER_BLOCK = 1024;


    class fs_superblock {
        public:
            unsigned int magic;
            int nblocks;
            int ninodeblocks;
            int ninodes;
    }; 

    class fs_inode {
        public:
            int isvalid;
            int size;
            int direct[POINTERS_PER_INODE];
            int indirect;
    };

    union fs_block {
        public:
            fs_superblock super;
            fs_inode inode[INODES_PER_BLOCK];
            int pointers[POINTERS_PER_BLOCK];
            char data[Disk::DISK_BLOCK_SIZE];
    };

    typedef struct fs_datablock_vec {
        int indirect{};        
        std::vector<int> blocks{};
    } fs_datablock_vec_t;

public:

    SimpleFs(
        Disk *d
    ) : bitmap(std::vector<bool>(d->size(), false)) {
        disk = d;        
    } 

    void fs_debug();
    int  fs_format();
    int  fs_mount();

    int  fs_create();
    int  fs_delete(int inumber);
    int  fs_getsize(int inumber);

    int  fs_read(int inumber, char *data, int length, int offset);
    int  fs_write(int inumber, const char *data, int length, int offset);

private:
    Disk *disk;
    bool is_disk_mounted{};
    std::vector<bool> bitmap{};
    fs_block tmp_block{};
    fs_superblock superblock;
    const fs_block EMPTY_BLOCK{};

private:
    
    bool is_valid_datablock(int datablock);
    bool is_valid_inode_num(int inumber);
    bool is_valid_block(int iblock);
    bool is_disk_full() const;

    int alloc_datablock();

    int read_datablock(const int idatablock, char* dest, int start, int end);
    int write_datablock(const int idatablock, const char* src, int start, int end);    
    
    bool inode_load(int inumber, fs_inode* inode);
    bool inode_save(int inumber, const fs_inode& inode);

    void reset_bitmap();

    void mark_block_free(int iblock);
    void mark_block_busy(int iblock);
    void clear_block_data(int iblock);

    void fill_datablocks(int inumber, fs_datablock_vec_t& vec);

    void read_superblock();

};

#endif