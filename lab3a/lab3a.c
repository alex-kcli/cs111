#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ext2_fs.h"

int fd = 0;
struct ext2_super_block super;
struct ext2_group_desc group;

unsigned int block_size = 0, inode_size = 0, blocks_per_group = 0, inodes_per_group = 0;
unsigned int free_block_bitmap = 0, free_inode_bitmap = 0, first_inodes = 0;

void superblock_summary();
void group_summary();
void free_block_entries();
void free_I_node_entries();
void I_node_summary();
void directory_entries();
void indirect_block_references();


void superblock_summary()
{
    unsigned int blocks_count = 0, inodes_count = 0, first_non_reserved = 0;
    
    //Note: Need to check the return value of pread to make sure it reads the //specified size.
    int res = pread(fd, &super, sizeof(super), 1024);
    if (res < 0) {
        fprintf(stderr, "Error: pread. \n");
        exit(2);
    }
    
    blocks_count = super.s_blocks_count;
    inodes_count = super.s_inodes_count;
    block_size = 1024 << super.s_log_block_size;
    inode_size = super.s_inode_size;
    blocks_per_group = super.s_blocks_per_group;
    inodes_per_group = super.s_inodes_per_group;
    first_non_reserved = super.s_first_ino;
    
    fprintf(stdout, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", blocks_count, inodes_count, block_size, inode_size, blocks_per_group, inodes_per_group, first_non_reserved);
}



void group_summary()
{
    unsigned int num_free_blocks = 0, num_free_inodes = 0, group_number = 0, group_num_blocks = 0, group_num_inodes = 0;
    
    int res = pread(fd, &group, sizeof(group), block_size + 1024);
    if (res < 0) {
        fprintf(stderr, "Error: pread. \n");
        exit(2);
    }
    
    group_number = 0;
    group_num_blocks = super.s_blocks_count;
    group_num_inodes = super.s_inodes_count;
    num_free_blocks = group.bg_free_blocks_count;
    num_free_inodes = group.bg_free_inodes_count;
    free_block_bitmap = group.bg_block_bitmap;
    free_inode_bitmap = group.bg_inode_bitmap;
    first_inodes = group.bg_inode_table;
    
    fprintf(stdout, "GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n", group_number, group_num_blocks, group_num_inodes, num_free_blocks, num_free_inodes, free_block_bitmap, free_inode_bitmap, first_inodes);
}



void free_block_entries()
{
    char block[block_size];
    int res = pread(fd, block, block_size, free_block_bitmap * block_size);
    if (res < 0) {
        fprintf(stderr, "Error: pread. \n");
        exit(2);
    }
    
    unsigned i = 0, j = 0;
    for (i = 0; i < block_size; i++) {
        char curr = block[i];
        for (j = 0; j < 8; j++) {
            int free = ((curr >> j) & 1);
            if (free == 0) {
                fprintf(stdout, "BFREE,%d\n", (i*8+j+1));
            }
        }
    }
}



void free_I_node_entries()
{
    char block[block_size];
    int res = pread(fd, block, block_size, free_inode_bitmap * block_size);
    if (res < 0) {
        fprintf(stderr, "Error: pread. \n");
        exit(2);
    }
    
    unsigned i = 0, j = 0;
    for (i = 0; i < block_size; i++) {
        char curr = block[i];
        for (j = 0; j < 8; j++) {
            int free = ((curr >> j) & 1);
            if (free == 0) {
                fprintf(stdout, "IFREE,%d\n", (i*8+j+1));
            }
        }
    }
}



char get_file_type(__u16 i_mode)
{
    if (S_ISDIR(i_mode))
        return 'd';
    else if (S_ISREG(i_mode))
        return 'f';
    else if (S_ISLNK(i_mode))
        return 's';
    else
        return '?';
}



void I_node_summary()
{
    struct ext2_inode inode_arr[inodes_per_group];
    int res = pread(fd, inode_arr, (inodes_per_group * sizeof(struct ext2_inode)), (first_inodes * block_size));
    if (res < 0) {
        fprintf(stderr, "Error: pread. \n");
        exit(2);
    }
    int inode_num = 1;
    
    for (unsigned int i = 0; i < inodes_per_group; i++, inode_num++) {
        struct ext2_inode inode = inode_arr[i];
        if (inode.i_mode != 0 && inode.i_links_count != 0) {
            
            char file_type = get_file_type(inode.i_mode);
            __u16 mode = inode.i_mode & 0xFFF;
            __u16 owner = inode.i_uid;
            __u16 group = inode.i_gid;
            __u16 link_count = inode.i_links_count;
            
            time_t change_raw_time = inode.i_ctime;
            struct tm *crtime;
            crtime = gmtime(&change_raw_time);
            char change_time[25];
            strftime(change_time, 25, "%m/%d/%y %H:%M:%S", crtime);
            
            time_t modifi_raw_time = inode.i_mtime;
            struct tm *mrtime;
            mrtime = gmtime(&modifi_raw_time);
            char modifi_time[25];
            strftime(modifi_time, 25, "%m/%d/%y %H:%M:%S", mrtime);
            
            time_t access_raw_time = inode.i_atime;
            struct tm *artime;
            artime = gmtime(&access_raw_time);
            char access_time[25];
            strftime(access_time, 25, "%m/%d/%y %H:%M:%S", artime);
            
            __u32 file_size = inode.i_size;
            __u32 num_blocks = inode.i_blocks;
            fprintf(stdout, "INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d",
                    inode_num, file_type, mode, owner, group, link_count,
                    change_time, modifi_time, access_time, file_size, num_blocks);
            
            if (file_type == 's' && file_size < 60) {
//                fprintf(stdout, ",%d\n", inode.i_block[0]);
                fprintf(stdout, "\n");
            }
            else {
                for (int i = 0; i < 15; i++) {
                    fprintf(stdout, ",%d", inode.i_block[i]);
                }
                fprintf(stdout, "\n");
            }
        }
    }
}


void directory_entries()
{
    struct ext2_inode inode_arr[inodes_per_group];
    int res = pread(fd, inode_arr, (inodes_per_group * sizeof(struct ext2_inode)), (first_inodes * block_size));
    if (res < 0) {
        fprintf(stderr, "Error: pread. \n");
        exit(2);
    }
    int inode_num = 1;
    
    for (unsigned int i = 0; i < inodes_per_group; i++, inode_num++) {
        struct ext2_inode inode = inode_arr[i];
        
        if (inode.i_mode != 0 && inode.i_links_count != 0) {
            char file_type = get_file_type(inode.i_mode);
            
            if (file_type == 'd') {
                for (int i = 0; i < 12; i++) {
                    if (inode.i_block[i] != 0) {
                        for (unsigned int offset = 0; offset < block_size; ) {
                            struct ext2_dir_entry dir_entry;
                            int res = pread(fd, &dir_entry, sizeof(dir_entry), (inode.i_block[i] * block_size) + offset);
                            if (res < 0) {
                                fprintf(stderr, "Error: pread. \n");
                                exit(2);
                            }
                            __u32 inode_num_of_reference_file = dir_entry.inode;
                            __u16 entry_length = dir_entry.rec_len;
                            __u8 name_length = dir_entry.name_len;
                            char name[name_length + 1];
                            for (int i = 0; i < name_length; i++) {
                                name[i] = dir_entry.name[i];
                            }
                            name[name_length] = '\0';
                            
                            if (dir_entry.inode != 0) {
                                fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,'%s'\n",
                                        inode_num, (i * block_size + offset), inode_num_of_reference_file, entry_length,
                                        name_length, name);
                            }
                            offset += entry_length;
                        }
                    }
                }
            }
        }
    }
}



