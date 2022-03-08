//
// Created by user on 10.12.21.
//

#include <map>
#include "blockdevice.h"
#include "myfs-structs.h"

#ifndef MYFS_ROOT_H
#define MYFS_ROOT_H




class Root {
private:
    BlockDevice *blockDevice;
    rootFile* rootFiles[NUM_DIR_ENTRIES];

public:
    Root(BlockDevice *blockDevice);
    ~Root();

    void initRootDir();
    void init();
    bool discWrite(rootFile* file);

    rootFile* getFileAtIndex(int index);
    rootFile* getRootEntryFile(const char* path);

    rootFile* createNewFile(const char* path);
    int deleteFile(const char* path);
};
#endif //MYFS_ROOT_H