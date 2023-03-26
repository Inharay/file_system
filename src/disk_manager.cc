#include "disk_manager.h"
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <utility>
#include <stdexcept>
#include "log.h"
#include "super_block_mem.h"
#include "inode_mem.h"
#include "const_variable.h"
#include "entry.h"
int DiskManager::FormatDisk() {
    if ((disk_fd = open(disk_name, O_RDWR)) < 0) {
        SYS_ERR("open error %s", disk_name);
    }
    struct stat stat;
    if (fstat(disk_fd, &stat) < 0) {
        SYS_ERR("get stat error");
    }

    if ((stat.st_size >> 20) < 10) {
        throw std::runtime_error("disk size small than 10MB");
    }

    SuperBlockMem sp_block;
    sp_block.saveSuperBlockToDisk(disk_fd);

    InitMapBlock(sp_block.sb_disk);

    Inode root_inode;
    root_inode.i_disk.i_data_map[0] = 1;

    // 添加默认目录
    Entry entryDot{".", 1};
    entryDot.saveEntryToDisk(disk_fd, GetBlkIdxByDataIdx(sp_block.sb_disk, root_inode.i_disk.i_data_map[0]), 0);

    Entry entryDotDot{"..", 1};
    entryDotDot.saveEntryToDisk(disk_fd, GetBlkIdxByDataIdx(sp_block.sb_disk, root_inode.i_disk.i_data_map[0]), 1);

    // 保存Inode
    root_inode.saveInodeToDisk(disk_fd, GetBlkIdxByInodeIdx(sp_block.sb_disk , 1));

    // 更新InodeMap
    SetMapStatus(1, 0, 1);

    // 更新DataMap
    SetMapStatus(1 + sp_block.sb_disk.i_map_blocks, 0 , 1);

}

void DiskManager::InitMapBlock(SuperBlockDisk &s_block) 
{
    char buff[BLK_SIZE];
    memset(buff, 0, sizeof(buff));
    ::lseek(disk_fd, 1 *  BLK_SIZE, SEEK_SET);
    for (int i = 0; i < s_block.i_map_blocks; i++) {
        ::write(disk_fd, buff, BLK_SIZE);
    }

    ::lseek(disk_fd, (1 + s_block.i_map_blocks) *  BLK_SIZE, SEEK_SET);
    for (int i = 0; i < s_block.d_map_blocks; i++) {
        ::write(disk_fd, buff, BLK_SIZE);
    }
    
}

int DiskManager::GetBlkIdxByInodeIdx(SuperBlockDisk &s_block, int inode_num) 
{
    return 1 + s_block.i_map_blocks + s_block.d_map_blocks + inode_num - 1;
} 

int DiskManager::GetBlkIdxByDataIdx(SuperBlockDisk &s_block, int data_blk_num) 
{
    return 1 + s_block.i_map_blocks + s_block.d_map_blocks + s_block.i_blocks + data_blk_num - 1;
} 

void DiskManager::SetMapStatus(int startBlkIdx, int mapIdx, int8_t status) 
{
    int blkIdx = startBlkIdx + mapIdx % BLK_SIZE;
    int8_t map[BLK_SIZE];
    memset(map, 0, sizeof(map));
    ::lseek(disk_fd, blkIdx *  BLK_SIZE, SEEK_SET);
    ::read(disk_fd, map, BLK_SIZE);
    map[mapIdx % BLK_SIZE] = status;
    
    ::lseek(disk_fd, blkIdx *  BLK_SIZE, SEEK_SET);
    ::write(disk_fd, map, BLK_SIZE);

}

DiskManager::~DiskManager()
{
    close(disk_fd);
}