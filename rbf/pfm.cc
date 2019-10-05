#include "pfm.h"

using namespace std;
PagedFileManager *PagedFileManager::_pf_manager = nullptr;

PagedFileManager &PagedFileManager::instance() {
    static PagedFileManager _pf_manager = PagedFileManager();
    return _pf_manager;
}

PagedFileManager::PagedFileManager() = default;

PagedFileManager::~PagedFileManager() { delete _pf_manager; }

PagedFileManager::PagedFileManager(const PagedFileManager &) = default;

PagedFileManager &PagedFileManager::operator=(const PagedFileManager &) = default;


RC PagedFileManager::createFile(const std::string &fileName) {
    // we first open the file with read mode to see if there exists a file.
    FILE* file = fopen(fileName.c_str(), "r");
    // if file exists, we return error
    if (file) return -1;

    file = fopen(fileName.c_str(), "w");
    return file == nullptr ? -1 : 0;

}

RC PagedFileManager::destroyFile(const std::string &fileName) {
    PagedFileManager::globalfile = fopen(fileName.c_str(), "r");
    if (!PagedFileManager::globalfile) return -1;
    if (fclose(PagedFileManager::globalfile) != 0) return -1;
    if (remove(fileName.c_str()) != 0) return -1;
    return 0;
}

RC PagedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    FILE* file = fopen(fileName.c_str(), "rb+s");

    if (!file) return -1;

    // filehandle was doing the operation on file
    if (fileHandle.addFile(file) == 0) return 0;

    return -1;
}

RC PagedFileManager::closeFile(FileHandle &fileHandle) {
    // if the file is currently not created, we return error.
    if(!fileHandle.getFile()) return -1;
    // other wise we close the file handle
    if (fflush(fileHandle.getFile()) != 0) return -1;
    if (fclose(fileHandle.getFile()) != 0) return -1;
    return 0;;
}

FileHandle::FileHandle() {

    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
    numberPageAmount = 0;
}

FileHandle::~FileHandle() = default;

RC FileHandle::addFile(FILE* file) {
    if (FileHandle::getFile() != nullptr && FileHandle::getFile() != file)
        return -1;
    this->file = file;
    return 0;
}

RC FileHandle::readPage(PageNum pageNum, void *data) {
    if (!FileHandle::getFile()) return -1;
    if (pageNum > FileHandle::numberPageAmount || pageNum < 0) return -1;


    fseek(getFile(), (pageNum) * PAGE_SIZE, SEEK_SET);
    if (fread(data, 1, PAGE_SIZE,getFile()) == PAGE_SIZE) {
        // read the page
        FileHandle::readPageCounter++;
        return 0;
    }
    return -1;
}

RC FileHandle::writePage(PageNum pageNum, const void *data) {
    if(!FileHandle::getFile()) return -1;
    if(pageNum > FileHandle::numberPageAmount || pageNum < 0) return -1;
    
    FILE* fptr = FileHandle::getFile();
   
    fseek(fptr,PAGE_SIZE * (pageNum),SEEK_SET);
    if (fwrite(data, sizeof(char), PAGE_SIZE, fptr) == PAGE_SIZE) {
        fflush(fptr);
        //increase the counter
        FileHandle::writePageCounter++;
        return 0;
    }
    return -1;

}

RC FileHandle::appendPage(const void *data) {
    FILE* file = getFile();
    // allocate memory to data, still need to work on that part!!!!
    if (!data) return -1;
    int res = fwrite(data, sizeof(char), PAGE_SIZE, file);
    // write the data into the
    if ( res == PAGE_SIZE) {
        // i think we should move the pointer
        FileHandle::appendPageCounter++;
        FileHandle::numberPageAmount++;
    }
    // after writting we flush the file*
    fflush(getFile());
    return 0;
}

FILE* FileHandle::getFile() {
    return this->file;
}

unsigned FileHandle::getNumberOfPages() {
    return numberPageAmount >= 0 ? numberPageAmount : -1;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {

    // any exceptions here?
    readPageCount = FileHandle::readPageCounter;
    writePageCount = FileHandle::writePageCounter;
    appendPageCount = FileHandle::appendPageCounter;

    return 0;
}
