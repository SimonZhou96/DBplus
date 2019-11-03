#include "rbfm.h"

using namespace std;

RecordBasedFileManager *RecordBasedFileManager::_rbf_manager = nullptr;

RecordBasedFileManager &RecordBasedFileManager::instance() {
    static RecordBasedFileManager _rbf_manager = RecordBasedFileManager();
    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager() = default;

RecordBasedFileManager::~RecordBasedFileManager() { delete _rbf_manager; }

RecordBasedFileManager::RecordBasedFileManager(const RecordBasedFileManager &) = default;

RecordBasedFileManager &RecordBasedFileManager::operator=(const RecordBasedFileManager &) = default;

RC RecordBasedFileManager::createFile(const std::string &fileName) {
    if (PagedFileManager::instance().createFile(fileName) == 0) return 0;
    return -1;
}

RC RecordBasedFileManager::destroyFile(const std::string &fileName) {
    if (PagedFileManager::instance().destroyFile(fileName) == 0) return 0;
    return -1;
}

RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    if (PagedFileManager::instance().openFile(fileName, fileHandle) == 0) return 0;
    return -1;
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    if (PagedFileManager::instance().closeFile(fileHandle) == 0) return 0;
    return -1;
}
RC RecordBasedFileManager::decodeRecord(void * src, const std::vector<Attribute> &recordDescriptor) {

    auto *dstPtr = (char *) src;
    auto* nullBitIndicator = (unsigned char*) src;
    int pos = 0;

    short recordSize = recordLength(recordDescriptor,src);
    // we first copy the whole record and then we do the encode.
    for (auto attr : recordDescriptor) {
        unsigned char compareBit = *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT)));
        char nullBit = 1;
        switch (attr.type) {
            case TypeInt:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    nullBit = 0;
                    memcpy(dstPtr, &nullBit, sizeof(char));
                } else {
                    nullBit = 1;
                    memcpy(src, &nullBit, sizeof(char));
                }
                break;
            case TypeReal:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    nullBit = 0;
                    memcpy(dstPtr, &nullBit, sizeof(char));
                } else {
                    nullBit = 1;
                    memcpy(dstPtr, &nullBit, sizeof(char));
                }
                break;
            case TypeVarChar:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    nullBit = 0;
                    memcpy(dstPtr, &nullBit, sizeof(char));
                } else {
                    // if the string is null, we set the offset to -1. But there is a unsigned record the length of string, so we should move the offset as well
                    nullBit = 1;
                    memcpy(dstPtr, &nullBit, sizeof(char));
                }
                break;
        }
        dstPtr++;
        pos++;
    }
}
/**
 * For every record, we encode it as from NULL indicator + real Data to Length + realData
 * */