int get_base_offset(int level) {
    if (level == 1)
        return 12;
    else if (level == 2)
        return 12 + block_size/4;
    else if (level == 3)
        return 12 + block_size/4 + ((block_size/4) * (block_size/4));
    else
        return 0;
}


int get_offset(int level, unsigned offset) {
    if (level == 1)
        return offset;
    else if (level == 2)
        return offset * block_size/4;
    else if (level == 3)
        return offset * (block_size/4) * (block_size/4);
    else
        return 0;
}


void scan_indirect(int level, int inode_num, int block_num, int ori_level, char file_type)
{
    if (level == 0)
        return;
    
    __u32 block_data[block_size / 4];
    int res = pread(fd, block_data, sizeof(block_data), block_num * block_size);
    if (res < 0) {
        fprintf(stderr, "Error: pread. \n");
        exit(2);
    }
    
    for(unsigned int offset = 0; offset < block_size / 4; offset++)
    {
        if(block_data[offset] != 0) {
            fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
                    inode_num, level, (get_base_offset(ori_level) + get_offset(level, offset)), block_num, block_data[offset]);
            
            if (level == 1 && file_type == 'd') {
                for (unsigned int oset = 0; oset < block_size; ) {
                    struct ext2_dir_entry dir_entry;
                    int res = pread(fd, &dir_entry, sizeof(dir_entry), (block_data[offset] * block_size) + oset);
                    if (res < 0) {
                        fprintf(stderr, "Error: pread. \n");
                        exit(2);
                    }
                    __u32 inode_num_of_reference_file = dir_entry.inode;
                    __u16 entry_length = dir_entry.rec_len;
                    __u8 name_length = dir_entry.name_len;
                    char name[name_length + 1];
                    for (int i = 0; i < name_length; i++) {
                        name[i] = dir_entry.name[i];
                    }
                    name[name_length] = '\0';
                    
                    if (dir_entry.inode != 0) {
                        fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,'%s'\n",
                                inode_num, (((get_base_offset(ori_level) + get_offset(level, offset)) * block_size) + oset),
                                inode_num_of_reference_file, entry_length,
                                name_length, name);
                    }
                    oset += entry_length;
                }
            }
            
            scan_indirect(level - 1, inode_num, block_data[offset], ori_level, file_type);
        }
    }
}



