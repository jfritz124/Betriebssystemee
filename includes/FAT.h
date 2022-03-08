//
// Created by user on 10.12.21.
//

#ifndef MYFS_FAT_H
#define MYFS_FAT_H


#include <myfs-structs.h>

class FAT {
private:
    BlockDevice *myDevice;
    int fatArray[NUMBER_BLOCKS];

public:
    FAT(BlockDevice *device);
    ~FAT();

    int setNext(int blockNr, int nextBlockNr);
    int getNext(int blockNr);
    void freeBlock(int blockNr);
    void init();
    void discWrite(int blockNr);
};
#endif //MYFS_FAT_H