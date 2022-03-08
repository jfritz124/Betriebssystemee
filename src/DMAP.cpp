//
// Created by user on 10.12.21.
//
#include "DMAP.h"
#include "myfs-structs.h"

DMAP::DMAP(BlockDevice *device) {
    this->myDevice = device;
}

DMAP::~DMAP() {

}

//TODO ausgeklammerte Methoden löschen und auch in der h datei entfernen

/**
 * Setzt ein Block an der übergebenen Blocknummer auf TRUE oder FALSE
 *
 * @param blocknumber
 * @param entry
 */
void DMAP::setBlock(int blocknumber, bool entry) {
    if (blocknumber < NUMBER_BLOCKS) {
        dmapArray[blocknumber] = entry;
        discWrite(blocknumber);
    }
}

/**
 * gibt den Index des ersten freien Blocks zurück
 *
 * @return
 */
int DMAP::getFirstFreeBlock() {
    int blocknumber = 1;
    while (blocknumber < NUMBER_DATA_BLOCKS) {
        if (!dmapArray[blocknumber]) {
            return blocknumber;
        }
        blocknumber++;
    }
    return -EINVAL; //keine freien Blöcke
}

/**
 * Gibt die Indexe der gewünschten Anzahl an freien Blöcken zurück
 *
 * @param number
 * @return
 */
int *DMAP::getCertainNumberOfFreeBlocks(int number) {
    int *returnArray = new int[number];
    for (int i = 0; i < number; i++) {
        int blockNo = getFirstFreeBlock();
        if (blockNo < 0) return nullptr;
        returnArray[i] = blockNo;
        dmapArray[blockNo] = true;
        discWrite(blockNo);
    }
    return returnArray;
}


void DMAP::discWrite(int dMapArrayIndex) {
    char buffer[BLOCK_SIZE];
    memcpy(buffer, &dmapArray[dMapArrayIndex / BLOCK_SIZE], BLOCK_SIZE);
    this->myDevice->write(DMAP_OFFSET_SIZE + dMapArrayIndex / BLOCK_SIZE, buffer);
}

/**
 * Initialieisert die existierende DMAP
 *
 */
void DMAP::init() {
    char buffer[BLOCK_SIZE];

    for (int i = 0; i < NUMBER_DATA_BLOCKS; i++) { //Iteriert über das ganze dmapArray
        this->myDevice->read(DMAP_OFFSET_SIZE + i, buffer);

        for (int j = 0; j < BLOCK_SIZE; j++) {
            int index = j + BLOCK_SIZE * i;
            if (index < NUMBER_DATA_BLOCKS) {
                std::memcpy(&dmapArray[index], buffer + j, sizeof(bool));
            }
        }
    }
}