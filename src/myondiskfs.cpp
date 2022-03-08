//
// Created by Oliver Waldhorst on 20.03.20.
// Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

#include "myondiskfs.h"

// For documentation of FUSE methods see https://libfuse.github.io/doxygen/structfuse__operations.html

#undef DEBUG

// TODO: Comment lines to reduce debug messages
#define DEBUG
#define DEBUG_METHODS
#define DEBUG_RETURN_VALUES

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "macros.h"
#include "myfs.h"
#include "myfs-info.h"
#include "blockdevice.h"
#include "myinmemoryfs.h"


/// @brief Constructor of the on-disk file system class.
///
/// You may add your own constructor code here.
MyOnDiskFS::MyOnDiskFS() : MyFS() {
    // create a block device object
    this->blockDevice = new BlockDevice(BLOCK_SIZE);

    root = new Root(blockDevice);
    buffer = new char[BLOCK_SIZE];
    dmap = new DMAP(blockDevice);
    fat = new FAT(blockDevice);
    for (int i = 0; i < NUM_OPEN_FILES; i++) {
        openFiles[i] = nullptr;
    }
}

/// @brief Destructor of the on-disk file system class.
///
/// You may add your own destructor code here.
MyOnDiskFS::~MyOnDiskFS() {
    // free block device object
    delete root;
    delete fat;
    delete dmap;

    delete this->blockDevice;


}

/// @brief Create a new file.
///
/// Create a new file with given name and permissions.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode Permissions for file access.
/// \param [in] dev Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseMknod(const char *path, mode_t mode, dev_t dev) {
    LOGM();
    int ret = 0;
    std::string name = std::string(path);
    if (root->getRootEntryFile(path) != nullptr) {
        ret = -EEXIST;
    } else if (name.size() > NAME_LENGTH) {
        ret = -EINVAL;
    } else {
        rootFile *file = root->createNewFile(path);
        if (file == nullptr) {
            ret = -EINVAL;
            RETURN(ret); //TODO error code no space
        }
        this->root->discWrite(file);
    }
    RETURN(ret);
}


/// @brief Delete a file.
///
/// Delete a file with given name from the file system.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseUnlink(const char *path) {
    LOGM();

    int ret = 0;
    rootFile *file = root->getRootEntryFile(path);
    if (file == nullptr) {
        ret = -ENOENT;
    } else {
        LOGF("firstFAT: %d", file->firstBlock);
        if (file->firstBlock != FAT_END) {
            int actualBlock = file->firstBlock;

            while (actualBlock != FAT_END) {
                int nextBlock = fat->getNext(actualBlock);
                dmap->setBlock(actualBlock, false);
                fat->freeBlock(actualBlock);
                actualBlock = nextBlock;
            }
        }
        root->deleteFile(path);

    }

    RETURN(ret);

}

/// @brief Rename a file.
///
/// Rename the file with with a given name to a new name.
/// Note that if a file with the new name already exists it is replaced (i.e., removed
/// before renaming the file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newpath  New name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseRename(const char *path, const char *newpath) {
    LOGM();

    int ret = 0;
    newpath++;
    std::string name = std::string(newpath);
    if (name.size() + 1 > NAME_LENGTH) {
        ret = -EINVAL;
    } else if (root->getRootEntryFile(path) == nullptr) {
        ret = -ENOENT;
    } else {
        rootFile *file = root->getRootEntryFile(path);
        strcpy(file->name, newpath);
        root->discWrite(file);
    }
    RETURN(ret);
}

/// @brief Get file meta data.
///
/// Get the metadata of a file (user & group id, modification times, permissions, ...).
/// \param [in] path Name of the file, starting with "/".
/// \param [out] statbuf Structure containing the meta data, for details type "man 2 stat" in a terminal.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseGetattr(const char *path, struct stat *statbuf) {
    LOGM();
    int ret = 0;
    if (strcmp(path, "/") == 0) {
        statbuf->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
        statbuf->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
        statbuf->st_atime = time(NULL); // The last "a"ccess of the file/directory is right now
        statbuf->st_mtime = time(NULL); // The last "m"odification of the file/directory is right now
        statbuf->st_ctime = time(NULL);
        statbuf->st_mode = S_IFDIR | 0755;
        statbuf->st_nlink = 2;
    } else if (root->getRootEntryFile(path) == nullptr) {
        ret = -ENOENT;
    } else {
        rootFile *file = root->getRootEntryFile(path);
        memcpy(statbuf, &file->fileStats, sizeof(*statbuf));
    }
    RETURN(ret);
}

/// @brief Change file permissions.
///
/// Set new permissions for a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode New mode of the file.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseChmod(const char *path, mode_t mode) {
    LOGM();
    int ret = 0;
    if (root->getRootEntryFile(path) == nullptr) {
        ret = -ENOENT;
    } else {
        rootFile *file = root->getRootEntryFile(path);
        file->fileStats.st_mode = mode;
        file->fileStats.st_mtime = time(NULL);
        root->discWrite(file);
    }
    RETURN(ret);
}

