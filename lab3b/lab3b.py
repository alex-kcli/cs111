#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Mon Mar  1 19:03:02 2021

@author: kangchengli
"""
import sys, csv

superblock_list = []
group_list = []
BFree_list = []
IFree_list = []
inode_list = []
dirent_list = []
indirect_list = []
block_dict = dict()
inode_num_list = []
dirent_dict = dict()
parent_dict = dict()
reserved = [0, 1, 2, 3, 4, 5, 6, 7]


class Superblock:
    def __init__(self,row):
        self.blocks_count = int(row[1])
        self.inodes_count = int(row[2])
        self.block_size = int(row[3])
        self.inode_size = int(row[4])
        self.blocks_per_group = int(row[5])
        self.inodes_per_group = int(row[6])
        self.first_non_reserved_inode = int(row[7])


class Group:
    def __init__(self,row):
        self.group_num = int(row[1])
        self.group_num_of_blocks = int(row[2])
        self.group_num_of_inodes = int(row[3])
        self.num_free_blocks = int(row[4])
        self.num_free_inodes = int(row[5])
        self.block_num_block_bitmap = int(row[6])
        self.block_num_inode_bitmap = int(row[7])
        self.block_num_first_block_inode = int(row[8])


class Inode:
    def __init__(self,row):
        self.inode_num = int(row[1])
        self.file_type = row[2]
        self.mode = int(row[3])
        self.owner = int(row[4])
        self.group = int(row[5])
        self.link_count = int(row[6])
        self.change_time = row[7]
        self.modify_time = row[8]
        self.access_time = row[9]
        self.file_size = int(row[10])
        
        direct_blocks_list = []
        if self.file_type != 's':
            for i in range(12, 24):
                direct_blocks_list.append(int(row[i]))
        self.direct_blocks = direct_blocks_list
        
        indirect_blocks_list = []
        if self.file_type != 's':
            for i in range(24, 27):
                indirect_blocks_list.append(int(row[i]))
        self.indirect_blocks = indirect_blocks_list


class Dirent:
    def __init__(self,row):
        self.parent_inode_num = int(row[1])
        self.logical_byte_offset = int(row[2])
        self.inode_num = int(row[3])
        self.entry_length = int(row[4])
        self.name_length = int(row[5])
        self.name = row[6]
        
        
class Indirect:
    def __init__(self,row):
        self.inode_num = int(row[1])
        self.level_indirection = int(row[2])
        self.logical_block_offset = int(row[3])
        self.indirect_block_num = int(row[4])
        self.referenced_block_num = int(row[5])


def main():
    flag = 0
    
    if len(sys.argv) != 2:
        sys.stderr.write("Incorrect Number of Arguments, Expected 2.\n")
        sys.exit(1)
    
    try:
        csvfile = open(sys.argv[1], 'r')
    except:
        sys.stderr.write("Failed to open csv file.\n")
        sys.exit(1)
        
        
    csv_reader = csv.reader(csvfile)
    for row in csv_reader:
        first_entry = row[0]
        if first_entry == "SUPERBLOCK":
            superblock_list.append(Superblock(row))
            
        elif first_entry == "GROUP":
            group_list.append(Group(row))
            
        elif first_entry == "BFREE":
            BFree_list.append(int(row[1]))
            
        elif first_entry == "IFREE":
            IFree_list.append(int(row[1]))
            
        elif first_entry == "INODE":
            inode_list.append(Inode(row))
            
        elif first_entry == "DIRENT":
            dirent_list.append(Dirent(row))
            
        elif first_entry == "INDIRECT":
            indirect_list.append(Indirect(row))
    
    
    
    
    
    superblock = superblock_list[0]
    
    for inode in inode_list:
        if inode.file_type == 'f' or inode.file_type == 'd' or inode.file_type == 's':
            inode_num_list.append(inode.inode_num)
            
            
        if inode.file_type != 's':
            for i in range(12):
                block_address = inode.direct_blocks[i]
                if block_address == 0:
                    continue
                if block_address < 0 or block_address > superblock.blocks_count:
                    print("INVALID BLOCK " + str(block_address) + " IN INODE " + str(inode.inode_num) + " AT OFFSET " + str(i))
                    flag = 1
                if block_address in reserved and block_address > 0:
                    print("RESERVED BLOCK " + str(block_address) + " IN INODE " + str(inode.inode_num) + " AT OFFSET " + str(i))
                    flag = 1
            
                if block_address in block_dict:
                    block_dict[block_address].append([0, inode.inode_num, i])
                else:
                    block_dict[block_address] = [[0, inode.inode_num, i]]
            
            
        if inode.file_type != 's':
            for i in range(3):
                indirect_address = inode.indirect_blocks[i]
                if i == 0:
                    if indirect_address == 0:
                        continue
                    if indirect_address < 0 or indirect_address > superblock.blocks_count:
                        print("INVALID INDIRECT BLOCK " + str(indirect_address) + " IN INODE " + str(inode.inode_num) + " AT OFFSET 12")
                        flag = 1
                    if indirect_address in reserved and indirect_address > 0:
                        print("RESERVED INDIRECT BLOCK " + str(indirect_address) + " IN INODE " + str(inode.inode_num) + " AT OFFSET 12")
                        flag = 1
                    
                    if indirect_address in block_dict:
                        block_dict[indirect_address].append([1, inode.inode_num, 12])
                    else:
                        block_dict[indirect_address] = [[1, inode.inode_num, 12]]
            
                if i == 1:
                    if indirect_address == 0:
                        continue
                    if indirect_address < 0 or indirect_address > superblock.blocks_count:
                        print("INVALID DOUBLE INDIRECT BLOCK " + str(indirect_address) + " IN INODE " + str(inode.inode_num) + " AT OFFSET 268")
                        flag = 1
                    if indirect_address in reserved and indirect_address > 0:
                        print("RESERVED DOUBLE INDIRECT BLOCK " + str(indirect_address) + " IN INODE " + str(inode.inode_num) + " AT OFFSET 268")
                        flag = 1
                    if indirect_address in block_dict:
                        block_dict[indirect_address].append([2, inode.inode_num, 268])
                    else:
                        block_dict[indirect_address] = [[2, inode.inode_num, 268]]
            
                if i == 2:
                    if indirect_address == 0:
                        continue
                    if indirect_address < 0 or indirect_address > superblock.blocks_count:
                        print("INVALID TRIPLE INDIRECT BLOCK " + str(indirect_address) + " IN INODE " + str(inode.inode_num) + " AT OFFSET 65804")
                        flag = 1
                    if indirect_address in reserved and indirect_address > 0:
                        print("RESERVED TRIPLE INDIRECT BLOCK " + str(indirect_address) + " IN INODE " + str(inode.inode_num) + " AT OFFSET 65804")
                        flag = 1
                    if indirect_address in block_dict:
                        block_dict[indirect_address].append([3, inode.inode_num, 65804])
                    else:
                        block_dict[indirect_address] = [[3, inode.inode_num, 65804]]
        
        
        
    for indirect in indirect_list:
        level = indirect.level_indirection
        block_address = indirect.referenced_block_num
        inode_num = indirect.inode_num
        offset = indirect.logical_block_offset
            
        if level == 1:
            if block_address == 0:
                continue
            if block_address < 0 or block_address > superblock.blocks_count:
                print("INVALID INDIRECT BLOCK " + str(block_address) + " IN INODE " + str(inode_num) + " AT OFFSET " + str(offset))
                flag = 1
            if block_address in reserved and block_address > 0:
                print("RESERVED INDIRECT BLOCK " + str(block_address) + " IN INODE " + str(inode_num) + " AT OFFSET " + str(offset))
                flag = 1
            if block_address in block_dict:
                block_dict[block_address].append([1, inode_num, offset])
            else:
                block_dict[block_address] = [[1, inode_num, offset]]
            
        if level == 2:
            if block_address == 0:
                continue
            if block_address < 0 or block_address > superblock.blocks_count:
                print("INVALID DOUBLE INDIRECT BLOCK " + str(block_address) + " IN INODE " + str(inode_num) + " AT OFFSET " + str(offset))
                flag = 1
            if block_address in reserved and block_address > 0:
                print("RESERVED DOUBLE INDIRECT BLOCK " + str(block_address) + " IN INODE " + str(inode_num) + " AT OFFSET " + str(offset))
                flag = 1
            if block_address in block_dict:
                block_dict[block_address].append([2, inode_num, offset])
            else:
                block_dict[block_address] = [[2, inode_num, offset]]
            
        if level == 3:    
            if block_address == 0:
                continue
            if block_address < 0 or block_address > superblock.blocks_count:
                print("INVALID TRIPLE INDIRECT BLOCK " + str(block_address) + " IN INODE " + str(inode_num) + " AT OFFSET " + str(offset))
                flag = 1
            if block_address in reserved and block_address > 0:
                print("RESERVED TRIPLE INDIRECT BLOCK " + str(block_address) + " IN INODE " + str(inode_num) + " AT OFFSET " + str(offset))
                flag = 1
            if block_address in block_dict:
                block_dict[block_address].append([3, inode_num, offset])
            else:
                block_dict[block_address] = [[3, inode_num, offset]]
            
            
            
    for i in range(8, superblock.blocks_count):
        if i not in block_dict and i not in BFree_list:
            print("UNREFERENCED BLOCK " + str(i))
            flag = 1
        if i in block_dict and i in BFree_list:
            print("ALLOCATED BLOCK " + str(i) + " ON FREELIST")
            flag = 1
            
            
            
    for block_num in block_dict:
        contents = block_dict[block_num]
        if len(contents) > 1:
            for content in contents:
                level = content[0]
                if level == 0:
                    print("DUPLICATE BLOCK " + str(block_num) + " IN INODE " + str(content[1]) + " AT OFFSET " + str(content[2]))
                    flag = 1
                if level == 1:
                    print("DUPLICATE INDIRECT BLOCK " + str(block_num) + " IN INODE " + str(content[1]) + " AT OFFSET " + str(content[2]))
                    flag = 1
                if level == 2:
                    print("DUPLICATE DOUBLE INDIRECT BLOCK " + str(block_num) + " IN INODE " + str(content[1]) + " AT OFFSET " + str(content[2]))
                    flag = 1
                if level == 3:
                    print("DUPLICATE TRIPLE INDIRECT BLOCK " + str(block_num) + " IN INODE " + str(content[1]) + " AT OFFSET " + str(content[2]))
                    flag = 1
    
    
    
    for i in range(1, superblock.inodes_count):
        if i in inode_num_list and i in IFree_list:
            print("ALLOCATED INODE " + str(i) + " ON FREELIST")
            flag = 1
    
    for i in range(superblock.first_non_reserved_inode, superblock.inodes_count):
        if i not in inode_num_list and i not in IFree_list:
            print("UNALLOCATED INODE " + str(i) + " NOT ON FREELIST")
            flag = 1
    
    
    
    for dirent in dirent_list:
        inode_num = dirent.inode_num
        if inode_num in dirent_dict:
            dirent_dict[inode_num] += 1
        else:
            dirent_dict[inode_num] = 1
            
        if dirent.name != "'.'" and dirent.name != "'..'":
            parent_dict[inode_num] = dirent.parent_inode_num    
    parent_dict[2] = 2
        
        
        
    for inode in inode_list:
        link_count = inode.link_count
        inode_num = inode.inode_num
        if inode_num not in dirent_dict:
            print("INODE " + str(inode_num) + " HAS 0 LINKS BUT LINKCOUNT IS " + str(link_count))
            flag = 1
        elif link_count != dirent_dict[inode_num]:
            print("INODE " + str(inode_num) + " HAS " + str(dirent_dict[inode_num]) + " LINKS BUT LINKCOUNT IS " + str(link_count))
            flag = 1
    
    
    
    for dirent in dirent_list:
        inode_num = dirent.inode_num
        if inode_num < 1 or inode_num > superblock.inodes_count:
            print("DIRECTORY INODE " + str(dirent.parent_inode_num) + " NAME " + dirent.name + " INVALID INODE " + str(inode_num))
            flag = 1
        elif inode_num not in inode_num_list:
            print("DIRECTORY INODE " + str(dirent.parent_inode_num) + " NAME " + dirent.name + " UNALLOCATED INODE " + str(inode_num))
            flag = 1
            
        if dirent.name == "'.'" and dirent.inode_num != dirent.parent_inode_num:
            print("DIRECTORY INODE " + str(dirent.parent_inode_num) + " NAME '.' LINK TO INODE " + str(dirent.inode_num) + " SHOULD BE " + str(dirent.parent_inode_num))
            flag = 1
        if dirent.name == "'..'" and dirent.inode_num != parent_dict[dirent.parent_inode_num]:
            print("DIRECTORY INODE " + str(dirent.parent_inode_num) + " NAME '..' LINK TO INODE " + str(dirent.inode_num) + " SHOULD BE " + str(parent_dict[dirent.parent_inode_num]))
            flag = 1
            


    if flag == 1:
        sys.exit(2)
    else:
        sys.exit(0)


if __name__ == '__main__':
    main()