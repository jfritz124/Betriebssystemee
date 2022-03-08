//
// Created by user on 10.12.21.
//
#include <FAT.h>


//Constructor FAT
FAT::FAT(BlockDevice *device) {
    this->myDevice = device;
}

//Deconstructor FAT
FAT::~FAT() {

}

int FAT::getNext(int blockNr) {
    return fatArray[blockNr];

}

int FAT::setNext(int blockNr, int nextBlockNr) {

    fatArray[blockNr] = nextBlockNr;
    discWrite(blockNr);
}

void FAT::freeBlock(int blockNr) {
    fatArray[blockNr] = FAT_END;
    discWrite(blockNr);
}

// hier wird der Vänderte Eintrag im Array auch auf den Datenspeichr geschrieben
void FAT::discWrite(int blockNr) {
    char buffer[BLOCK_SIZE];
    int blockOnDevice = blockNr / 256;
    int NrInBlock = blockNr % 256;
    int firstAddressInBlock = blockNr - NrInBlock;
    char firstHalf;
    char secondHalf;

    for (int i = 0; i < 256; i++) {
        int currentAddress = fatArray[firstAddressInBlock + i];
        firstHalf = currentAddress & 0xFF;
        secondHalf = ((currentAddress) >> 8) & 0xFF;
        buffer[i * 2] = firstHalf;
        buffer[i * 2 + 1] = secondHalf;

    }
    myDevice->write(blockOnDevice, buffer);

}

// die FAT wird koplett aus dem Block Device gelesen und in das Array gepackt.
void FAT::init() {
    char buffer[BLOCK_SIZE];
    for (int i = 0; i < FAT_SIZE; i++) {
        myDevice->read(i, buffer);
        for (int j = 0; j < 256; j++) {
            int address = 0;
            std::memcpy(&address, buffer + j * 2, 2);
            fatArray[j + i * 256] = address;
        }

    }
}