/// @brief Change the owner of a file.
///
/// Change the user and group identifier in the meta data of a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] uid New user id.
/// \param [in] gid New group id.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseChown(const char *path, uid_t uid, gid_t gid) {
    LOGM();

    int ret = 0;
    if (root->getRootEntryFile(path) == nullptr) {
        ret = -ENOENT;
    } else {
        rootFile *file = root->getRootEntryFile(path);
        file->fileStats.st_uid = uid;
        file->fileStats.st_gid = gid;
        file->fileStats.st_mtime = time(NULL);
        root->discWrite(file);
    }
    RETURN(ret);
}

/// @brief Open a file.
///
/// Open a file for reading or writing. This includes checking the permissions of the current user and incrementing the
/// open file count.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [out] fileInfo Can be ignored in Part 1
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseOpen(const char *path, struct fuse_file_info *fileInfo) {
    LOGM();
    int ret = 0;
    if (root->getRootEntryFile(path) == nullptr) {
        ret = -ENOENT;
    } else if (openCount == NUM_OPEN_FILES) {
        ret = -EMFILE;
    } else {

        openFile *openFile = new ::openFile();
        openFile->file = root->getRootEntryFile(path);

        int openIndex = getIndexOpen();
        LOGF("openIndex: %d", openIndex);
        openFiles[openIndex] = openFile;
        fileInfo->fh = openIndex;
        openCount++;
    }
    RETURN(ret);
}

/// @brief Read from a file.
///
/// Read a given number of bytes from a file starting form a given position.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// Note that the file content is an array of bytes, not a string. I.e., it is not (!) necessarily terminated by '\0'
/// and may contain an arbitrary number of '\0'at any position. Thus, you should not use strlen(), strcpy(), strcmp(),
/// ... on both the file content and buf, but explicitly store the length of the file and all buffers somewhere and use
/// memcpy(), memcmp(), ... to process the content.
/// \param [in] path Name of the file, starting with "/".
/// \param [out] buf The data read from the file is stored in this array. You can assume that the size of buffer is at
/// least 'size'
/// \param [in] size Number of bytes to read
/// \param [in] offset Starting position in the file, i.e., number of the first byte to read relative to the first byte of
/// the file
/// \param [in] fileInfo Can be ignored in Part 1
/// \return The Number of bytes read on success. This may be less than size if the file does not contain sufficient bytes.
/// -ERRNO on failure.
int MyOnDiskFS::fuseRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    int ret = 0;
    if (root->getRootEntryFile(path) == nullptr) {
        ret = -EEXIST;
    } else if (openFiles[fileInfo->fh] == nullptr) {
        ret = -EBADF;
    } else {
        rootFile *file = root->getRootEntryFile(path);
        if (offset + size > file->fileStats.st_size) {
            size = file->fileStats.st_size - offset;

        }
        LOGF("--> Trying to read %s, %lu, %lu\n", path, (unsigned long) offset, size);

        int offsetBlock = offset / BLOCK_SIZE;
        int blocks = ceil((size + (offset % BLOCK_SIZE)) / (double) BLOCK_SIZE);
        int currentBlock = file->firstBlock;
        int newOffset = 0;

        for (int i = 0; i < offsetBlock; i++) currentBlock = fat->getNext(currentBlock);

        for (int i = 0; i < blocks; i++) {
            char buff[BLOCK_SIZE] = {};
            this->blockDevice->read(currentBlock + DATA_OFFSET, buff);
            if (i == 0) {
                memcpy(buf, &(buff[offset % BLOCK_SIZE]),
                       (size < BLOCK_SIZE - (offset % BLOCK_SIZE) ? size : BLOCK_SIZE - (offset % BLOCK_SIZE)));
                newOffset += BLOCK_SIZE - (offset % BLOCK_SIZE);
            } else if (i == blocks - 1) {
                memcpy(buf + newOffset, buff, size - newOffset);
            } else {
                memcpy(buf + newOffset, buff, BLOCK_SIZE);
                newOffset += BLOCK_SIZE;
            }
            currentBlock = fat->getNext(currentBlock);
        }
        ret = size;
    }
    RETURN(ret)
}