RC RecordBasedFileManager::encodeRecord(void * src, const std::vector<Attribute> &recordDescriptor) {
    // first we get the number of null indicators in a record.
    unsigned nullFieldsIndicatorActualSize = ceil((double) recordDescriptor.size() / CHAR_BIT);

    // encode process begin
    auto *dstPtr = (char *) src;
    auto* nullBitIndicator = (unsigned char*) src;
    char *temp = (char *)src;
    int pos = 0;

    unsigned offset = 0;
    short recordSize = recordLength(recordDescriptor,src);
    // we first copy the whole record and then we do the encode.
    for (auto attr : recordDescriptor) {
        unsigned char compareBit = *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT)));
        switch (attr.type) {
            case TypeInt:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    memcpy(dstPtr, &offset, sizeof(unsigned char));
                    offset += sizeof(unsigned);
                } else {
                    char nullOffset = -1;
                    memcpy(dstPtr, &nullOffset, sizeof(char));
                }
                break;
            case TypeReal:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    memcpy(dstPtr, &offset, sizeof(unsigned char));
                    offset += sizeof(float);
                } else {
                    char nullOffset = -1;
                    memcpy(dstPtr, &nullOffset, sizeof(char));
                }
                break;
            case TypeVarChar:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    auto *curPtr = (char *)src;
                    int nameLength = *(int *)((char *)curPtr + offset);
                    memcpy(dstPtr, &offset, sizeof(unsigned));
                    offset += sizeof(unsigned);
                    offset += nameLength;
                } else {
                    // if the string is null, we set the offset to -1. But there is a unsigned record the length of string, so we should move the offset as well
                    memcpy(dstPtr, &offset, sizeof(unsigned char));
                    offset += sizeof(unsigned);
                }
                break;
        }
        dstPtr++;
        pos++;
    }
    return 0;
}
RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, RID &rid) {
    if(fileHandle.numberPageAmount < 0) return -1;

    if(fileHandle.numberPageAmount == 0){
        initialPage(fileHandle);
    }
    unsigned PID = fileHandle.numberPageAmount-1;
    short slotNum;
    short freeSpace;
    void* buffer = malloc(PAGE_SIZE);
    // get the information the number of the slots, and the free space
    getPageInformation(buffer,fileHandle,PID,slotNum,freeSpace);
    // get the length of the record
    short len = recordLength(recordDescriptor, data) + 2 * sizeof(short);

    encodeRecord(buffer,recordDescriptor);
    // printf("length: %d\n", len);
    if(len > freeSpace){ //over the size of the page, seek to the next one or generate the new one
        if(freeSpace == PAGE_SIZE-2*sizeof(short)) return -1;
        PID = 0;
        getPageInformation(buffer,fileHandle,PID,slotNum,freeSpace);
        while(len > freeSpace) {
            // when we go through all pages and still cannot find the appropriate one, create a new one
            PID++;
            if(PID >= fileHandle.numberPageAmount-1){
                initialPage(fileHandle);
                PID = fileHandle.numberPageAmount-1;
                getPageInformation(buffer,fileHandle,PID,slotNum,freeSpace);
                break;
            }
            getPageInformation(buffer,fileHandle,PID,slotNum,freeSpace);

        }
    }
    //cout << "*************** Before: free space : " << freeSpace << "  *********************** " << endl;
    rid.pageNum = PID;
    //now inserting the new record
    short recordLength;
    short offset;
    short sid;
    recordLength = len - 2*sizeof(short);
    sid = findEmptySlot(buffer,slotNum,offset);
    //cout << "*************** slotNum : " << sid << "  *********************** offset "  << offset<< endl;
    if(sid == slotNum+1){
        slotNum++;
        freeSpace -= 2*sizeof(short);
    }

    freeSpace -= recordLength;
    
    byte* cur = (byte*)buffer;
    cur += (PAGE_SIZE - 2* sizeof(short));
    memcpy(cur, &slotNum, sizeof(short));
    memcpy(cur + sizeof(short), &freeSpace, sizeof(short));
    cur -= sizeof(short) * 2 * sid;

    memcpy(cur, &recordLength, sizeof(short));
    memcpy(cur + sizeof(short), &offset, sizeof(short));

    rid.slotNum = (unsigned)sid;

    cur = (byte*)buffer + offset;
    //cout << "*************** Len : " << recordLength << "  *********************** offset "  << offset<< endl;
    
    memcpy(cur,data,recordLength);
    
    fileHandle.writePage(PID,buffer);
    /*short last_offset = getLastRecordsInfo(buffer);
    slotNum = *(short*)((char*)buffer+PAGE_SIZE-2*sizeof(short));
    short f_value = PAGE_SIZE - sizeof(short)*2- sizeof(short)*2*slotNum - last_offset;
    if(f_value != freeSpace){
        
        cout << "*************** should be : " << f_value << "  *********************** " << endl;
    }*/
    cur = nullptr;

    free(buffer);


    return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data) {
    // we find the page id
    unsigned PID = rid.pageNum;
    unsigned slotNum = rid.slotNum;

    void* buffer = malloc(PAGE_SIZE);
    fileHandle.readPage(PID,buffer);
    short curRecordLength;
    short curRecordOffset;

    // in here, we should decide if that rid is being delete or not
    getSlotInfo(buffer,PID,curRecordLength,curRecordOffset, (short)slotNum);
    
    if (curRecordLength == -1) {
        free(buffer);
        return -1;
    }
    auto *ptr = (char *)buffer;
    // in here, we should decide if that rid is being indirect or not
    if (curRecordLength == PAGE_SIZE + 1) {
        short offset = *(short *)(ptr + PAGE_SIZE - 2 * sizeof(short) - 2 * slotNum * sizeof(short) + sizeof(short));
        unsigned newPageNum = *(unsigned *)(ptr + offset);
        unsigned newSlotNum = *(unsigned *)(ptr + offset + sizeof(unsigned));
//        fileHandle.readPage(newPageNum,buffer);
//        // in here, we should decide if that rid is being delete or not
//        getSlotInfo(buffer, newPageNum,curRecordLength,curRecordOffset,newSlotNum);
        RID currentRID;
        currentRID.pageNum = newPageNum;
        currentRID.slotNum = newSlotNum;

        if (readRecord(fileHandle,recordDescriptor,currentRID,data) == -1){
            free(buffer);
            return -1;
        }
    }
    else{
        auto *cur = (byte*)buffer;
        memcpy(data, cur+curRecordOffset, curRecordLength);
    }
    free(buffer);
    return 0;
}

/**
 * This function is written to delete the record in a single page.
 * The delete function only delete the data content, but not delete the record metadata, so the number of record remains unchanged (Important!)
 * The indirect record [unsigned][unsigned] not short.
 * */
RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const RID &rid) {
//    cout << "delete rid: " << rid.pageNum << " " << rid.slotNum << endl;
    /*
     * 1. read page
     * 2. find where the record is
     * 3. slotNum--, move the delete rid to the deleteQueue
     * 4. shift all the records
     * 5. set the rest of the last record to 0;
     * */
    void* data = malloc(PAGE_SIZE);

    short recordLength;
    short recordOffset;

    unsigned PID = rid.pageNum;
    unsigned slotNum = rid.slotNum;

    fileHandle.readPage(rid.pageNum,data);

    // find where the deleting record is
    auto cur = (char *)data;
    short numberOfRecord;
    short freespace;

    // get the information of which should be deleted, cast to unsigned pointer and then read as unsigned int
    freespace = *(short *)(cur + PAGE_SIZE - sizeof(short));
    numberOfRecord = *(short *)(cur + PAGE_SIZE - 2 * sizeof(short));


    cur = cur + PAGE_SIZE - (2 + 2 * (slotNum)) * sizeof(short);
    recordLength = *(short *) cur;
    recordOffset = *(short *) (cur + sizeof(short));
    //cout << "*************** slotNum : " << rid.slotNum << "***************** len: " << recordLength<< "  *********************** offset "  << recordOffset<< endl;
    cur = (char *) data;
    cur = cur + PAGE_SIZE - 2 * sizeof(short);
    // get the last record's information, to end up as the condition for the shifting loop
    

    // get the boundary of the loop
    short recordBoundary = getLastRecordsInfo(data);
    unsigned ptr1 = recordOffset;
    unsigned ptr2 = recordOffset + recordLength;
    if (recordLength == PAGE_SIZE + 1) {
        auto cur = (char *)data;
        RID newRID;
        newRID.pageNum = *(unsigned *)(cur + recordOffset);
        newRID.slotNum = *(unsigned *)(cur + recordOffset + sizeof(unsigned));
        deleteRecord(fileHandle, recordDescriptor, newRID);
        // we should reset the record length and the movement starting pointer since the special record only has 2*sizeof(short),
        // 4097 is only an indicator
        recordLength = 2 * sizeof(unsigned);
        ptr2 = recordOffset + recordLength;
    }
    cur = (char *) data;

    // shift all the record
    memmove(cur + ptr1, cur + ptr2, recordBoundary - ptr2);
    memset(cur + recordBoundary - recordLength, 0, recordLength);
    // shift the record meta data
    cur += PAGE_SIZE - 2 * sizeof(short) - 2 * slotNum * sizeof(short);

    // from the record after that deleted record
    shiftRecord(data,rid,numberOfRecord,recordLength);

    // change all the variable, and write it to the disk again

    freespace = freespace + recordLength;
    recordLength = -1;
    cur = (char *) data;
    cur = cur + PAGE_SIZE - (2 + 2 * (slotNum)) * sizeof(short);
    memcpy(cur, &recordLength, sizeof(short));
    updatePageInformation(freespace, numberOfRecord, data);



    // store the deleted record
    fileHandle.writePage(PID, data);
    free(data);

    

    return 0;
}


RC RecordBasedFileManager::updatePageInformation(short freespace, short numberOfRecord, void *data){
    // write the new freespace
    auto cur = (char *) data;;
    cur += PAGE_SIZE - sizeof(short);
    memcpy(cur, &freespace, sizeof(short));

    // write the new number of record
    cur -= sizeof(short);
    memcpy(cur, &numberOfRecord, sizeof(short));
    return 0;
}
RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data) {
    // if the data is null, we return -1
    if (!data) return -1;

    char *temp = (char *)data; // set an one bit move pointer

    int nullBitSize = (int)ceil((double)recordDescriptor.size() / CHAR_BIT);
    // the for loop code above is for NULL deciding, this function still cannot handle when NULL comes out.
    temp += nullBitSize;

    auto* nullBitIndicator = (unsigned char*) data;
    int pos = 0;
    // nullBit = nullFieldsIndicator[0] & (unsigned) 1 << (unsigned) 7;
    for (auto attr : recordDescriptor) {
        unsigned char compareBit = *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT)));
        switch (attr.type) {
            case TypeInt:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    cout << attr.name << ": " << *(int *) temp;
                    temp += sizeof(int);
                } else cout << attr.name << ": " << "NULL";
                break;
            case TypeReal:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    cout << attr.name << ": " << *(float *) temp;
                    temp += sizeof(float);
                } else cout << attr.name << ": " << "NULL";
                break;
            case TypeVarChar:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    cout << attr.name << ": ";
                    int nameLength = *(int *) temp;
                    if(nameLength > 999) return -1;
                    temp += sizeof(int);
                    for (int j = 0; j < nameLength; j++) {
                        cout << *temp;
                        temp++;
                    }
                } else cout << attr.name << ": " << "NULL";
                break;
        }
        pos++;
        cout << "\t";
    }
    cout << endl;

    return 0;
}


