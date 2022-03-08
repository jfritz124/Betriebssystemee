#include <cstring>
#include <fuse.h>
#include <unistd.h>
#include "Root.h"
#include "myfs-structs.h"

Root::Root(BlockDevice *blockDevice) {
    this->blockDevice = blockDevice;
    for (int i = 0; i < NUM_DIR_ENTRIES; i++) {
        rootFiles[i] = nullptr;
    }
}

Root::~Root() {

}

void Root::init() {
    for (int i = 0; i < NUM_DIR_ENTRIES; i++) {
        rootFile r = rootFile();
        r.valid = false;
        r.indexRootDirBlock = i;
        this->discWrite(&r);
    }

}


void Root::initRootDir() {
    char buff[BLOCK_SIZE] = {};
    for (int i = 0; i < NUM_DIR_ENTRIES; i++) {
        auto *file = new rootFile();
        this->blockDevice->read(ROOT_DIR_OFFSET + i, buff);
        (void) std::memcpy(file, buff, sizeof(rootFile));
        if (file->valid) {
            rootFiles[i] = file;
        } else {
            rootFiles[i] = nullptr;
            free(file);
        }
    }
}

bool Root::discWrite(rootFile *file) {
    char buff[BLOCK_SIZE] = {};
    //void* memcpy( void* dest, const void* src, std::size_t count );
    // dest 	- 	pointer to the memory location to copy to
    // src 	- 	pointer to the memory location to copy from
    // count 	- 	number of bytes to copy
    std::memcpy(buff, file, sizeof(rootFile));
    this->blockDevice->write(ROOT_DIR_OFFSET + file->indexRootDirBlock, buff);
    return true;
}

rootFile *Root::getFileAtIndex(int index) {
    if (index < NUM_DIR_ENTRIES) {
        return rootFiles[index];
    }
    return nullptr;
}

rootFile *Root::getRootEntryFile(const char *path) {
    path++;
    for (auto const &rootFile: rootFiles) {
        if (rootFile != nullptr && strcmp(path, rootFile->name) == 0) {
            return rootFile;
        }
    }
    return nullptr;
}


rootFile *Root::createNewFile(const char *path) {
    path++;
    int i = 0;

    while (rootFiles[i] != nullptr) {
        i++;
    }
    if (i < NUM_DIR_ENTRIES) {

        auto *newFile = new rootFile();
        strcpy(newFile->name, path);
        newFile->firstBlock = FAT_END;
        newFile->valid = true;

        newFile->fileStats.st_mode = S_IFREG | 0644;
        newFile->fileStats.st_blksize = BLOCK_SIZE;
        newFile->fileStats.st_size = 0;
        newFile->fileStats.st_nlink = 1;
        newFile->fileStats.st_atime = time(nullptr);
        newFile->fileStats.st_mtime = time(nullptr);
        newFile->fileStats.st_ctime = time(nullptr);
        newFile->fileStats.st_uid = getuid();
        newFile->fileStats.st_gid = getgid();

        newFile->indexRootDirBlock = i;

        rootFiles[i] = newFile;
        return newFile;
    } else {
        return nullptr;
    }


}

int Root::deleteFile(const char *path) {
    int ret = -ENOENT;
    path++;
    for (int i = 0; i < NUM_DIR_ENTRIES; i++) {
        if (rootFiles[i] != nullptr && strcmp(path, rootFiles[i]->name) == 0) {
            delete rootFiles[i];
            rootFiles[i] = nullptr;
            rootFile r = rootFile();
            r.valid = false;
            r.indexRootDirBlock = i;
            this->discWrite(&r);
            ret = 0;
        }
    }
    return ret;
}