/// @brief Write to a file.
///
/// Write a given number of bytes to a file starting at a given position.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// Note that the file content is an array of bytes, not a string. I.e., it is not (!) necessarily terminated by '\0'
/// and may contain an arbitrary number of '\0'at any position. Thus, you should not use strlen(), strcpy(), strcmp(),
/// ... on both the file content and buf, but explicitly store the length of the file and all buffers somewhere and use
/// memcpy(), memcmp(), ... to process the content.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] buf An array containing the bytes that should be written.
/// \param [in] size Number of bytes to write.
/// \param [in] offset Starting position in the file, i.e., number of the first byte to read relative to the first byte of
/// the file.
/// \param [in] fileInfo Can be ignored in Part 1 .
/// \return Number of bytes written on success, -ERRNO on failure.
int
MyOnDiskFS::fuseWrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    int ret = 0;

    if (openFiles[fileInfo->fh] != nullptr) {
        rootFile *file = openFiles[fileInfo->fh]->file;

        if (size + offset > file->fileStats.st_size) {
            this->setFATBlocks(size, offset, file);
        }
        int offsetBlock = offset / BLOCK_SIZE;
        int blocks = ceil((size + (offset % BLOCK_SIZE)) / (double) BLOCK_SIZE);
        int currentBlock = file->firstBlock;
        int newOffset = 0;

        for (int i = 0; i < offsetBlock; i++) currentBlock = fat->getNext(currentBlock);

        for (int i = 0; i < blocks; i++) {
            char buff[BLOCK_SIZE] = {};
            this->blockDevice->read(currentBlock + DATA_OFFSET, buff);
            if (i == 0) {
                memcpy(&(buff[offset % BLOCK_SIZE]), buf,
                       (size < BLOCK_SIZE - (offset % BLOCK_SIZE) ? size : BLOCK_SIZE - (offset % BLOCK_SIZE)));
                newOffset += BLOCK_SIZE - (offset % BLOCK_SIZE);
                this->blockDevice->write(currentBlock + DATA_OFFSET, buff);
            } else if (i == blocks - 1) {
                memcpy(buff, buf + newOffset, size - newOffset);
                this->blockDevice->write(currentBlock + DATA_OFFSET, buff);
            } else {
                memcpy(buff, buf + newOffset, BLOCK_SIZE);
                newOffset += BLOCK_SIZE;
                this->blockDevice->write(currentBlock + DATA_OFFSET, buff);
            }
            currentBlock = fat->getNext(currentBlock);
        }
        if ((off_t) (offset + size) > file->fileStats.st_size) {
            file->fileStats.st_size = offset + size;
        }
        root->discWrite(file);
        ret = size;
    } else {
        ret = -EBADF;
    }

    RETURN(ret)
}

int MyOnDiskFS::numBlocks(int size) {
    return (size >> 9) + ((size % BLOCK_SIZE) != 0 ? 1 : 0);
}

void MyOnDiskFS::setFATBlocks(size_t size, off_t offset, rootFile *file) {
    int blocks = numBlocks(size + offset) - numBlocks(file->fileStats.st_size); //Blöcke zum schreiben
    int offsetBlock = offset / BLOCK_SIZE; //offset um auf existierendem block zu schreiben
    int blocksAll = blocks + offsetBlock - (file->fileStats.st_size / BLOCK_SIZE); //neue blöcke anhängen
    LOGF("blocks: %d, offsetBlock: %d, blocksAll: %d", blocks, offsetBlock, blocksAll);
    if (blocksAll > 0) {
        //find old last Block
        int *newBlocks = dmap->getCertainNumberOfFreeBlocks(blocksAll);
        int currentBlock = file->firstBlock;
        if (currentBlock == FAT_END) {
            file->firstBlock = newBlocks[0];
        } else {
            while (fat->getNext(currentBlock) != FAT_END) {
                currentBlock = fat->getNext(currentBlock);
            }
            fat->setNext(currentBlock, newBlocks[0]);
        }
        currentBlock = newBlocks[0];
        //set new Blocks
        for (int i = 1; i < blocksAll; i++) {
            fat->setNext(currentBlock, newBlocks[i]);
            currentBlock = newBlocks[i];
        }
        fat->setNext(currentBlock, FAT_END);
        delete[] newBlocks;
    }
}

/// @brief Close a file.
///
/// \param [in] path Name of the file, starting with "/".
/// \param [in] File handel for the file set by fuseOpen.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseRelease(const char *path, struct fuse_file_info *fileInfo) {
    LOGM();
    int ret = 0;
    if (root->getRootEntryFile(path) == nullptr) {
        ret = -ENOENT;
    } else if (openFiles[fileInfo->fh] == nullptr) {
        ret = -EBADF;
    } else {
        int openIndex = fileInfo->fh;
        delete openFiles[openIndex];
        openFiles[openIndex] = nullptr;
        openCount--;
    }

    RETURN(ret);
}

/// @brief Truncate a file.
///
/// Set the size of a file to the new size. If the new size is smaller than the old size, spare bytes are removed. If
/// the new size is larger than the old size, the new bytes may be random.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newSize New size of the file.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseTruncate(const char *path, off_t newSize) {
    LOGM();
    int ret = 0;
    struct fuse_file_info emptyFileInfo = {};
    ret = fuseTruncate(path, newSize, &emptyFileInfo);
    RETURN(ret);
}