RC RecordBasedFileManager::updateEqualRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const void *data, void *pageData, const RID &rid) {
    auto *cur = (char *)pageData;
    short curRecordSize = *(short *)(cur + PAGE_SIZE - 2 * sizeof(short) - 2 * rid.slotNum * sizeof(short));
    short curRecordOffset = *(short *)(cur + PAGE_SIZE - 2 * sizeof(short) - 2 * rid.slotNum * sizeof(short) + sizeof(short));
    memcpy(cur + curRecordOffset, data, curRecordSize);
    return 0;
}

RC getRecordInformation(void *pageData, short recordIndex, short &recordLength, short &recordOffset) {
    auto *cur = (char *)pageData;
    recordLength = *(short *)(cur + PAGE_SIZE - 2 * sizeof(short) - 2 * recordIndex * sizeof(short));
    recordOffset = *(short *)(cur + PAGE_SIZE - 2 * sizeof(short) - 2 * recordIndex * sizeof(short) + sizeof(short));
    return 0;
}
// shift record, if update for larger record then offset should be negative, otherwise positive
RC RecordBasedFileManager::shiftRecord(void *pageData, const RID &rid, short numberOfRecord, short offset) {
    auto *cur = (char *)pageData;
    cur += PAGE_SIZE - 2 * sizeof(short) - 2 * rid.slotNum * sizeof(short);

    short startPos = *(short*)(cur+sizeof(short));
    //cout << "Shifting start offset: " << startPos << endl;
    //cout << "Shifting increment: " << offset << endl;
    cur = (char*)((char*)pageData+PAGE_SIZE-2*sizeof(short));
    for (short i = 1;i<=numberOfRecord;i++) {
        cur -= 2 * sizeof(short);
        if(i == (short)rid.slotNum) continue;
        if(*(short*)cur == -1) continue;
        short curOffset = *(short *)(cur + sizeof(short));
        if(curOffset < startPos) continue;
        short newOffset = curOffset - offset;
        //cout << "Shifting old offset: " << curOffset << endl;
        //cout << "Shifting new offset: " << newOffset << endl;
        memcpy(cur + sizeof(short), &newOffset, sizeof(short));
    }
    return 0;
}
RC RecordBasedFileManager::updateLargeInRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const void *data, void * pageData, const RID &rid){
    auto *cur = (char *)pageData;

    short length = recordLength(recordDescriptor, data);
    short curRecordSize;
    short curRecordOffset;
    getRecordInformation(pageData, rid.slotNum, curRecordSize, curRecordOffset);
    short shiftOffset = length - curRecordSize;

    short numberOfRecord;
    short freeSpace;
    getPageInformation(pageData, fileHandle, rid.pageNum, numberOfRecord, freeSpace);
    

    // get the boundary of the loop
    short recordBoundary = getLastRecordsInfo(pageData);

    freeSpace = freeSpace - shiftOffset;

    // shift all the records data segment
    memmove(cur + curRecordOffset + length,
            cur + curRecordSize + curRecordOffset, recordBoundary - curRecordSize - curRecordOffset);

    cur = (char *)pageData;
    // update the length of the metadata of the current updated record
    memcpy(cur + PAGE_SIZE - 2 * sizeof(short) - 2 * rid.slotNum * sizeof(short), &length, sizeof(short));
//    memcpy(cur + PAGE_SIZE - 2 * sizeof(short) - 2 * rid.slotNum * sizeof(short) + sizeof(short), &curRecordOffset, sizeof(short));
    // copy the new larger record
    memcpy(cur + curRecordOffset, data, length);
    // after shift the metadata pages, we need to decrement the freespace  by shiftOffset, and write it back into memory, the number of records doesn't change
    memcpy(cur + PAGE_SIZE - sizeof(short), &freeSpace, sizeof(short));

    shiftRecord(pageData, rid, numberOfRecord, -shiftOffset);
    return 0;
}

/**
 * The data has t
 * **/
RC RecordBasedFileManager::updateSmallInRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                               const void *data, void * pageData,const RID &rid, int ifIndirect) {

    unsigned pageNum = rid.pageNum;
    unsigned slotNum = rid.slotNum;
    short curRecordSize;
    short curRecordOffset;

    short recordBoundary;
    short numberOfRecord;
    short freeSpace;

    getPageInformation(pageData, fileHandle, pageNum, numberOfRecord, freeSpace);
    getRecordInformation(pageData, slotNum, curRecordSize, curRecordOffset);

    // if it is indirect address, we need to allocate 2*sizeof(unsigned)
    short newRecordSize = ifIndirect == 0 ? recordLength(recordDescriptor, data) : 2 * sizeof(unsigned);
    auto cur = (char *)pageData;

    
    recordBoundary = getLastRecordsInfo(pageData);
            // in this case we do not need new page but we need to shift the records
    short offsetNeedsToShift = curRecordSize - newRecordSize;

    /**
     * move the rest of the record to the new position
     * */
    memmove(cur + curRecordOffset + newRecordSize, cur + curRecordOffset + curRecordSize, recordBoundary - curRecordOffset - curRecordSize);
    memset(cur + recordBoundary - offsetNeedsToShift, 0, offsetNeedsToShift);

    freeSpace += offsetNeedsToShift;

    /**
     * copy the indirect address pointer to the memory
     * */
    memcpy(cur+PAGE_SIZE-2*sizeof(short)-2*rid.slotNum*sizeof(short),&newRecordSize,sizeof(short));
    memcpy(cur + curRecordOffset, data, newRecordSize);
    memcpy(cur + PAGE_SIZE - sizeof(short), &freeSpace, sizeof(short));

    // shift the record meta data
    shiftRecord(pageData, rid, numberOfRecord, offsetNeedsToShift);
    return 0;
}

RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, const RID &rid) {

    unsigned pageNum = rid.pageNum;
    unsigned slotNum = rid.slotNum;

    short curFreeSpace;
    short numberOfRecords;

    void * pageData = malloc(PAGE_SIZE);
    fileHandle.readPage(pageNum, pageData);
    getPageInformation(pageData, fileHandle, pageNum, numberOfRecords, curFreeSpace);

    short curRecordSize;
    short curRecordOffset;
    // get the space from the current record
    getRecordInformation(pageData, slotNum, curRecordSize, curRecordOffset);

    /**
     * If the current records length shows 4097, which means we need to find the real data, use recursion here to find the real data segment
     * RECURSIVELY!
     * **/

     if (curRecordSize == PAGE_SIZE + 1) {
         RID directRID;
         auto *cur = (char *)pageData;
         directRID.pageNum = *(unsigned *)(cur + curRecordOffset);
         directRID.slotNum = *(unsigned *)(cur + curRecordOffset + sizeof(unsigned));
         // find the specific record and delete it
         updateRecord(fileHandle, recordDescriptor,data,directRID);
     } else {
         //cout << "current free space is: " << curFreeSpace << " current number of record is: " << numberOfRecords;
         short newRecordSize = recordLength(recordDescriptor, data);
         //cout << "current record size is: " << curRecordSize << " current record offset is : " << curRecordOffset << " the new record size is: " << newRecordSize;

         /**
          * 1. the new record's size is equal to current record size
          * 2. the new record's size is larger than the current size but smaller than the free space
          * 3. the new record size is smaller than the current record size
          * 4. the new record size cannot be stored the current page
          * 5. the new record size is smaller than the pointer size;
          * **/
         if (newRecordSize == curRecordSize) {
             //cout << " in case 1, current page id " << rid.pageNum << " current r id " << rid.slotNum;
             // in this case, we should update the record with the equal size new
             updateEqualRecord(fileHandle, recordDescriptor, data, pageData, rid);
         } else if (newRecordSize > curRecordSize && newRecordSize - curRecordSize <= curFreeSpace) {
             //cout << " in case 2, current page id " << rid.pageNum << " current r id " << rid.slotNum;
             // in this case, we should update the record to a larger one, and shift all the records backforward
             updateLargeInRecord(fileHandle, recordDescriptor, data, pageData, rid);
         } else if (newRecordSize < curRecordSize && newRecordSize >= (short)(2 * sizeof(unsigned))) {
             //cout << " in case 3, current page id " << rid.pageNum << " current r id " << rid.slotNum;
             updateSmallInRecord(fileHandle, recordDescriptor, data, pageData, rid, 0);
         } else if (newRecordSize - curRecordSize > curFreeSpace) {
             //cout << " in case 4, current page id " << rid.pageNum << " current r id " << rid.slotNum;
             RID newRID;
             insertRecord(fileHandle, recordDescriptor, data, newRID);
             //cout << " new page id " << newRID.pageNum << " new r id " << newRID.slotNum;
             void *RIDSpace = malloc(2 * sizeof(unsigned));
             auto *ptr = (char *) RIDSpace;

             memcpy(ptr, &newRID.pageNum, sizeof(unsigned));
             memcpy(ptr + sizeof(unsigned), &newRID.slotNum, sizeof(unsigned));

             short indirectLength = PAGE_SIZE + 1;
             auto *cur = (char *) pageData;

             updateSmallInRecord(fileHandle, recordDescriptor, RIDSpace, pageData, rid, 1);
             memcpy(cur + PAGE_SIZE - 2 * sizeof(short) - 2 * rid.slotNum * sizeof(short), &indirectLength,
                    sizeof(short));
             free(RIDSpace);
         } else if (newRecordSize < (short)(2 * sizeof(unsigned))) {
             //cout << " in case 5, current page id " << rid.pageNum << " current r id " << rid.slotNum;
             void *RIDSpace = malloc(2 * sizeof(unsigned));
             auto *ptr = (char *) RIDSpace;

             memcpy(ptr, data, newRecordSize);
             updateSmallInRecord(fileHandle, recordDescriptor, RIDSpace, pageData, rid, 0);
         }
         fileHandle.writePage(rid.pageNum, pageData);
     }
     //cout << endl;
    free(pageData);
    return 0;
}

RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                         const RID &rid, const std::string &attributeName, void *data) {
    void* buffer = malloc(PAGE_SIZE);
    readRecord(fileHandle, recordDescriptor,rid,buffer);

    decodeRecord(buffer, recordDescriptor);
    short len = 0;

    int nullBitSize = (int)ceil((double)recordDescriptor.size() / CHAR_BIT);

    len += (short)nullBitSize;
    auto * cur = (char *)buffer;

    cur += nullBitSize;

    unsigned char* nullBitIndicator;
    nullBitIndicator = (unsigned char *) buffer;
    memcpy(data,nullBitIndicator,nullBitSize);
    auto* dataCur = ((char *)data + nullBitSize);


    int pos = 0;

    for(unsigned i = 0;i<recordDescriptor.size();i++){
        unsigned char compareBit = *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT)));
        if(recordDescriptor[i].name.compare(attributeName) == 0){
            if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))){
                switch (recordDescriptor[i].type){
                    case TypeReal:
                        memcpy(dataCur,cur,sizeof(float));
                        break;
                    case TypeInt:
                        memcpy(dataCur,cur,sizeof(int));
                        break;
                    case TypeVarChar:
                        int nameLength = *(int *) cur;
                        memcpy(dataCur,cur,sizeof(int));
                        memcpy(dataCur+sizeof(int),cur+sizeof(int),nameLength);
                        break;
                }
                free(buffer);
                return 0;
            }
            else{
                free(buffer);
                return 1; //the value is null, return
            }
        }
        switch (recordDescriptor[i].type){
            case TypeReal:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    len += sizeof(float);
                    cur += sizeof(float);
                }
                break;
            case TypeInt:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    len += sizeof(int);
                    cur += sizeof(int);
                }
                break;
            case TypeVarChar:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    int nameLength = *(int *) cur;
                    len += sizeof(int);
                    cur += sizeof(int);
                    cur += nameLength;
                    len += nameLength;
                }
                break;
        }
    }
    free(buffer);
    return -1;
    

}

RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                const std::vector<std::string> &attributeNames, RBFM_ScanIterator &rbfm_ScanIterator) {
    return rbfm_ScanIterator.initialize(fileHandle,recordDescriptor,conditionAttribute,compOp,value,attributeNames);
}

RC RBFM_ScanIterator::initialize(FileHandle &fileHandle,const std::vector<Attribute> &recordDescriptor,
                                const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                const std::vector<std::string> &attributeNames){
    RBFM_ScanIterator::current_rid.pageNum = -1;
    RBFM_ScanIterator::current_rid.slotNum = -1;
    RBFM_ScanIterator::fileHandle = fileHandle;
    RBFM_ScanIterator::totalPage = fileHandle.numberPageAmount;
    RBFM_ScanIterator::recordDescriptor = recordDescriptor;
    RBFM_ScanIterator::attributeNames = attributeNames;
    RBFM_ScanIterator::conditionAttribute = conditionAttribute;
    RBFM_ScanIterator::compOp = compOp;
    RBFM_ScanIterator::value = (byte*)value;
    RBFM_ScanIterator::buffer = malloc(PAGE_SIZE);
    RBFM_ScanIterator::totalRecord = 0;
    RBFM_ScanIterator::rbfm = &RecordBasedFileManager::instance();
    conditionPos = -1;
    for(int i = 0;i<(short)recordDescriptor.size();i++){
        if(recordDescriptor[i].name.compare(conditionAttribute) == 0 ) conditionPos = i;
    }
    AttriPos.clear();
    for(auto att: attributeNames){
        for(unsigned i = 0;i<recordDescriptor.size();i++){
            if(recordDescriptor[i].name.compare(att) == 0){
                AttriPos.push_back(recordDescriptor[i].type);
                break;
            }
        }
    }
    return 0;
}

