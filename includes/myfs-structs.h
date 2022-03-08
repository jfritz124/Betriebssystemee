//
//  myfs-structs.h
//  myfs
//
//  Created by Oliver Waldhorst on 07.09.17.
//  Copyright © 2017 Oliver Waldhorst. All rights reserved.
//
#include <string>


#include <ctime>// time_t wird zu String "Www Mmm dd hh:mm:ss yyyy"
#include <cstring>
#ifndef myfs_structs_h
#define myfs_structs_h

#define NAME_LENGTH 255
#define BLOCK_SIZE 512
#define NUM_DIR_ENTRIES 64
#define NUM_OPEN_FILES 64

#define FILE_SMALL_SIZE 1024
#define FILE_BIG_SIZE 2048

#define NUMBER_BLOCKS 65536
#define NUMBER_DATA_BLOCKS 55912
#define FAT_END 0
#define FAT_SIZE 256
#define DATA_ALL FAT_SIZE + FILE_BIG_SIZE + NUM_DIR_ENTRIES
#define DATA_OFFSET ROOT_DIR_OFFSET + ROOT_SIZE

#define DMAP_SIZE 128
#define ROOT_SIZE 240
#define ROOT_DIR_OFFSET FAT_SIZE+DMAP_SIZE
#define DMAP_OFFSET_SIZE FAT_SIZE

#include "blockdevice.h"
#include <sys/stat.h>

struct MyFsFileInfoInMemory {
    //char name[NAME_LENGTH];
    std::string name; //ToDo
    size_t size;
    char * data;
    bool used;
    time_t atime; // Zeitpunkt letzter Zugriff
    time_t mtime; // Zeitpunkt letzte Veränderung
    time_t ctime; //letzte Statusänderung
    uid_t uid;	//User-ID File Owner
    gid_t gid; 	//Group-ID File Owner
    mode_t mode; // Zugrifsberechtigungen
};


struct MyFsFileInfo {
    std::string name; //ToDo
    size_t size;
    char * data;
    char * path;
    bool used;
    time_t atime; // Zeitpunkt letzter Zugriff
    time_t mtime; // Zeitpunkt letzte Veränderung
    time_t ctime; //letzte Statusänderung
    uid_t uid;	//User-ID File Owner
    gid_t gid; 	//Group-ID File Owner
    mode_t mode; // Zugrifsberechtigungen
};

struct rootFile {
    char name[NAME_LENGTH];
    int firstBlock;
    struct stat fileStats = {};
    int indexRootDirBlock;
    bool valid;
};

struct openFile{
    rootFile *file;
};

#endif /* myfs_structs_h */
