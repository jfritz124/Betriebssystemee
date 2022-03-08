//
// Created by user on 10.12.21.
//

#ifndef MYFS_DMAP_H
#define MYFS_DMAP_H
#include "myfs-structs.h"
#include "blockdevice.h"




class DMAP{
private:
    BlockDevice *myDevice;
    bool dmapArray[NUMBER_DATA_BLOCKS];

public:
    DMAP(BlockDevice *device);
    ~DMAP();
    bool getBlock(int);
    void setBlock(int, bool);
    //bool[] getFreeBlocksArray(int);
    int getNextFreeBlockFrom(int);
    int getFirstFreeBlock();
    int* getCertainNumberOfFreeBlocks(int);
    int getNumberFreeBlocks();
    void discWrite(int);
    void init();
    void firstInit();
};
#endif //MYFS_DMAP_H