RC RBFM_ScanIterator::getNextRecord(RID &rid, void *data){
    //Check whether we need to go to the next page or not
    while((short)current_rid.slotNum == -1 || (short)current_rid.slotNum >= totalRecord){
        current_rid.pageNum++;
        //cout << "pageNum:" << current_rid.pageNum << "total Page:" << totalPage << endl;
        if(current_rid.pageNum >= totalPage) return RBFM_EOF; //no more record, end.
        short freeSpace = 0;
        rbfm->getPageInformation(buffer,fileHandle,current_rid.pageNum,totalRecord,freeSpace);
        if(totalRecord == 0){
            continue;
        }
        current_rid.slotNum = 0;
    }
    current_rid.slotNum++;
    short offset = 0;
    short length = 0;
    while((short)current_rid.slotNum <= totalRecord){
        rbfm->getSlotInfo(buffer,current_rid.pageNum, length, offset, current_rid.slotNum);
        if(length == -1 || length == PAGE_SIZE + 1){ //special case, ignore them
            current_rid.slotNum++;
            if((short)current_rid.slotNum > totalRecord) return getNextRecord(rid,data);
            continue;
        }
        void* returnAttribute = malloc(PAGE_SIZE);
        
        if(compOp == NO_OP){
            formatData(data);
            rid.pageNum = current_rid.pageNum;
            rid.slotNum = current_rid.slotNum;
            free(returnAttribute);
            return 0;

        }
        RC res = rbfm->readAttribute(fileHandle,recordDescriptor,current_rid,conditionAttribute,returnAttribute);
        if(res == 1){
            free(returnAttribute);
            current_rid.slotNum++;
            if((short)current_rid.slotNum > totalRecord) return getNextRecord(rid,data);
            continue;
        }
        auto* r_value = (byte*)returnAttribute;
        int nullBitSize = (int)ceil((double)attributeNames.size() / CHAR_BIT);
        r_value+=nullBitSize;
        switch (recordDescriptor[conditionPos].type){
            case TypeReal:
                switch(compOp){
                    case(EQ_OP):
                        if(*(float*)r_value == *(float*)value){
                            formatData(data);
                            rid = current_rid;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(LT_OP):
                        if(*(float*)r_value < *(float*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(LE_OP):
                        if(*(float*)r_value <= *(float*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(GT_OP):
                        if(*(float*)r_value > *(float*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(GE_OP):
                        if(*(float*)r_value >= *(float*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(NE_OP):
                        if(*(float*)r_value != *(float*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(NO_OP):
                        formatData(data);
                        rid.pageNum = current_rid.pageNum;
                        rid.slotNum = current_rid.slotNum;
                        free(returnAttribute);
                        return 0;
                        break;
                }
                break;
            case TypeInt:
                switch(compOp){
                    case(EQ_OP):
                        if(*(int*)r_value == *(int*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(LT_OP):
                        if(*(int*)r_value < *(int*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(LE_OP):
                        if(*(int*)r_value <= *(int*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(GT_OP):
                        if(*(int*)r_value > *(int*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(GE_OP):
                        if(*(int*)r_value >= *(int*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(NE_OP):
                        if(*(int*)r_value != *(int*)value){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            return 0;
                        }
                        break;
                    case(NO_OP):
                        formatData(data);
                        rid.pageNum = current_rid.pageNum;
                        rid.slotNum = current_rid.slotNum;
                        free(returnAttribute);
                        return 0;
                        break;
                }
                break;
                case TypeVarChar:
                    
                    if(compOp == EQ_OP){
                        int s1_len = *(int*)r_value;
                        char* s2 = (char*)value;
                        char* temp_char = (char*)malloc(sizeof(char) * (s1_len+1));
                        auto* cur = (byte*)r_value;
                        cur += sizeof(int);
                        memcpy(temp_char, r_value+sizeof(int),s1_len);
                        temp_char[s1_len] = '\0';
                        if(strcmp(temp_char,s2) == 0){
                            formatData(data);
                            rid.pageNum = current_rid.pageNum;
                            rid.slotNum = current_rid.slotNum;
                            free(returnAttribute);
                            free(temp_char);
                            return 0;
                        }
                        free(temp_char);
                        break;

                    }
                    break;
        }
        current_rid.slotNum++;
        free(returnAttribute);
        if((short)current_rid.slotNum > totalRecord) return getNextRecord(rid,data);

    }
    return -1;

}

/*format:
1. Null nullBitIndicator
2. data each*/
RC RBFM_ScanIterator::formatData(void *data){
    int nullBitSize = (int)ceil((double)attributeNames.size() / CHAR_BIT);
    auto* cur = (byte*)data;
    cur += nullBitSize;
    auto* nullBitIndicator = (byte*)data;
    int pos = 0;
    for(auto s : attributeNames){
        void* returnAttribute = malloc(PAGE_SIZE);
        RC res = rbfm->readAttribute(fileHandle,recordDescriptor,current_rid,s,returnAttribute);
        memset(nullBitIndicator,0,nullBitSize);
        unsigned char compareBit = *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT)));
        if(res == 1){
            compareBit |= ((unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT));
            *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT))) = compareBit;
            free(returnAttribute);
            pos++;
            continue;
        }
        auto* r_value = (byte*)returnAttribute;
        r_value += (int)ceil((double)recordDescriptor.size() / CHAR_BIT);
        switch (AttriPos[pos]){
            case TypeInt:
                memcpy(cur,r_value,sizeof(int));
                cur += sizeof(int);
                break;
            case TypeReal:
                memcpy(cur,r_value,sizeof(float));
                cur += sizeof(float);
                break;
            case TypeVarChar:
                int len = *(int*)r_value;
                memcpy(cur,r_value,sizeof(int));
                cur += sizeof(int);
                memcpy(cur,(r_value+sizeof(int)),len);
                cur += len;
                break;
        }
        free(returnAttribute);
        pos++;
    }
    return 0;
    
}

RC RBFM_ScanIterator::close(){
    free(buffer);
    current_rid.pageNum = -1;
    current_rid.slotNum = -1;
    totalRecord = 0;
    rbfm->closeFile(fileHandle);
    return 0;
}

RC RecordBasedFileManager::getPageInformation(void* data, FileHandle &fileHandle, unsigned PID, short &numberRecord, short &freeSpace){
    if(fileHandle.readPage(PID,data) != 0) return -1;
    byte* cur = (byte*)data;
    cur += (PAGE_SIZE - sizeof(short) * 2);
    numberRecord = *(short*) cur;
    freeSpace = *(short*) (cur+sizeof(short));

    return 0;
}


RC RecordBasedFileManager::getSlotInfo(void* data,unsigned PID, short &length, short &offset, short numberRecord){
    auto* cur = (char*)data;
    cur += (PAGE_SIZE - sizeof(short) * 2);
    cur -= (sizeof(short) * 2 * numberRecord);
    length = *(short*) cur;
    offset = *(short*) (cur+sizeof(short));
    return 0;
}

RC RecordBasedFileManager::initialPage(FileHandle &fileHandle){
    void* data = malloc(PAGE_SIZE);
    memset(data, 0, PAGE_SIZE);
    short numberRecord = 0;
    short freeSpace = PAGE_SIZE - 2*sizeof(short);
    // intialize the number of record into the new page
    memcpy((char *)data + (PAGE_SIZE-2*sizeof(short)),&numberRecord,sizeof(short));
    // intialize the number of free space into the new page
    memcpy((char *)data + (PAGE_SIZE-sizeof(short)),&freeSpace,sizeof(short));
    RC result = fileHandle.appendPage(data);
    free(data);
    return result;
}

short RecordBasedFileManager::getLastRecordsInfo(const void* page){
    auto* cur = (byte*) page;
    cur += (PAGE_SIZE - 2*sizeof(short));
    short numberOfRecords = *(short*)cur;
    short freespace = *(short*)(cur+sizeof(short));
    short offset = 0;

    if(numberOfRecords == 0){
        return 0;
    }
    offset = PAGE_SIZE - (freespace + 2 * sizeof(short) * (1+numberOfRecords));
    return offset;
    /*for(short i = 1;i<=numberOfRecords;i++){

        cur -= 2*sizeof(short);
        if(*(short*)cur == -1) continue;
        if(*(short*)cur == PAGE_SIZE+1){
            short newOffset = (*(short*)(cur+sizeof(short)));
            if(offset <= newOffset){
                offset = newOffset;
                length = sizeof(unsigned)*2;
            }
        }
        else{
            short newOffset = (*(short*)(cur+sizeof(short)));
            if(offset <= newOffset) {
                offset = newOffset;
                length = *(short*)cur;
            }
        }

    }*/

}

short RecordBasedFileManager::findEmptySlot(const void* data, const short numberRecord, short &offset){
    if(numberRecord == 0){
        offset = 0;
        return 1;
    }
    byte* cur = (byte*)data;
    cur += (PAGE_SIZE - sizeof(short)*2);
    short sid = -1;
    offset = 0;
    short freespace = *(short*)(cur+sizeof(short));

    for(short i = 1;i<=numberRecord;i++){
        cur -= 2*sizeof(short);
        if(*(short*)cur == -1){
            if(sid == -1)sid = i;
            break;
        }
        /*short temp = *(short*)(cur+sizeof(short));
        if(*(short*)cur == PAGE_SIZE+1){
            temp += 2*sizeof(unsigned);
        }
        else temp += *(short*)cur;
        if(offset < temp )offset = temp;*/
    }
    offset = PAGE_SIZE - (freespace + 2 * sizeof(short) * (1+numberRecord));
    if(sid != -1) return sid;
    else return numberRecord+1;
}

short RecordBasedFileManager::recordLength(const std::vector<Attribute> &recordDescriptor, const void* data){

    int len = 0;
//    len += sizeof(short) * 2; // space for recording len and offset.

    int nullBitSize = (int)ceil((double)recordDescriptor.size() / CHAR_BIT);

    len += (short)nullBitSize;
    auto * cur = (char *)data;

    cur += nullBitSize;

    unsigned char* nullBitIndicator;
    nullBitIndicator = (unsigned char *) data;

    int pos = 0;
    for (auto arr: recordDescriptor) {
        unsigned char compareBit = *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT)));
        switch (arr.type){
            case TypeReal:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    len += sizeof(float);
                    cur += sizeof(float);
                }
                break;
            case TypeInt:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    len += sizeof(int);
                    cur += sizeof(int);
                }
                break;
            case TypeVarChar:
                if (!(compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT))) {
                    int nameLength = *(int *) cur;
                    len += sizeof(int);
                    cur += sizeof(int);
                    cur += nameLength;
                    len += nameLength;
                }
                break;
        }
        pos++;
    }
    //cout << "Size calculated:" << len << endl;
    return (short)len;
}