void indirect_block_references()
{
    struct ext2_inode inode_arr[inodes_per_group];
    int res = pread(fd, inode_arr, (inodes_per_group * sizeof(struct ext2_inode)), (first_inodes * block_size));
    if (res < 0) {
        fprintf(stderr, "Error: pread. \n");
        exit(2);
    }
    int inode_num = 1;
    
    for (unsigned int i = 0; i < inodes_per_group; i++, inode_num++) {
        struct ext2_inode inode = inode_arr[i];
        
        if (inode.i_mode != 0 && inode.i_links_count != 0) {
            char file_type = get_file_type(inode.i_mode);
            
            if (file_type == 'f' || file_type == 'd') {
                if (inode.i_block[12] != 0) {
                    scan_indirect(1, inode_num, inode.i_block[12], 1, file_type);
                }
                if (inode.i_block[13] != 0) {
                    scan_indirect(2, inode_num, inode.i_block[13], 2, file_type);
                }
                if (inode.i_block[14] != 0) {
                    scan_indirect(3, inode_num, inode.i_block[14], 3, file_type);
                }
            }
        }
    }
}


int main(int argc, char *argv[])
{
    if(argc > 2) {
        fprintf(stderr, "Too many arguments. \n");
        exit(1);
    }
    
    if(argc < 2) {
        fprintf(stderr, "Too little arguments. \n");
        exit(1);
    }
    
    fd = open(argv[1], O_RDONLY);
    if (fd < 0){
        fprintf(stderr, "Failed to read from file. \n");
        exit(2);
    }
    
    superblock_summary();
    group_summary();
    free_block_entries();
    free_I_node_entries();
    I_node_summary();
    directory_entries();
    indirect_block_references();
}
