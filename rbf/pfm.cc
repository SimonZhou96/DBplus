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
    if (file) {
        fclose(file);
        return -1;
    }

    file = fopen(fileName.c_str(), "w");

    if(file == nullptr) return -1;
    fclose(file);
    return 0;
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

    //load the counter information first
    if(fileHandle.loadCounter(file) != 0) return -1;
    // filehandle was doing the operation on file
    if (fileHandle.addFile(file) == 0) return 0;

    return -1;
}

RC PagedFileManager::closeFile(FileHandle &fileHandle) {
    // if the file is currently not created, we return error.
    if(!fileHandle.getFile()) return -1;

    //save the counter information first
    if(fileHandle.saveCounter() != 0) return -1;
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
    if(!FileHandle::getFile()) return -1;
    if(pageNum+1 > FileHandle::numberPageAmount || pageNum < 0) return -1;

    FILE* fptr = FileHandle::getFile();
    fseek(fptr,PAGE_SIZE * (pageNum+1),SEEK_SET);
    if (fread(data, sizeof(byte), PAGE_SIZE, fptr) == PAGE_SIZE) {
        FileHandle::readPageCounter++;
        return 0;
    }
    return -1;
}

RC FileHandle::writePage(PageNum pageNum, const void *data) {
    if(!FileHandle::getFile()) return -1;
    if(pageNum+1 > FileHandle::numberPageAmount || pageNum < 0) return -1;

    FILE* fptr = FileHandle::getFile();
   
    fseek(fptr,PAGE_SIZE * (pageNum+1),SEEK_SET);
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
    fseek(file,0,SEEK_END);
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

RC FileHandle::loadCounter(FILE* file){
    fseek(file,0,SEEK_END);
    int size = ftell(file);
    unsigned* counters = (unsigned*)malloc(sizeof(unsigned)*4);
    if(size == 0){ //New file, create hidden page first
        
        counters[0] = 0;
        counters[1] = 0;
        counters[2] = 0;
        counters[3] = 0;
        void *data = malloc(PAGE_SIZE);
        memset(data,0,PAGE_SIZE);
        memcpy(data,counters,sizeof(unsigned)*4);
        int res = fwrite(data,sizeof(char),PAGE_SIZE,file);
        if(res != PAGE_SIZE) return -1;
        fflush(file);
        free(data);
    }
    fseek(file,0,SEEK_SET);
    fread(counters,sizeof(unsigned),4,file);
    FileHandle::readPageCounter = counters[0];
    FileHandle::writePageCounter = counters[1];
    FileHandle::appendPageCounter = counters[2];
    FileHandle::numberPageAmount = counters[3];
    
    free(counters);
    return 0;
}

RC FileHandle::saveCounter(){
    FILE* file = getFile();
    fseek(file,0,SEEK_SET);
    unsigned* counters = (unsigned*)malloc(sizeof(unsigned)*4);
    counters[0] = FileHandle::readPageCounter;
    counters[1] = FileHandle::writePageCounter;
    counters[2] = FileHandle::appendPageCounter;
    counters[3] = FileHandle::numberPageAmount;
    fwrite(counters,sizeof(unsigned),4,file);
    free(counters);
    return 0; 

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