/// @brief Truncate a file.
///
/// Set the size of a file to the new size. If the new size is smaller than the old size, spare bytes are removed. If
/// the new size is larger than the old size, the new bytes may be random. This function is called for files that are
/// open.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newSize New size of the file.
/// \param [in] fileInfo Can be ignored in Part 1.
/// \return 0 on success, -ERRNO on failure.

int MyOnDiskFS::fuseTruncate(const char *path, off_t newSize, struct fuse_file_info *fileInfo) {
    LOGM();
    int ret = 0;
    rootFile *file = root->getRootEntryFile(path);
    if (file == nullptr) {
        ret = -ENFILE;
    } else {
        if (newSize >= file->fileStats.st_size) {
            this->setFATBlocks(newSize, 0, file);
        } else {
            int offsetBlock = ceil(newSize / (double) BLOCK_SIZE);
            int currentBlock = file->firstBlock;
            if (newSize == 0) {
                file->firstBlock = FAT_END;
                root->discWrite(file);
            }
            for (int i = 0; currentBlock != FAT_END; i++) {
                int nextBlock = fat->getNext(currentBlock);
                if (i >= offsetBlock) {

                    fat->setNext(currentBlock, FAT_END);
                    dmap->setBlock(currentBlock, false);
                }
                currentBlock = nextBlock;
            }
            file->fileStats.st_size = newSize;
            root->discWrite(file);
        }
    }
    RETURN(ret);
}


/// @brief Read a directory.
///
/// Read the content of the (only) directory.
/// You do not have to check file permissions, but can assume that it is always ok to access the directory.
/// \param [in] path Path of the directory. Should be "/" in our case.
/// \param [out] buf A buffer for storing the directory entries.
/// \param [in] filler A function for putting entries into the buffer.
/// \param [in] offset Can be ignored.
/// \param [in] fileInfo Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                            struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Getting The List of Files of %s\n", path);

    filler(buf, ".", NULL, 0); // Current Directory
    filler(buf, "..", NULL, 0); // Parent Directory
    if (strcmp(path, "/") ==
        0) { // If the user is trying to show the files/directories of the Root directory show the following

        for (int i = 0; i < NUM_DIR_ENTRIES; i++) {
            rootFile *files = root->getFileAtIndex(i);
            if (files != nullptr) {
                filler(buf, files->name, NULL, 0);
            }
        }
        RETURN(0);
    } else {
        RETURN(-ENOTDIR);
    }

}


/// Initialize a file system.
///
/// This function is called when the file system is mounted. You may add some initializing code here.
/// \param [in] conn Can be ignored.
/// \return 0.
void *MyOnDiskFS::fuseInit(struct fuse_conn_info *conn) {
    // Open logfile
    this->logFile = fopen(((MyFsInfo *) fuse_get_context()->private_data)->logFile, "w+");
    if (this->logFile == NULL) {
        fprintf(stderr, "ERROR: Cannot open logfile %s\n", ((MyFsInfo *) fuse_get_context()->private_data)->logFile);
    } else {
        // turn of logfile buffering
        setvbuf(this->logFile, NULL, _IOLBF, 0);

        LOG("Starting logging...\n");
        LOG("Using on-disk mode");
        LOGF("Container file name: %s", ((MyFsInfo *) fuse_get_context()->private_data)->contFile);

        int ret = this->blockDevice->open(((MyFsInfo *) fuse_get_context()->private_data)->contFile);

        if (ret >= 0) {
            LOG("Container file does exist, reading");
            root->initRootDir();
            dmap->init();
            fat->init();


        } else if (ret == -ENOENT) {
            LOG("Container file does not exist, creating a new one");

            ret = this->blockDevice->create(((MyFsInfo *) fuse_get_context()->private_data)->contFile);
            root->init();


        }

        if (ret < 0) {
            LOGF("ERROR: Access to container file failed with error %d", ret);
        }
    }

    return 0;
}

/// @brief Clean up a file system.
///
/// This function is called when the file system is unmounted. You may add some cleanup code here.
void MyOnDiskFS::fuseDestroy() {
    LOGM();
    delete root;
    delete fat;
    delete dmap;

    delete this->blockDevice;

    LOG("--> Delete all Files");
}


///
///
///

int MyOnDiskFS::getIndexOpen() {
    for (int i = 0; i < NUM_OPEN_FILES; i++) {
        if (openFiles[i] == nullptr) {
            RETURN(i);
        }
    }
    RETURN(-ENOENT)
}

// DO NOT EDIT ANYTHING BELOW THIS LINE!!!

/// @brief Set the static instance of the file system.
///
/// Do not edit this method!
void MyOnDiskFS::SetInstance() {
    MyFS::_instance = new MyOnDiskFS();
}
