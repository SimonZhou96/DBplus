#include "ix.h"

IndexManager &IndexManager::instance() {
    static IndexManager _index_manager = IndexManager();
    return _index_manager;
}

RC IndexManager::createFile(const std::string &fileName) {
    /**
     * read append counter, write counter, read counter into the memory
     * **/

    return _rbfm->createFile(fileName.c_str());
}

RC IndexManager::destroyFile(const std::string &fileName) {

    return _rbfm->destroyFile(fileName);
}

RC IndexManager::openFile(const std::string &fileName, IXFileHandle &ixFileHandle) {
    RC rc = _rbfm->openFile(fileName.c_str(), ixFileHandle.fileHandler);
    if(rc != 0) return rc;
    unsigned readPageCount = 0;
    unsigned writePageCount = 0;
    unsigned appendPageCount = 0;
    ixFileHandle.collectCounterValues(readPageCount, writePageCount, appendPageCount);
    return rc;
}

RC IndexManager::closeFile(IXFileHandle &ixFileHandle) {
    /**
     * write append counter, write counter, read counter into the memory
     * **/
    return _rbfm->closeFile(ixFileHandle.fileHandler);
}


RC IndexManager::initialDummyPage(IXFileHandle &ixFileHandle, const Attribute &attribute) {
    void *data = malloc(PAGE_SIZE);
    memset(data,0,PAGE_SIZE);
    ixFileHandle.appendIxPage(data);
    //ixFileHandle.ixAppendPageCounter--;
    // hide the dummy page from the user
    free(data);
    return 0;
}
RC IndexManager::alterRootPointer(IXFileHandle &ixFileHandle, const Attribute &attribute, const unsigned newPageID) {
    void* data = malloc(PAGE_SIZE);
    memset(data,0,PAGE_SIZE);
    ixFileHandle.readIxPage(DUMMY_ID,data);

    memcpy(data, &newPageID, sizeof(unsigned));
    ixFileHandle.writeIxPage(DUMMY_ID, data);
    free(data);
    return 0;
}
/**
 * Get the current page number that the dummy pointer points to
 * **/
RC IndexManager::getRootPageID(IXFileHandle &ixFileHandle, unsigned &rootPageID) {
    void *data = malloc(PAGE_SIZE);
    ixFileHandle.readIxPage(0, data);
    rootPageID = *(unsigned *)((char *)data);
    free(data);
    return 0;
}

/**
 * Format of Index Page
 * The first bit is the page type(non-leaf or leaf)
 * The second unsigned indicates the current free space
 * The third unsigned indicates the number of keys
 * Then we start with the data entries
 * **/
RC IndexManager::initialIndexEntriesPage(IXFileHandle &ixFileHandle, void *indexPage, PageNum &pageID) {
    pType pageType = INDEXENTRIES;
    unsigned numberOfRecord = 0;
    unsigned freeSpace = PAGE_SIZE - sizeof(pType) - RID_SIZE;

    auto* ptr = (char *)indexPage + PAGE_SIZE - sizeof(pType);
    memcpy(ptr, &pageType, sizeof(pType));

    ptr -= sizeof(unsigned);
    memcpy(ptr, &freeSpace, sizeof(unsigned));

    ptr -= sizeof(unsigned);
    memcpy(ptr, &numberOfRecord, sizeof(unsigned));

    ixFileHandle.appendIxPage(indexPage);

    pageID = ixFileHandle.fileHandler.getNumberOfPages()-1;
    return 0;
}

/**
 * Format of Data Page
 * The first unsigned char is the page type(non-leaf or leaf)
 * The second INT is the INT pointer which points to another node
 * The third unsigned indicates the current free space
 * The fourth unsigned indicates the current number of keys
 * Then we start with the data entries
 * **/

RC IndexManager::getDataEntriesPageInformation(void *dataPage, unsigned char &pageType, PageNum &nextPagePointer,
                                               unsigned &freeSpace, unsigned &numberOfKeys) {
    auto *ptr = (char *)dataPage + PAGE_SIZE - sizeof(pType);
    memcpy(&pageType, ptr, sizeof(pType));

    ptr -= sizeof(PageNum);
    memcpy(&nextPagePointer, ptr, sizeof(PageNum));

    ptr -= sizeof(unsigned);
    memcpy(&freeSpace, ptr, sizeof(unsigned));

    ptr -= sizeof(unsigned);
    memcpy(&numberOfKeys, ptr, sizeof(unsigned));


    return 0;
}

/**
 * return the new page id that will be allocated with that page
 * **/
RC IndexManager::initialDataEntriesPage(IXFileHandle &ixFileHandle, void *dataPage, PageNum &pageID) {
    pType pageType = DATAENTRIES;
    PageNum pointer = 0;
    unsigned numberOfRecord = 0;
    unsigned freeSpace = PAGE_SIZE - sizeof(PageNum) - sizeof(pType) - RID_SIZE;
    auto *ptr = (char *)(dataPage);

    ptr += PAGE_SIZE;

    ptr -= sizeof(pType);
    memcpy(ptr, &pageType, sizeof(pType));

    ptr -= sizeof(PageNum);
    memcpy(ptr, &pointer, sizeof(PageNum));

    ptr -= sizeof(unsigned);
    memcpy(ptr, &freeSpace, sizeof(unsigned));

    ptr -= sizeof(unsigned);
    memcpy(ptr, &numberOfRecord, sizeof(unsigned));

    ixFileHandle.appendIxPage(dataPage);

    pageID = ixFileHandle.fileHandler.getNumberOfPages()-1;
    
    return 0;
}

/**
 * The format of key that is ready to insert is
 * LeftPagePtr + Key + Rid + RightPagePtr
 * **/

RC IndexManager::getKeySize(const Attribute &attribute, const void *key) {
    unsigned keySize = 0;
    switch (attribute.type) {
        case TypeInt:
            keySize += sizeof(int);
            break;
        case TypeReal:
            keySize += sizeof(float);
            break;
        case TypeVarChar:
            int varcharLength = *(int *)((byte *)key);
            keySize += sizeof(int) + varcharLength;
            break;
    }

    return keySize;
}

/**
 * return the pointer
 * IMPORTANT:
 * This can be only used in index node search!!!!!
 * **/
void* IndexManager::indexBinarySearch(void* page, const Attribute &attribute, const void *key, const RID &rid, int &index){

    byte* cur = (byte*)page;
    int startOffset = 0;
    unsigned numberOfRecords = *(unsigned*)(cur + PAGE_SIZE - sizeof(pType) - RID_SIZE);

    int endOffset = numberOfRecords - 1;
    PageNum leftPageID;
    char *temp = nullptr;
    if(key == nullptr){ //We want to start at the left most result
        temp = (char *)moveCursorToFirstIndexMetaEntires(page);
        index = 0;
        byte* tempKey = (byte*)getIndexEntryKey(attribute, page, temp, leftPageID);

        return tempKey;
    }



    //Corner Case: the key locate the right pointer of the right most key:
    temp = (char *)moveCursorToFirstIndexMetaEntires(page);
    temp -= endOffset * RID_SIZE;


    void *lastOffsetIndexEntries = getIndexEntryKey(attribute,page,temp,leftPageID);


    if (keyCompare(attribute, key, rid, lastOffsetIndexEntries) > 0){

        byte* tempKey = (byte*)getIndexEntryKey(attribute, page, temp,leftPageID);
        index = endOffset;
        return tempKey;
    }



    // find the last element that is smaller or equal to the key
    while (startOffset < endOffset - 1) {

        char *mid = nullptr;
        unsigned midIndex = (startOffset + (endOffset - startOffset) / 2);

        mid = (char *)moveCursorToFirstIndexMetaEntires(page);
        mid -=  midIndex * RID_SIZE;

        void* midKey = getIndexEntryKey(attribute, page, mid, leftPageID);
        if (keyCompare(attribute, key, rid, midKey) > 0) startOffset = midIndex;
        else if (keyCompare(attribute, key, rid, midKey) == 0){
            index = midIndex -1;
            if(index == -1)return midKey;
            else{
                mid = (char *)moveCursorToFirstIndexMetaEntires(page);
                mid -=  (index) * RID_SIZE;
                midKey = getIndexEntryKey(attribute, page, mid, leftPageID);
                return midKey;
            }
        }
        else endOffset = midIndex;
    }
    /**
     * we need to decide whether the key is larger than all of the index entries or not.
     * In other words, we are validating whether the index entries the to which the endOffset points is really bigger than key
     * **/
    temp = nullptr;

    temp = (char *)moveCursorToFirstIndexMetaEntires(page);
    temp -= startOffset * RID_SIZE;

    void *firstOffsetIndexEntries = getIndexEntryKey(attribute,page,temp, leftPageID);

    if (startOffset == 0 && keyCompare(attribute, key, rid, firstOffsetIndexEntries) <= 0){
        index = startOffset - 1;
    }
        // if the larget key is sill smaller than the key we want, then goes to the last pointer
    else index = startOffset;


    return firstOffsetIndexEntries;

}

/**
 * 1. find the type of the key that we need to insert, get the key, rid, left pointer and right pointer from the const void *key
 * 2. binary search to find the last element that is smaller or equal to key.
 * 3. if we cannot find such an element, which means that the key is smaller than all of the key in the index node
 * 3.1 then we change the left pointer of the first node to overflow pageid,
 * 3.2 insert the leftpointer + key + rid into the first place
 * 3.3 update the meta data
 * 3.4 insert new meta data into the meta data area
 * 4. else we find insert the key next to such element, repeat 3.1-3.4
 * **/
RC IndexManager::insertIndexEntriesKey(void *page, const Attribute &attribute, const void *key) {


//    unsigned keyL = *(unsigned *) ((char *)key + sizeof(unsigned));
//    cout << "---";
//    for (int i = 0; i < keyL; i++) cout << *((char *) key + 2 * sizeof(unsigned) + i);
//    cout << "------";
//    cout << "before insertion " << endl;
//    unsigned tnumberOfKeys = *(unsigned *) ((char *) page + PAGE_SIZE - TYPE_PTR - 2 * sizeof(unsigned));
//    char *testMDP = ((char *) page + PAGE_SIZE - TYPE_PTR - 2 * sizeof(unsigned) - RID_SIZE);
//    int count = 0;
//    while (count < tnumberOfKeys) {
//        unsigned offset = *(unsigned *) (testMDP + sizeof(unsigned));
//        unsigned len = *(unsigned *) (testMDP);
//        auto *RDP = (char *) page + offset;
//        unsigned length = *(unsigned *) RDP;
//        cout << "(";
//        for (int i = 0; i < length; i++) {
//            cout << *(RDP + sizeof(unsigned) + i);
//        }
//        cout << "[" << offset << "," << len << "]";
//        cout << ")";
//        count++;
//        testMDP -= RID_SIZE;
//    }
//    cout << endl;


    pType pageType;
    unsigned freeSpace;
    unsigned numberOfKeys;
    getIndexEntriesPageInformation(page,pageType,freeSpace,numberOfKeys);

    auto *parseKeyPtr = (char *)key;
    PageNum leftPagePtr = *(PageNum *)parseKeyPtr;
    PageNum rightPagePtr;
    unsigned keyridSize;
    RID rid;
    // skip the left pointer
    parseKeyPtr += sizeof(PageNum);

    void *insertComparator = parseKeyPtr;
    if (attribute.type == TypeInt) {
        // INT + RID
        keyridSize = sizeof(int) + RID_SIZE;
        // insert comparator is just key, without rid
        rightPagePtr = *(PageNum *)(parseKeyPtr + keyridSize);
        rid.pageNum = *(unsigned *)(parseKeyPtr + keyridSize - 2 * sizeof(unsigned));
        rid.slotNum = *(unsigned *)(parseKeyPtr + keyridSize - sizeof(unsigned));

    } else if (attribute.type == TypeReal) {
        keyridSize = sizeof(float) + RID_SIZE;

        rightPagePtr = *(PageNum *)(parseKeyPtr + keyridSize);
        rid.pageNum = *(unsigned *)(parseKeyPtr + keyridSize - 2 * sizeof(unsigned));
        rid.slotNum = *(unsigned *)(parseKeyPtr + keyridSize - sizeof(unsigned));
    } else {
        int varcharLength = *(int *)parseKeyPtr;
        keyridSize = sizeof(int) + varcharLength + RID_SIZE;

        rightPagePtr = *(PageNum *)(parseKeyPtr + keyridSize);
        rid.pageNum = *(unsigned *)(parseKeyPtr + keyridSize - 2 * sizeof(unsigned));
        rid.slotNum = *(unsigned *)(parseKeyPtr + keyridSize - sizeof(unsigned));
    }
    /**
     * If number of keys is equal to 0, we just simply insert index entry and change numberof Keys and free space
     * **/

    if (numberOfKeys == 0) {
        auto *ptr = (char *)page;
        unsigned insertKeySize = keyridSize + PTR_SIZE * 2;
        memcpy(ptr, key, insertKeySize);

        unsigned insertPureKeyOffset = sizeof(unsigned); // skip the first pointer, and then is the key offset

        /** insert the meta data
         * **/
        memcpy(ptr + PAGE_SIZE - sizeof(pType) - 2 * sizeof(unsigned) - 2 * sizeof(unsigned), &keyridSize,
               sizeof(unsigned));
        memcpy(ptr + PAGE_SIZE - sizeof(pType) - 2 * sizeof(unsigned) - sizeof(unsigned), &insertPureKeyOffset,
               sizeof(unsigned));

        numberOfKeys++;
        freeSpace -= insertKeySize;
        freeSpace -= RID_SIZE;
        memcpy(ptr + PAGE_SIZE  - sizeof(pType) - sizeof(unsigned), &freeSpace, sizeof(unsigned));
        memcpy(ptr + PAGE_SIZE  - sizeof(pType) - 2 * sizeof(unsigned), &numberOfKeys, sizeof(unsigned));

        return 0;
    }
    /**
     * We need to use binary search to get the last element that is smaller than the insert value
     * code waiting for pulling
     * **/
    int searchOffset;
    auto *targetRealIndexPtr = indexBinarySearch(page, attribute, parseKeyPtr, rid,searchOffset);


    /**
     * Decide whether the return data is larger than the insert key
     * **/
    auto *targetIndexEntryPtr = (char *)page + PAGE_SIZE - TYPE_PTR - 2 * sizeof(unsigned) - RID_SIZE;
    if (searchOffset != -1) targetIndexEntryPtr -= 2 * searchOffset * sizeof(unsigned);


    unsigned targetIndexEntryLength = *(unsigned *)(targetIndexEntryPtr);
    unsigned targetIndexEntryOffset = *(unsigned *)(targetIndexEntryPtr + sizeof(unsigned));

    if (keyCompare(attribute,insertComparator,rid, targetRealIndexPtr) <= 0) {
        /**
         * Change the page pointer
         * **/
        unsigned leftMostPointer = *(unsigned *)((char *)targetRealIndexPtr - PTR_SIZE);
        leftMostPointer = rightPagePtr;
        memcpy((char *)targetRealIndexPtr - PTR_SIZE, &leftMostPointer, PTR_SIZE);

        auto *ptr = (char *)page;
        ptr += PAGE_SIZE - sizeof(pType) - RID_SIZE - 2 * numberOfKeys * sizeof(unsigned);
        unsigned lastKeyOffset = *(unsigned *)(ptr + sizeof(unsigned));
        unsigned lastKeyLength = *(unsigned *)(ptr);

        /**
         * To calculate the key boundary, we need to count into the right most page pointer;
         * **/
        unsigned keyBoundary = lastKeyOffset + lastKeyLength + PTR_SIZE;
        ptr = (char *)page;
        memmove(ptr + targetIndexEntryOffset - PTR_SIZE + keyridSize + PTR_SIZE,
                ptr + targetIndexEntryOffset - PTR_SIZE, keyBoundary - (targetIndexEntryOffset - PTR_SIZE ));

        // insert the new leftmost pointer and the key + rid segment into the first slot
        memcpy(ptr + targetIndexEntryOffset - PTR_SIZE, &leftPagePtr, PTR_SIZE);

        parseKeyPtr = (char *)key;
        memcpy(ptr + targetIndexEntryOffset, parseKeyPtr + PTR_SIZE, keyridSize);
        /**
         * update the meta data
         * **/

        ptr = (char *)page;
        ptr += PAGE_SIZE - sizeof(pType) - 2 * sizeof(unsigned) - 2 * numberOfKeys * sizeof(unsigned);
        auto *updateStartMetaDataPointer = ptr;
        unsigned count = 0;
        while (count < numberOfKeys - searchOffset) {
            unsigned metaDataOffset = *(unsigned *)(updateStartMetaDataPointer + sizeof(unsigned));
            metaDataOffset += keyridSize + PTR_SIZE;
            memcpy(updateStartMetaDataPointer + sizeof(unsigned), &metaDataOffset, sizeof(unsigned));
            count++;
            updateStartMetaDataPointer += RID_SIZE;
        }

        memmove(ptr - RID_SIZE, ptr, 2 * (numberOfKeys - searchOffset) * sizeof(unsigned));
        // copy the new inserted meta data key information
        memcpy(targetIndexEntryPtr,&keyridSize ,sizeof(unsigned));
        memcpy(targetIndexEntryPtr + sizeof(unsigned),&targetIndexEntryOffset, sizeof(unsigned));
    } else {
        auto *ptr = (char *)page;
        // locate to the last meta data
        ptr += PAGE_SIZE - sizeof(pType) - RID_SIZE - 2 * numberOfKeys * sizeof(unsigned);
        unsigned lastKeyOffset = *(unsigned *)(ptr + sizeof(unsigned));
        unsigned lastKeyLength = *(unsigned *)(ptr);


        /**
         * To calculate the key boundary, we need to count into the right most page pointer;
         * **/
        unsigned keyBoundary = lastKeyOffset + lastKeyLength + sizeof(unsigned);

        // insert next to the target index entry
        unsigned insertOffset = targetIndexEntryOffset + targetIndexEntryLength + sizeof(unsigned);
        ptr = (char *)page;
        memmove(ptr + insertOffset + keyridSize + PTR_SIZE, ptr + insertOffset, keyBoundary - insertOffset);
        auto *keyPtr = (char *)key + sizeof(unsigned);
        memcpy(ptr + insertOffset, keyPtr, keyridSize);
        memcpy(ptr + insertOffset + keyridSize, &rightPagePtr, PTR_SIZE);

        /**
         * update the meta data
         * **/
        auto *updateStartMetaDataPointer = targetIndexEntryPtr - RID_SIZE;
        unsigned count = 0;
        while (count < numberOfKeys - searchOffset - 1) {
            unsigned metaKeyOffset = *(unsigned *)(updateStartMetaDataPointer + sizeof(unsigned));
            metaKeyOffset += keyridSize + PTR_SIZE;
            memcpy(updateStartMetaDataPointer + sizeof(unsigned), &metaKeyOffset, sizeof(unsigned));
            updateStartMetaDataPointer -= RID_SIZE;
            count++;
        }

        // insert the target key next to the key we found using binary search, this is different from inserting into the left of the key.
        // in this case, we don't need to move the key we found, but just insert next to the key, so the number of the keys that should be
        // move is numberOfKeys - searchOffset - 1
        auto *moveStartMetaDataPointer = (char *)page + PAGE_SIZE - TYPE_PTR - 2 * sizeof(unsigned) - numberOfKeys * RID_SIZE;


        memset(moveStartMetaDataPointer - RID_SIZE, 0, RID_SIZE);
        memmove(moveStartMetaDataPointer - RID_SIZE, moveStartMetaDataPointer, (numberOfKeys - searchOffset - 1) * RID_SIZE);

        auto *insertStartMetaDataPointer = targetIndexEntryPtr - RID_SIZE;
        memcpy(insertStartMetaDataPointer, &keyridSize, sizeof(unsigned));
        memcpy(insertStartMetaDataPointer + sizeof(unsigned), &insertOffset, sizeof(unsigned));
    }

    /**
     * Change the free space and the number of keys
     * **/
    auto *ptr = (char *)page + PAGE_SIZE - TYPE_PTR - sizeof(unsigned);
    freeSpace -= keyridSize;
    freeSpace -= RID_SIZE;
    freeSpace -= PTR_SIZE; // for the space of pointer
    memcpy(ptr, &freeSpace, sizeof(unsigned));

    numberOfKeys ++;
    ptr -= sizeof(unsigned);
    memcpy(ptr, &numberOfKeys, sizeof(unsigned));

    return 0;
}
/**
 * In the function insertEntry()
 * 1. we decide whether here is the first insert, if it is, we initialize a dummy page and insert that data entries
 * 2. else we recursively call the helper function to pinpoint where the data entry is
 * **/
RC IndexManager::insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {
    // we first decide whether is a first time insert or not, if it is, we first initialize the dummy index
    if (ixFileHandle.fileHandler.getNumberOfPages() == 0) {
        // insert the dummy key into the page ZERO
        initialDummyPage(ixFileHandle, attribute);
        // append new page to place the real data entries
        void *dataPage = malloc(PAGE_SIZE);
        memset(dataPage,0,PAGE_SIZE);
        PageNum initialPageID;
        initialDataEntriesPage(ixFileHandle, dataPage, initialPageID);
        alterRootPointer(ixFileHandle, attribute, initialPageID);
        insertDataEntriesKey(dataPage, attribute,key,rid);
        ixFileHandle.writeIxPage(initialPageID,dataPage);
        free(dataPage);
        return 0;
    }

    void *childKey = nullptr;
    unsigned rootPageID;
    getRootPageID(ixFileHandle, rootPageID);
    childKey = helper(ixFileHandle, attribute, key, rid, rootPageID);
    if (childKey) {
        PageNum newIndexPageID;
        void *indexPage = malloc(PAGE_SIZE);
        memset(indexPage,0,PAGE_SIZE);
        initialIndexEntriesPage(ixFileHandle, indexPage, newIndexPageID);

        /**
         * After we intialize a new page for index, we need to insert it, the same as the function we need for insertDataEntriesKey;
         * **/
        insertIndexEntriesKey(indexPage, attribute, childKey);
        // change the root the the dummy points to, also we need to create a index data page to store the root index entires
        alterRootPointer(ixFileHandle, attribute, newIndexPageID);


        ixFileHandle.writeIxPage(newIndexPageID,indexPage);
        free(indexPage);
        free(childKey);
    }

    return 0;
}
/**
 * In index entries page, the structure is:
 * The first unsigned char indicates whether it is a index entries page
 * The second unsigned indicates its free space offset
 * The third unsigned indicates its number of records
 * Then it follows up with index metadata, (the length of one index entries, the offset of that index entries)
 * **/
pType IndexManager::getPageType(const void *page) {
    auto *ptr = (char *)page;
    // decide which type of the node it is
    pType pagetype = *(ptr + PAGE_SIZE - sizeof(pType));
    return pagetype;
}
/**
 * Key structure is : key + RID
 * i.e: INT = int + rid
 *      Float = Float + rid
 *      String = indicator(unsigned) + content + rid
 * IMPORTANT!!!:
 *      Index entries = pointer + key + rid + overflow page pointer.
 *      return the key contains real key and rid as well as the pointer
 * **/

/**
 * Get the data entries inserting in a data node, the data entries may contain data [content + rid] based on the type of the data
 * **/
RC IndexManager::getInsertingDataKeySize(const Attribute &attribute, const void *key, unsigned &keySize) {
    if (attribute.type == TypeInt) {
        keySize = sizeof(unsigned);
    } else if (attribute.type == TypeReal) {
        keySize = sizeof(float);
    } else {
        unsigned varcharLength = *(unsigned *)key;
        keySize = sizeof(unsigned) + varcharLength;
    }
    return 0;
}
/**
 * Insert the attribute in the sorting order, we assume that the free space is larger or equal to the length of the key
 * 1. use binary search to find the first element that is smaller or equal than the inserting key
 * 2. if there is key element that can satisfy the condition.
 * 3. insert the inserting key into the left most part.
 * 4. otherwise insert key next to the search key
 *
 * Binary seach is so buggy, so I use linear scan..........
 * **/
RC IndexManager::insertDataEntriesKey(void *page, const Attribute &attribute, const void *key, const RID &rid) {

    //auto *ptr = (char *) page;
    unsigned numberOfKeys;
    unsigned freeSpace;
    unsigned char pageType;
    PageNum nextPagePointer;
    getDataEntriesPageInformation(page, pageType, nextPagePointer, freeSpace, numberOfKeys);

    unsigned keySize;
    getInsertingDataKeySize(attribute, key, keySize);
    unsigned keyridSize = keySize + RID_SIZE;
    int start = 0;
    auto *metaDataPtr = (char *) page + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) - RID_SIZE;

    while (start < (int)numberOfKeys) {

        //unsigned curEntryLength = *(unsigned *) (metaDataPtr);

        unsigned curEntryOffset = *(unsigned *) (metaDataPtr + sizeof(unsigned));

        /**get the real key of the current pointer points to and compare with the inserting key**/
        auto *ptr = (char *) page + curEntryOffset;

        if (keyCompare(attribute, key, rid, ptr) < 0) {
            /**we found the first element that is larger than the inserting entry**/
            auto *lastMetaDataPtr =
                    (char *) page + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) - numberOfKeys * RID_SIZE;

            unsigned lastEntryLength = *(unsigned *) lastMetaDataPtr;
            unsigned lastEntryOffset = *(unsigned *) (lastMetaDataPtr + sizeof(unsigned));

            unsigned entryBound = lastEntryLength + lastEntryOffset;


            auto *searchRealEntryPtr = (char *) page + curEntryOffset;

            memmove(searchRealEntryPtr + keyridSize, searchRealEntryPtr, entryBound - curEntryOffset);

            memcpy(searchRealEntryPtr, key, keySize);
            memcpy(searchRealEntryPtr + keySize, &rid.pageNum, sizeof(unsigned));
            memcpy(searchRealEntryPtr + keySize + sizeof(unsigned), &rid.slotNum, sizeof(unsigned));

            /**
             * update the meta data
             * **/
            unsigned count = 0;
            auto *searchMetaDataPtr =
                    (char *) page + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) - (start + 1) * RID_SIZE;
            while (count < numberOfKeys - start) {
                //unsigned entryLength = *(unsigned *) searchMetaDataPtr;
                unsigned entryOffset = *(unsigned *) (searchMetaDataPtr + sizeof(unsigned));

                entryOffset += keyridSize;
                memcpy(searchMetaDataPtr + sizeof(unsigned), &entryOffset, sizeof(unsigned));
                searchMetaDataPtr -= RID_SIZE;
                count++;
            }

            /**
            * move the meta data with one RID_SIZE, update them
            * and insert new meta data record
            * **/
            lastMetaDataPtr = (char *) page + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) -
                              2 * numberOfKeys * sizeof(unsigned);
            memmove(lastMetaDataPtr - RID_SIZE, lastMetaDataPtr, (numberOfKeys - start) * RID_SIZE);

            searchMetaDataPtr =
                    (char *) page + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) - (start + 1) * RID_SIZE;

//            unsigned testMidLength = *(unsigned *)((char *)page + PAGE_SIZE - PTR_SIZE - NXT_PTR - RID_SIZE - 56 * RID_SIZE);
//            cout << "mid value's length: " << testMidLength << endl;
            memcpy(searchMetaDataPtr, &keyridSize, sizeof(unsigned));
            memcpy(searchMetaDataPtr + sizeof(unsigned), &curEntryOffset, sizeof(unsigned));

            auto *basicInfoUpdatePtr = (char *) page + PAGE_SIZE - TYPE_PTR - NXT_PTR - sizeof(unsigned);
            freeSpace -= keyridSize;
            freeSpace -= RID_SIZE;
            memcpy(basicInfoUpdatePtr, &freeSpace, sizeof(unsigned));

            numberOfKeys++;
            basicInfoUpdatePtr -= sizeof(unsigned);
            memcpy(basicInfoUpdatePtr, &numberOfKeys, sizeof(unsigned));
            return 0;
        }
        start++;
        metaDataPtr -= RID_SIZE;
    }


    /**we can't find such element that is larger than our inserting key, so we insert the key into the right most**/
    auto *lastMetaDataPtr =
            (char *) page + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) - numberOfKeys * RID_SIZE;

    unsigned lastEntryOffset = 0;
    unsigned lastEntryLength = 0;
    if (numberOfKeys != 0) {
        lastEntryOffset = *(unsigned *) (lastMetaDataPtr + sizeof(unsigned));
        lastEntryLength = *(unsigned *) (lastMetaDataPtr);
    }

    unsigned entryBound = lastEntryLength + lastEntryOffset;

    memcpy(lastMetaDataPtr - RID_SIZE, &keyridSize, sizeof(unsigned));
    memcpy(lastMetaDataPtr - sizeof(unsigned), &entryBound, sizeof(unsigned));

    auto *realDataPtr = (char *) page + entryBound;
    memcpy(realDataPtr, key, keySize);
    memcpy(realDataPtr + keySize, &rid.pageNum, sizeof(unsigned));
    memcpy(realDataPtr + keySize + sizeof(unsigned), &rid.slotNum, sizeof(unsigned));

    auto *basicInfoUpdatePtr = (char *) page + PAGE_SIZE - TYPE_PTR - NXT_PTR - sizeof(unsigned);
    freeSpace -= keyridSize;
    freeSpace -= RID_SIZE;
    memcpy(basicInfoUpdatePtr, &freeSpace, sizeof(unsigned));

    numberOfKeys++;
    basicInfoUpdatePtr -= sizeof(unsigned);
    memcpy(basicInfoUpdatePtr, &numberOfKeys, sizeof(unsigned));

    /*auto *data = (char *)page;
    for (int i = 0; i < numberOfKeys; i++) {
        cout << "key in page: " << *(unsigned *)(data);
        data += 3 * sizeof(unsigned);
    }
    cout << endl;*/
    return 0;
}
/**
 * to split two types of nodes: Data node and index node
 * the overflow page is initially empty, waiting for the entries to insert in
 * **/

RC IndexManager::splitIndexNode(void *page, void *overflowPage, void* returnedKey,
                                const Attribute &attribute, const void *key, const PageNum overflowPageID,const PageNum PID) {

    pType pageType;

    unsigned freeSpace;
    unsigned numberOfKeys;
    getIndexEntriesPageInformation(page, pageType, freeSpace, numberOfKeys);


    int mid = numberOfKeys / 2;
    byte* page_ptr = (byte*)page + PAGE_SIZE - sizeof(pType)  - RID_SIZE;
    page_ptr -= mid*RID_SIZE;
    unsigned midKeyLength = *(unsigned*)page_ptr;
    unsigned midKeyOffset = *(unsigned*)(page_ptr + sizeof(unsigned));

    page_ptr = (byte*)page + midKeyOffset;
    byte* midKey = page_ptr;

    //unsigned pageID = *(unsigned *)(page_ptr+ midKeyLength - RID_SIZE);
    //unsigned slotID = *(unsigned *)(page_ptr+ midKeyLength - RID_SIZE + sizeof(unsigned));

    //move the right part to the overflow page
    int index = mid + 1;
    page_ptr = (byte*)page + PAGE_SIZE - sizeof(pType) - RID_SIZE;
    page_ptr -= mid*RID_SIZE;

    unsigned beforeNumber = numberOfKeys;
    while(index <= (int)beforeNumber){
        page_ptr -= RID_SIZE;

        unsigned nextKeyLength = *(unsigned*)page_ptr;
        unsigned nextKeyOffset = *(unsigned*)(page_ptr + sizeof(unsigned));

        byte* next_ptr = (byte*)page + nextKeyOffset;

        //Insert Key Format: pointer + key + rid + pointer
        void* tempKey = next_ptr - sizeof(PageNum);


        //cout << "key content: " << *(int*)next_ptr << "  left: " << *(PageNum*) tempKey << "  right: " << *(PageNum*)(next_ptr + nextKeyLength) << endl;


        //RID nextRid;
        //nextRid.pageNum = *(unsigned *)(next_ptr + nextKeyLength - RID_SIZE);
        //nextRid.slotNum = *(unsigned *)(next_ptr + nextKeyLength - RID_SIZE + sizeof(unsigned));
        insertIndexEntriesKey(overflowPage, attribute, tempKey);
        numberOfKeys --;
        //clear the right part of entries
        freeSpace += nextKeyLength+RID_SIZE + sizeof(PageNum);
        memset(tempKey,0,nextKeyLength+sizeof(PageNum));
        memset(page_ptr,0,RID_SIZE);
        if(index == (int)beforeNumber){
            memset((char *)tempKey+nextKeyLength+sizeof(PageNum),0,sizeof(PageNum));
        }
        index++;
    }
    memset(midKey+midKeyLength,0,PTR_SIZE);

    freeSpace += sizeof(PageNum);

    page_ptr = (byte*)page + PAGE_SIZE - sizeof(pType) - RID_SIZE;

    memcpy(page_ptr,&numberOfKeys,sizeof(unsigned));
    memcpy(page_ptr+sizeof(unsigned),&freeSpace,sizeof(unsigned));

    byte* key_ptr = (byte*)key+sizeof(PageNum);
    RID rid;
    rid.pageNum = 0;
    rid.slotNum = 0;
    switch(attribute.type){
        case TypeInt:
            rid.pageNum = *(unsigned*) (key_ptr + sizeof(int));
            rid.slotNum = *(unsigned*) (key_ptr + sizeof(int)+sizeof(unsigned));
            break;
        case TypeReal:
            rid.pageNum = *(unsigned*) (key_ptr + sizeof(float));
            rid.slotNum = *(unsigned*) (key_ptr + sizeof(float)+sizeof(unsigned));
        case TypeVarChar:
            int vc_len = *(int*) (key_ptr);
            rid.pageNum = *(unsigned*) (key_ptr + sizeof(int)+vc_len);
            rid.slotNum = *(unsigned*) (key_ptr + sizeof(int)+vc_len+sizeof(unsigned));
            break;
    }
    //cout << "key content: " << *(int*)key_ptr << "left:" << *(PageNum*) key << "right: " << *(PageNum*) (key_ptr + 12) << endl;
    if(keyCompare(attribute, key_ptr, rid, midKey) < 0){
        insertIndexEntriesKey(page, attribute, key);
    }
    else{
        insertIndexEntriesKey(overflowPage, attribute, key);
    }
    memcpy((byte*)returnedKey,&PID,sizeof(PageNum));
    memcpy((byte*)returnedKey + sizeof(PageNum),midKey,midKeyLength);
    memcpy((byte*)returnedKey + sizeof(PageNum)+midKeyLength,&overflowPageID,sizeof(unsigned));


    return 0;
}

/**
 * return the key which has key + rid
 * **/
RC IndexManager::splitDataNode(void *page, void *overflowPage, void* returnedKey,
                               const Attribute &attribute, const void *key, const RID &rid, const PageNum overflowPageID, const PageNum PID){

    unsigned char pageType;
    unsigned freeSpace;
    unsigned numberOfKeys;
    //void *newRootKey = nullptr;
    PageNum nextPagePointer;
    getDataEntriesPageInformation(page,pageType,nextPagePointer,freeSpace,numberOfKeys);


    memcpy((byte*)overflowPage+PAGE_SIZE - sizeof(pType) - PTR_SIZE,&nextPagePointer,PTR_SIZE);

    int mid = numberOfKeys / 2;
    byte* page_ptr = (byte*)page + PAGE_SIZE - sizeof(pType) - NXT_PTR - RID_SIZE;
    page_ptr -= mid*RID_SIZE;
    unsigned midKeyLength = *(unsigned*)page_ptr;

    unsigned midKeyOffset = *(unsigned*)(page_ptr + sizeof(unsigned));

    page_ptr = (byte*)page + midKeyOffset;
    byte* midKey = page_ptr;

    memcpy((byte*)returnedKey,&PID,sizeof(PageNum));
    memcpy((byte*)returnedKey + sizeof(PageNum),midKey,midKeyLength);
    memcpy((byte*)returnedKey + sizeof(PageNum)+midKeyLength,&overflowPageID,sizeof(unsigned));

    //move the right part to the overflow page
    int index = mid + 1;
    page_ptr = (byte*)page + PAGE_SIZE - sizeof(pType) - NXT_PTR - RID_SIZE;
    page_ptr -= mid*RID_SIZE;
    unsigned beforeNumber = numberOfKeys;
    while(index <= (int)beforeNumber){
        page_ptr -= sizeof(unsigned)*2;

        unsigned nextKeyLength = *(unsigned*)page_ptr;
        unsigned nextKeyOffset = *(unsigned*)(page_ptr + sizeof(unsigned));

        byte* next_ptr = (byte*)page + nextKeyOffset;

        void* tempKey = next_ptr;
        RID nextRid;
        nextRid.pageNum = *(unsigned *)(next_ptr + nextKeyLength - RID_SIZE);
        nextRid.slotNum = *(unsigned *)(next_ptr + nextKeyLength - RID_SIZE + sizeof(unsigned));
        insertDataEntriesKey(overflowPage, attribute, tempKey, nextRid);

        //clear the right part of entries
        numberOfKeys --;
        freeSpace += nextKeyLength+RID_SIZE;
        memset(next_ptr,0,nextKeyLength);
        memset(page_ptr,0,RID_SIZE);
        index++;
    }

    page_ptr = (byte*)page + PAGE_SIZE - sizeof(pType) - PTR_SIZE - RID_SIZE;
    memcpy(page_ptr,&numberOfKeys,sizeof(unsigned));
    memcpy(page_ptr+sizeof(unsigned),&freeSpace,sizeof(unsigned));
    memcpy(page_ptr+sizeof(unsigned)*2,&overflowPageID,sizeof(PageNum));


    if(keyCompare(attribute, key, rid, midKey) < 0){
        insertDataEntriesKey(page, attribute, key, rid);
    }
    else{
        insertDataEntriesKey(overflowPage, attribute, key, rid);

    }
    /*cout << "when inserting" << *(int*)key << endl;
    cout << "Split key " << *(int*)((byte*)returnedKey+sizeof(PageNum)) << "  left: " << *(PageNum*)returnedKey << "  right:  " << *(PageNum*)((byte*)returnedKey+midKeyLength+4) << endl; 
    cout << "key length: " << midKeyLength << "pid: " << PID <<  "overflowPageID: " << overflowPageID << endl;*/
    return 0;
}

/**
 * moveCursorToFirstIndexMetaEntires() && moveCursorToFirstDataMetaEntries()
 * move cursor to point to the first meta data of index/data entries. i.e. data[0], index[0[, ptr = 0
 * **/
void* IndexManager::moveCursorToFirstIndexMetaEntires(void *indexPage) {
    void* ptr;
    ptr = ((char *)indexPage + PAGE_SIZE - sizeof(pType) - 4 * sizeof(unsigned));
    return ptr;
}

void* IndexManager::moveCursorToFirstDataMetaEntries(void *dataPage) {
    void* ptr;
    ptr = ((char *)dataPage + PAGE_SIZE - sizeof(pType) - NXT_PTR - 4 * sizeof(unsigned));
    return ptr;
}

/**
 * return -1 if key1 < key2
 *         0 if key1 == key2
 *         1 if key1 > key2
 * **/
RC IndexManager::keyCompare(const Attribute &attribute, const void *key1, const RID &rid1, const void *key2) {
    //corner case: key is null, indicating infinity:
    if(key1 == nullptr){
        if(rid1.pageNum == 0 && rid1.slotNum == 0){
            // This indecate it is a low key. Treat it as -infinity
            return -1;
        }
        else if(rid1.pageNum == UINT_MAX - 1 && rid1.slotNum == UINT_MAX - 1){
            // This indecate it is a high key. Treat it as +infinity
            return 1;
        }
    }

    if (attribute.type == TypeInt) {

        /**
         * INT Format: INT + rid
         * **/
        int key1Value = *(int *) key1;
        unsigned page1id = rid1.pageNum;
        unsigned slot1id = rid1.slotNum;

        int key2Value = *(int *) key2;
        unsigned page2id = *(unsigned *) ((char *) key2 + sizeof(unsigned));
        unsigned slot2id = *(unsigned *) ((char *) key2 + sizeof(int) + sizeof(unsigned));

        if (key1Value < key2Value) return -1;
        if (key1Value > key2Value) return 1;

        if (page1id < page2id) return -1;
        if (page1id > page2id) return 1;

        if (slot1id < slot2id) return -1;
        if (slot1id > slot2id) return 1;

        return 0;
    } else if (attribute.type == TypeReal) {
        float key1ValueFLOAT = *(float *) key1;
        unsigned page1idFLOAT = rid1.pageNum;
        unsigned slot1idFLOAT = rid1.slotNum;

        float key2ValueFLOAT = *(float *) key2;
        unsigned page2idFLOAT = *(unsigned *) ((char *) key2 + sizeof(float));;
        unsigned slot2idFLOAT = *(unsigned *) ((char *) key2 + sizeof(float) + sizeof(unsigned));;

        if (key1ValueFLOAT < key2ValueFLOAT) return -1;
        if (key1ValueFLOAT > key2ValueFLOAT) return 1;

        if (page1idFLOAT < page2idFLOAT) return -1;
        if (page1idFLOAT > page2idFLOAT) return 1;

        if (slot1idFLOAT < slot2idFLOAT) return -1;
        if (slot1idFLOAT > slot2idFLOAT) return 1;

        return 0;
    } else {
        unsigned varChar1Length = *(unsigned *) key1;
        char varchar1[varChar1Length + 1];
        for (unsigned i = 0; i < varChar1Length; i++) {
            varchar1[i] = *((char *)key1 + sizeof(unsigned) + i);
        }
        varchar1[varChar1Length] = '\0';
        unsigned page1ID = rid1.pageNum;
        unsigned slot1ID = rid1.slotNum;

        unsigned varChar2Length = *(unsigned *) key2;
        char varchar2[varChar2Length + 1];
        for (unsigned i = 0; i < varChar2Length; i++) {
            varchar2[i] = *((char *)key2 + sizeof(unsigned) + i);
        }
        varchar2[varChar2Length] = '\0';

        unsigned page2ID = *(unsigned *) ((char *) key2 + sizeof(unsigned) + varChar2Length);
        unsigned slot2ID = *(unsigned *) ((char *) key2 + sizeof(unsigned) + varChar2Length + sizeof(unsigned));

        if (strcmp(varchar1, varchar2) < 0) return -1;
        if (strcmp(varchar1, varchar2) > 0) return 1;

        if (page1ID < page2ID) return -1;
        if (page1ID > page2ID) return 1;

        if (slot1ID < slot2ID) return -1;
        if (slot1ID > slot2ID) return 1;

        return 0;
    }

    return 0;
}

RC IndexManager::getIndexEntriesPageInformation(const void *page, pType &pageType, unsigned &freeSpace,
                                                unsigned &numberOfKeys) {
    auto *ptr = (char *)page + PAGE_SIZE;

    ptr -= sizeof(pType);
    pageType = *(pType *)(ptr);

    ptr -= sizeof(unsigned);
    freeSpace = *(unsigned *)ptr;

    ptr -= sizeof(unsigned);
    numberOfKeys = *(unsigned *)ptr;

    return 0;
}

/**
 * helper function is recursively search for the key in the B+ tree based on the key and rid
 * set the value of the left pointer of the key
 * and return the pointer that points to the start of the key content
 * **/
void *IndexManager::helper(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key,
                           const RID &rid, const unsigned &currentPageID){
    void* childKey = nullptr;

    // first we get the key that is the first key that is greater or equal than the inserting key
    void * page = malloc(PAGE_SIZE);
    memset(page,0,PAGE_SIZE);
    if (ixFileHandle.readIxPage(currentPageID, page) == -1){
        perror(&"the read operation has something with insertEntries. No page found for id: " [ currentPageID]);
    }

    pType pagetype = getPageType(page);

    if (pagetype == INDEXENTRIES) {
        /**
         * Here is the index page, we first
         * 1. do the binary search
         * 2. goes to the specific branch
         * 3. And then after insertion, we decide if there is need to split the current index node
         * **/
        /**
         * find the last element that is smaller or equal to the key
         * get the start offset of the search key
         * **/
        /*if(currentPageID == 174){
            cout << "PID: " << currentPageID << endl;
            byte* test_ptr = (byte*) moveCursorToFirstIndexMetaEntires(page);
            unsigned nok = *(unsigned*) (test_ptr + RID_SIZE);
            cout << "nok: " << nok << endl;

            for(int i = 0;i<nok;i++){
                test_ptr = (byte*) moveCursorToFirstIndexMetaEntires(page);
                test_ptr -= RID_SIZE * i;
                unsigned l = *(unsigned*) test_ptr;
                unsigned o = *(unsigned*) (test_ptr+sizeof(unsigned));
                
                cout << "len: " << l << "   offset:" << o << endl;
                test_ptr = (byte*) page + o;
                unsigned getKey = *(unsigned*)test_ptr;
                cout << "key" << getKey << endl;
                cout << "left: " << *(unsigned*)(test_ptr-sizeof(PageNum)) << endl;
                cout << "right: " << *(unsigned*)(test_ptr+3*sizeof(int)) << endl;
            }
        }*/


        int search;

        indexBinarySearch(page,attribute,key,rid,search);

        /**
         * we need to decide whether the key is larger than all of the index entries or not.
         * In other words, we are validating whether the index entries the to which the endOffset points is really bigger than key
         * **/
        /*cout << "cur PID:" << currentPageID << endl;
        cout << "search index: " << search << endl;
        //cout << "key to be inserted: " << *(int*) key << endl;
        cout << "Returned key:" << *(int*) test_key << endl;
        cout << "left: " << *(PageNum*)(test_key- PTR_SIZE) << endl;
        cout << "right: " << *(PageNum*)(test_key+ *(int*)test_key + 12) << endl;*/



        if (search == -1) {
            auto *temp = (char *)page + PAGE_SIZE - TYPE_PTR - 2 * sizeof(unsigned) - RID_SIZE;
            // if the first key is sill larger than the key we want, then goes to the first pointer
            unsigned firstKeyOffset = *(unsigned *)(temp + sizeof(unsigned));
            //unsigned firstKeyLength = *(unsigned *)(temp);

            temp = (char *)page + firstKeyOffset;

            // get the leftmost pointer
            const unsigned firstPageID = *(unsigned *)(temp - sizeof(unsigned));

            childKey = helper(ixFileHandle, attribute, key, rid, firstPageID);

        } else {

            auto *temp = (char *)page + PAGE_SIZE - TYPE_PTR - 2 * sizeof(unsigned) - RID_SIZE - search * RID_SIZE;
            unsigned searchKeyLength = *(unsigned *)temp;
            unsigned searchKeyOffset = *(unsigned *)(temp + sizeof(unsigned));

//            cout << "search key: " << search << " " << searchKeyLength << " " << searchKeyOffset << endl;
//            cout <<  "wrong length: "<< *(PageNum *)((char *)page + searchKeyOffset);
            PageNum keyPagePointer = *(PageNum *)((char *)page + searchKeyOffset + searchKeyLength);
            //if(currentPageID == 174) cout << "left: " << *(PageNum *)((char *)page + searchKeyOffset - sizeof(PageNum)) << "  right: " << keyPagePointer << endl;
            // otherwise goes to the left pointer of the specific key that end offset refers to
            childKey = helper(ixFileHandle, attribute, key, rid, keyPagePointer);
        }

        if (childKey == nullptr) {
            free(page);
            return nullptr;
        }
        /**
         *
         * **/
        pType pageType;
        unsigned freeSpace;
        unsigned numberOfKeys;
        getIndexEntriesPageInformation(page,pageType,freeSpace,numberOfKeys);

        auto *ptrForInsertingKey = (char *)childKey;
        ptrForInsertingKey += PTR_SIZE;
        unsigned wholeKeySize;
        wholeKeySize = getKeySize(attribute,ptrForInsertingKey);
        wholeKeySize += 2 * PTR_SIZE + RID_SIZE;

        if (freeSpace < wholeKeySize) {
            // we split child, insert *childKey in current index node
            void *overflowPage = malloc(PAGE_SIZE);
            memset(overflowPage,0,PAGE_SIZE);
            PageNum overflowPageID = 0;
            initialIndexEntriesPage(ixFileHandle, overflowPage, overflowPageID);

            void *returnKey = malloc(PAGE_SIZE);
            memset(returnKey,0,PAGE_SIZE);
            splitIndexNode(page, overflowPage, returnKey, attribute, childKey, overflowPageID, currentPageID);

            ixFileHandle.writeIxPage(currentPageID, page);
            ixFileHandle.writeIxPage(overflowPageID, overflowPage);
            free(childKey);
            free(overflowPage);
            free(page);
            return returnKey;
        } else {
            /*cout << "=================before==================" << endl;
            cout << "key to be inserted: " << *(int*) ((byte*) childKey + PTR_SIZE) << endl;
            cout << "inserted key left: " << *(int*) childKey << endl;
            cout << "inserted key right: " << *(int*) ((byte*) childKey + PTR_SIZE + 12) << endl;

            unsigned tnumberOfKeys = *(unsigned *)((char *)page + PAGE_SIZE - TYPE_PTR  - 2 * sizeof(unsigned));
            cout << "number of Keys: " << tnumberOfKeys << endl;
            char *testMDP = ((char *)page + PAGE_SIZE - TYPE_PTR  - 2 * sizeof(unsigned) - RID_SIZE);
            int count = 0;
            while (count  < tnumberOfKeys) {
                unsigned offset = *(unsigned *)(testMDP + sizeof(unsigned));
                auto *RDP = (char *)page + offset;
                unsigned length = *(unsigned *)RDP;
                cout << " key:" << length << endl;
                cout << "left: " << *(PageNum*)(RDP- PTR_SIZE) << endl;
                cout << "right: " << *(PageNum*)(RDP+  12) << endl;
                count++;
                testMDP -= RID_SIZE;
            }
            cout << endl;*/

            insertIndexEntriesKey(page,attribute,childKey);

            /*cout << "================After=====================" << endl;

            tnumberOfKeys = *(unsigned *)((char *)page + PAGE_SIZE - TYPE_PTR  - 2 * sizeof(unsigned));
            cout << "number of Keys: " << tnumberOfKeys << endl;
            testMDP = ((char *)page + PAGE_SIZE - TYPE_PTR  - 2 * sizeof(unsigned) - RID_SIZE);
            count = 0;
            while (count  < tnumberOfKeys) {
                unsigned offset = *(unsigned *)(testMDP + sizeof(unsigned));
                auto *RDP = (char *)page + offset;
                unsigned length = *(unsigned *)RDP;
                cout << " key:" << length << endl;
                cout << "left: " << *(PageNum*)(RDP- PTR_SIZE) << endl;
                cout << "right: " << *(PageNum*)(RDP + 12) << endl;
                count++;
                testMDP -= RID_SIZE;
            }
            cout << endl;*/


            /*getIndexEntriesPageInformation(page,pageType,freeSpace,numberOfKeys);
            if(numberOfKeys > 3){
                getIndexEntriesPageInformation(page,pageType,freeSpace,numberOfKeys);
                byte* test_ptr = (byte*) moveCursorToFirstIndexMetaEntires(page);
                test_ptr -= RID_SIZE * (numberOfKeys - 3);
                unsigned test_len = *(unsigned*) test_ptr;
                unsigned test_offset = *(unsigned*)(test_ptr+sizeof(unsigned));
                int test_key = *(int*) ((byte*) page + test_offset);
                cout << "test key: " << test_key << endl;
            }*/

            ixFileHandle.writeIxPage(currentPageID,page);
            free(childKey);
            free(page);
            return nullptr;
        }
    } else {
        // to decide whether the free space of that data entries is enough or not
        unsigned freeSpace;
        unsigned numberOfKeys;
        unsigned char pageType;

        PageNum nextPagePointerID;
        getDataEntriesPageInformation(page,pageType,nextPagePointerID,freeSpace,numberOfKeys);

        unsigned keySize = getKeySize(attribute, key);
        if (keySize + RID_SIZE + RID_SIZE <= freeSpace) {
            insertDataEntriesKey(page, attribute, key, rid);
            ixFileHandle.writeIxPage(currentPageID,page);

        } else {
            /**
             * We split to a new overflow data page, and write half of the data into the page.
             * **/
            void *overflowPage = malloc(PAGE_SIZE);
            memset(overflowPage,0,PAGE_SIZE);
            PageNum overflowPageID;
            initialDataEntriesPage(ixFileHandle, overflowPage, overflowPageID);

            void *returnKey = malloc(PAGE_SIZE);

            splitDataNode(page, overflowPage, returnKey, attribute, key, rid, overflowPageID,currentPageID);
            //cout << "has been splitted. key: " << *(int*) key << " return: " << *(int*) ((byte*)returnKey + PTR_SIZE) << "  overflowPageID: " << overflowPageID << "PID  " << currentPageID << endl;

//            auto *test = (char *)overflowPage + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) - RID_SIZE;
//            cout << "over flow page information" << endl;
//            unsigned temp = 0;
//            cout << "data segment:(";
//                cout << ") --- ";
//                while (temp < numberOfKeys) {
//                    unsigned offset = *(unsigned *)(test + sizeof(unsigned));
//                    unsigned length = *(unsigned *)test;
//                    auto *data = (char *)page + offset;
//                    unsigned varcharLength = *(unsigned *)data;
//                    cout << "( ";
//                    for(int i = 0; i < varcharLength; i++) {
//                        cout << *(char * )(data + sizeof(unsigned) + i);
//                    }
//                    cout << " )";
//                    temp++;
//                    test -= RID_SIZE;
//                }
//                cout << endl;
            /**
             * The new root contains pointer to the current page pointer, key+rid, overflow page pointer to the next page
             * **/
            ixFileHandle.writeIxPage(currentPageID,page);
            ixFileHandle.writeIxPage(overflowPageID,overflowPage);
            free(overflowPage);
            free(page);
            return returnKey;
        }

    }

    free(page);
    return nullptr;
}

/**
 * delete entry that points by the searchRealDataPtr pointer
 *
 * **/
RC IndexManager::deleteDataEntriesKey(void *page, const void *searchMetaDataPtr, const unsigned numberOfUpdateMetaData) {

    /**get the length, offset of the search data entry**/
    unsigned searchEntryLength = *(unsigned *)((char *)searchMetaDataPtr);
    unsigned searchEntryOffset = *(unsigned *)((char *)searchMetaDataPtr + sizeof(unsigned));

    char *searchRealDataPtr = (char *)page + searchEntryOffset;

    unsigned numberOfKeys = *(unsigned *)((char *)page + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned));

    char *lastMetaDataPtr = (char *)page + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) - numberOfKeys * RID_SIZE;
    char *nextDataEntryPtr = searchRealDataPtr + searchEntryLength;

    unsigned lastDataEntryOffset = *(unsigned *)(lastMetaDataPtr + sizeof(unsigned));
    unsigned lastDataEntryLength = *(unsigned *)(lastMetaDataPtr);

    unsigned entryBound = lastDataEntryLength + lastDataEntryOffset;
    /**move the rest of the data segment, set the origin last data to 0**/
    memmove(nextDataEntryPtr - searchEntryLength, nextDataEntryPtr, entryBound - searchEntryOffset - searchEntryLength);
    memset((char *)page + lastDataEntryOffset, 0, lastDataEntryLength);
    /**
     * update the meta data, move the rest of the meta data
     * **/
    auto *nextSearchMetaDataPtr = (char *)searchMetaDataPtr - RID_SIZE;
    unsigned count = 0;
    while (count < numberOfUpdateMetaData) {
        unsigned updateMetaDataOffset = *(unsigned *)(nextSearchMetaDataPtr + sizeof(unsigned));
        updateMetaDataOffset -= searchEntryLength;
        memcpy(nextSearchMetaDataPtr + sizeof(unsigned), &updateMetaDataOffset, sizeof(unsigned));
        nextSearchMetaDataPtr -= RID_SIZE;
        count ++;
    }
    memmove(lastMetaDataPtr + RID_SIZE, lastMetaDataPtr, numberOfUpdateMetaData * RID_SIZE);
    memset(lastMetaDataPtr, 0, RID_SIZE);

    /**update the free space and number of keys**/

    unsigned freeSpace;
    auto *basicInfoPtr = (char *)page + PAGE_SIZE - TYPE_PTR - NXT_PTR - sizeof(unsigned);
    freeSpace += searchEntryLength + RID_SIZE;
    memcpy(basicInfoPtr, &freeSpace, sizeof(unsigned));

    basicInfoPtr -= sizeof(unsigned);
    numberOfKeys--;
    memcpy(basicInfoPtr, &numberOfKeys, sizeof(unsigned));
    return 0;
}
/**
 * Same logic as insertEntry, recursively go down to the leaf node
 * if the corresponding leaf node doesn't exist, return -1
 * other wise delete it, update the meta data
 * **/
RC IndexManager::deleteHelper(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid,
                              const unsigned curPageID) {
    void *curPage = malloc(PAGE_SIZE);
    memset(curPage,0,PAGE_SIZE);
    ixFileHandle.readIxPage(curPageID,curPage);
    auto *ptr = (char *)curPage;
    unsigned char pageType = *(ptr + PAGE_SIZE - TYPE_PTR);

    if (pageType == INDEXENTRIES) {
        unsigned char pageType;
        unsigned freeSpace;
        unsigned numberOfKeys;
        getIndexEntriesPageInformation(curPage,pageType,freeSpace,numberOfKeys);
        if (numberOfKeys == 0){
            free(curPage);
            return -1;
        }
        /**
         * assume we have binary search here and get the output result searchkey
         * **/
        int search;
        indexBinarySearch(curPage, attribute, key, rid, search);


        if (search == -1) {
            unsigned leftPageID = *(unsigned *)((char *)curPage);
            int ifDeleteSuccessfully = deleteHelper(ixFileHandle,attribute,key,rid,leftPageID);
            free(curPage);
            return ifDeleteSuccessfully;
        } else {
            auto *searchMetaDataKey = (char *)curPage + PAGE_SIZE - TYPE_PTR - 2 * sizeof(unsigned) - RID_SIZE - (search) * RID_SIZE;

            unsigned searchKeyOffset = *(unsigned *)(searchMetaDataKey + sizeof(unsigned));
            unsigned searchKeyLength = *(unsigned *)(searchMetaDataKey);
            auto *searchKey = (char *)curPage + searchKeyOffset;
            unsigned nextPageID;

            nextPageID = *(unsigned *)(searchKey + searchKeyLength);
            int ifDeleteSuccessfully = deleteHelper(ixFileHandle,attribute,key,rid,nextPageID);
            free(curPage);
            return ifDeleteSuccessfully;
        }
    } else {
        /**
         * Binary search to find the specific element, simplest BS here
         * **/
        unsigned char pageType;
        PageNum nextPtr;
        unsigned freeSpace;
        unsigned numberOfKeys;
        getDataEntriesPageInformation(curPage,pageType,nextPtr,freeSpace,numberOfKeys);

        if (numberOfKeys == 0) {
            free(curPage);
            return -1;
        }
        int start = 0;
        int end = (int)numberOfKeys - 1;



        char *searchRealDataPtr = nullptr;
        while (start <= end) {
            int mid = start + (end - start) / 2;
            char *leftMetaDatPtr = (char *)curPage + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) - RID_SIZE - mid * RID_SIZE;
            //unsigned midEntryLength = *(unsigned *)leftMetaDatPtr;
            unsigned midEntryOffset = *(unsigned *)(leftMetaDatPtr + sizeof(unsigned));
            searchRealDataPtr = (char *)curPage + midEntryOffset;
            if (keyCompare(attribute,key,rid,searchRealDataPtr) == 0) {

                /**
                 * find that key, delete it
                 * **/
                int sign = deleteDataEntriesKey(curPage,leftMetaDatPtr,numberOfKeys - mid - 1);
                ixFileHandle.writeIxPage(curPageID, curPage);
                free(curPage);

//                 unsigned tempNum = *(unsigned *)((char *)curPage + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned));
//                 auto *test = (char *)curPage;
//                 cout << "[" <<"delete keys" << *(float *)key << " " << rid.pageNum << " " << rid.slotNum << "]"<< "---";
//                 for (int i = 0; i < tempNum; i++) {
//                     cout << "( " <<*(float *)test << " " << *(unsigned *)(test + sizeof(float)) << " " << *(unsigned *)(test + sizeof(float) +
//                             sizeof(unsigned)) << " ) ";
//                     test += 3 * PTR_SIZE;
//                 }
//                 cout << endl;

//                 unsigned tempNum = *(unsigned *)((char *)curPage + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned));
//                 auto *test = (char *)curPage + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) - RID_SIZE;
//                 cout << "[" <<"delete keys" << *(float *)key << " " << rid.pageNum << " " << rid.slotNum << "]"<< "---";
//                 for (int i = 0; i < tempNum; i++) {
//                     cout << "( " <<*(unsigned *)test << " " << *(unsigned *)(test + sizeof(unsigned)) << " ) ";
//                     test -= RID_SIZE;
//                 }
//                 cout << endl;
                return sign;
            } else if (keyCompare(attribute,key,rid,searchRealDataPtr) < 0) end = mid - 1;
            else start = mid + 1;
        }

        free(curPage);
    }
    return -1;
}
RC IndexManager::deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid) {
    unsigned rootPageID;
    getRootPageID(ixFileHandle,rootPageID);


    if (deleteHelper(ixFileHandle, attribute,key,rid,rootPageID) == -1) {
        return -1;
    }

    return 0;
}

RC IndexManager::scan(IXFileHandle &ixFileHandle,
                      const Attribute &attribute,
                      const void *lowKey,
                      const void *highKey,
                      bool lowKeyInclusive,
                      bool highKeyInclusive,
                      IX_ScanIterator &ix_ScanIterator) {
    if(ixFileHandle.fileHandler.getFile() == nullptr) return -1;
    return ix_ScanIterator.initialize(ixFileHandle, attribute, lowKey,highKey,lowKeyInclusive,highKeyInclusive);
}


void IndexManager::printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const {
    IndexManager ixm = IndexManager::instance();
    PageNum rootPageID;
    ixm.getRootPageID(ixFileHandle,rootPageID);

    printHelper(ixm,ixFileHandle,attribute,rootPageID);
    cout << endl;
    cout << "}" << endl;
}

void *IndexManager::getIndexEntryKey(const Attribute &attribute, const void *page, const void *mid,
                                     unsigned &leftPagePointer) {
    unsigned keyOffset = *(unsigned *)((char *)mid + sizeof(unsigned));

    auto *returnPtr = (char *)page + keyOffset;
    leftPagePointer = *(unsigned *)(returnPtr - PTR_SIZE);
    return returnPtr;
}

void IndexManager::printHelper(IndexManager ixm, IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum curPageID) const {

    queue<PageNum> pageIDs;

    void *curPage = malloc(PAGE_SIZE);

    ixFileHandle.readIxPage(curPageID,curPage);

    pType pageType = ixm.getPageType(curPage);
    cout << "{" << endl;
    if (pageType == INDEXENTRIES) {
        pType pageType;
        unsigned freeSpace;
        unsigned numberOfKeys;
        ixm.getIndexEntriesPageInformation(curPage,pageType,freeSpace,numberOfKeys);
        if(numberOfKeys == 0) return;
        unsigned curEntryIndex = 0;

        cout << '"' << "keys" << '"' << ":" << "[";
        auto *metaDataPtr = (char *) curPage + PAGE_SIZE - TYPE_PTR - 2 * sizeof(unsigned) - RID_SIZE;
        while (curEntryIndex < numberOfKeys) {
            unsigned entryOffset = *(unsigned *) (metaDataPtr + sizeof(unsigned));

            auto *realDataPtr = (char *)curPage + entryOffset;
            /**print the data based on its attribute**/
            if (attribute.type == TypeInt) {
                int keyContent = *(int *)realDataPtr;
                if (curEntryIndex != numberOfKeys - 1) cout << '"' <<keyContent << '"' << ",";
                else cout << '"' << keyContent << '"' << "]," << endl;

            } else if (attribute.type == TypeReal) {
                float keyContent = *(float *)realDataPtr;
                if (curEntryIndex != numberOfKeys - 1) cout << '"' <<keyContent << '"' << ",";
                else cout << '"' << keyContent << '"'<< "]," << endl;

            } else {
                int varcharLength = *(int *)realDataPtr;
                char varchar[varcharLength + 1];
                varchar[varcharLength] = '\0';

                for (int i = 0; i < varcharLength; i++) varchar[i] = *(realDataPtr + sizeof(unsigned)+i);

                if (curEntryIndex != numberOfKeys - 1) {
                    cout << '"';
                    cout << varchar;
                    cout << '"' << ",";
                }
                else cout << '"' << varchar << '"'<< "]," << endl;

            }

            PageNum leftPagePtr = *(unsigned *)(realDataPtr - sizeof(unsigned));
            pageIDs.push(leftPagePtr);
            if (curEntryIndex == numberOfKeys - 1) {
                unsigned keyRidSize = 0;
                if (attribute.type == TypeInt) keyRidSize = sizeof(unsigned) + RID_SIZE;
                else if (attribute.type == TypeReal) keyRidSize = sizeof(float) + RID_SIZE;
                else {
                    unsigned varcharLength = *(unsigned *)realDataPtr;
                    keyRidSize = sizeof(unsigned) + varcharLength + RID_SIZE;
                }
                PageNum rightPagePtr = *(unsigned *)(realDataPtr + keyRidSize);
                pageIDs.push(rightPagePtr);
            }
            curEntryIndex++;
            metaDataPtr -= RID_SIZE;
        }

        
    } else {
        pType pageType;
        PageNum pagePointer;
        unsigned freeSpace;
        unsigned numberOfKeys;
        ixm.getDataEntriesPageInformation(curPage,pageType,pagePointer,freeSpace,numberOfKeys);
        if(numberOfKeys == 0) return;
        unsigned curEntryIndex = 0;

        cout << '"' << "keys" << '"'<< ":" << "[";

        vector<RID> ridWithSameKey;
        void *key = nullptr;
        void *lastKey = nullptr;
        /*RID rid;
        *
         * rid in this case doesn't really matter, since we only need to compare the previous key and the current key, which is 0 or non-0
         * *
        rid.pageNum = 999;
        rid.slotNum = 999;*/
        while (curEntryIndex < numberOfKeys) {
            auto *metaDataPtr = (byte*)ixm.moveCursorToFirstDataMetaEntries(curPage);
            metaDataPtr -= curEntryIndex * RID_SIZE;
            unsigned entryOffset = *(unsigned *) (metaDataPtr + sizeof(unsigned));

            auto *realDataPtr = (char *)curPage + entryOffset;

            if(curEntryIndex == 0){
                key = realDataPtr;
                if (attribute.type == TypeInt)
                    cout << '"' << *(int *)key << ":[";
                else if (attribute.type == TypeReal)
                    cout << '"' << *(float *)key << ":[";
                else {
                    unsigned varcharLength = *(unsigned *)realDataPtr;
                    char varchar[varcharLength + 1];
                    varchar[varcharLength] = '\0';
                    for (unsigned i = 0; i < varcharLength; i++) {
                        varchar[i] = *(realDataPtr + sizeof(unsigned) + i);
                    }
                    cout << '"';
                    cout << varchar;
                    cout << ":[";
                }
                lastKey = key;
                RID curRID;
                if (attribute.type == TypeInt){
                    curRID.pageNum = *(unsigned *)(realDataPtr + sizeof(unsigned));
                    curRID.slotNum = *(unsigned *)(realDataPtr + 2 * sizeof(unsigned));
                }
                else if (attribute.type == TypeReal){
                    curRID.pageNum = *(unsigned *)(realDataPtr + sizeof(float));
                    curRID.slotNum = *(unsigned *)(realDataPtr + sizeof(float) + sizeof(unsigned));
                }
                else{
                    int size = *(int*)realDataPtr;
                    curRID.pageNum = *(unsigned *)(realDataPtr + sizeof(int) + size);
                    curRID.slotNum = *(unsigned *)(realDataPtr + sizeof(int) + size + sizeof(unsigned));
                }
                ridWithSameKey.push_back(curRID);
                curEntryIndex ++;
                continue;
            }

            key = realDataPtr;
            bool isSame = false;
            if (attribute.type == TypeInt){
                isSame = *(int *)key == *(int *)lastKey;
            }
            else if (attribute.type == TypeReal){
                isSame = *(float *)key == *(float *)lastKey;
            }
            else{
                int varChar1Length = *(int *) key;
                char varchar1[varChar1Length + 1];
                for (int i = 0; i < varChar1Length; i++) {
                    varchar1[i] = *((char *)key + sizeof(unsigned) + i);
                }
                varchar1[varChar1Length] = '\0';

                int varChar2Length = *(int *) lastKey;
                char varchar2[varChar2Length + 1];
                for (int i = 0; i < varChar2Length; i++) {
                    varchar2[i] = *((char *)lastKey + sizeof(int) + i);
                }
                varchar2[varChar2Length] = '\0';

                if(strcmp(varchar1,varchar2) == 0) isSame = true;
                else isSame = false;
            }

            if(!isSame){
                if (!ridWithSameKey.empty()) {
                for (unsigned i = 0; i < ridWithSameKey.size(); i++) {
                    if (i == ridWithSameKey.size() - 1) {
                        cout << "(" << ridWithSameKey[i].pageNum << "," << ridWithSameKey[i].slotNum << ")";
                    } else
                        cout << "(" << ridWithSameKey[i].pageNum << "," << ridWithSameKey[i].slotNum << ")" << ",";
                    }
                    cout << "]" << '"' << ",";
                }
                ridWithSameKey.clear();
                lastKey = key;

                if (attribute.type == TypeInt)
                    cout << '"' << *(int *)key << ":[";
                else if (attribute.type == TypeReal)
                    cout << '"' << *(float *)key << ":[";
                else {
                    unsigned varcharLength = *(unsigned *)realDataPtr;
                    char varchar[varcharLength + 1];
                    varchar[varcharLength] = '\0';
                    for (unsigned i = 0; i < varcharLength; i++) {
                        varchar[i] = *(realDataPtr + sizeof(unsigned) + i);
                    }
                    cout << '"';
                    for (unsigned i = 0; i < varcharLength; i++) {
                        cout << varchar[i];
                    }
                    cout << ":[";
                }

            }

            RID curRID;
            if (attribute.type == TypeInt){
                curRID.pageNum = *(unsigned *)(realDataPtr + sizeof(unsigned));
                curRID.slotNum = *(unsigned *)(realDataPtr + 2 * sizeof(unsigned));
            }
            else if (attribute.type == TypeReal){
                curRID.pageNum = *(unsigned *)(realDataPtr + sizeof(float));
                curRID.slotNum = *(unsigned *)(realDataPtr + sizeof(float) + sizeof(unsigned));
            }
            else{
                int size = *(int*)realDataPtr;
                curRID.pageNum = *(unsigned *)(realDataPtr + sizeof(int) + size);
                curRID.slotNum = *(unsigned *)(realDataPtr + sizeof(int) + size + sizeof(unsigned));
            }
            
            ridWithSameKey.push_back(curRID);
            curEntryIndex ++;

        }
        if (!ridWithSameKey.empty()) {
            for (unsigned i = 0; i < ridWithSameKey.size(); i++) {
                if (i == ridWithSameKey.size() - 1) {
                    cout << "(" << ridWithSameKey[i].pageNum << "," << ridWithSameKey[i].slotNum << ")";
                } else
                    cout << "(" << ridWithSameKey[i].pageNum << "," << ridWithSameKey[i].slotNum << ")" << ",";
            }
            cout << "]" << '"' << "]";
        }
        ridWithSameKey.clear();
        cout << "}" << endl;
    }

    
    free(curPage);
    if(!pageIDs.empty()){
        if (pageType == INDEXENTRIES) cout << '"' << "children" << ":" << "[" << endl;
        while (!pageIDs.empty()) {
            
            PageNum nextPageID = pageIDs.front();
            pageIDs.pop();
            printHelper(ixm,ixFileHandle,attribute,nextPageID);
            if (!pageIDs.empty())cout << "," << endl;
        }
        cout << "]} " << '"' << endl;
    }
    

}

/**
 * return the last element's index that is smaller than or equal to the target value
 * -1 means we cannot find such element
 * **/
void IndexManager::dataEntryBinarySearch(void *page, const Attribute &attribute, const void *key, const RID &rid,
                                         int &index) {

    pType pageType;
    PageNum nextPointer;
    unsigned freeSpace;
    unsigned numberOfKeys;

    getDataEntriesPageInformation(page,pageType,nextPointer,freeSpace,numberOfKeys);

    auto *searchMetaDataPtr = (char *)page + PAGE_SIZE - TYPE_PTR - NXT_PTR - 2 * sizeof(unsigned) - RID_SIZE;
    int start = 0;
    int end = numberOfKeys - 1;

    while (start <= end) {
        int mid = start + (end - start) / 2;
        searchMetaDataPtr -= mid * RID_SIZE;
        //unsigned searchEntryLength = *(unsigned *)searchMetaDataPtr;
        unsigned searchEntryOffset = *(unsigned *)(searchMetaDataPtr + sizeof(unsigned));
        auto *realDataPtr = (char *)page + searchEntryOffset;
        if (keyCompare(attribute,key,rid,realDataPtr) >= 0) {
            start = mid + 1;
        } else end = mid - 1;
    }

    if (start == 0) {
        index = -1;
        return;
    }

    index = start - 1;
    return;
}


/*IX_ScanIterator::IX_ScanIterator() {
}

IX_ScanIterator::~IX_ScanIterator() {
}*/

RC IX_ScanIterator::initialize(IXFileHandle &ixFileHandle,const Attribute &attribute,
                               const void *lowKey, const void *highKey, bool lowKeyInclusive, bool highKeyInclusive){
    IX_ScanIterator::cur_pageNum = -1;
    IX_ScanIterator::cur_slotNum = -1;
    IX_ScanIterator::ixfileHandle = &ixFileHandle;
    IX_ScanIterator::ixm = &IndexManager::instance();
    IX_ScanIterator::attribute = attribute;
    IX_ScanIterator::lowKey = (void *)lowKey;
    IX_ScanIterator::highKey = (void *)highKey;
    IX_ScanIterator::lowKeyInclusive = lowKeyInclusive;
    IX_ScanIterator::highKeyInclusive = highKeyInclusive;
    IX_ScanIterator::curKey = nullptr;
    IX_ScanIterator::numberOfKeys = 0;
    IX_ScanIterator:: nextPagePointer = 0;


    //start navigating the leaf node
    unsigned rootID;
    cur_pageNum = ixm->getRootPageID(ixFileHandle,rootID);
    buffer = malloc(PAGE_SIZE);
    ixfileHandle->readIxPage(rootID,buffer);
    pType pageType = *(pType*)((byte*)buffer + PAGE_SIZE - sizeof(pType));
    RID rid;
    rid.pageNum = 0;
    rid.slotNum = 0;
    if(lowKeyInclusive){
        rid.pageNum = 0;
        rid.slotNum = 0;
    }
    else{
        rid.pageNum = UINT_MAX - 1;
        rid.slotNum = UINT_MAX - 1;
    }
    while(pageType != DATAENTRIES){
        int index;
        void* nextKey = ixm->indexBinarySearch(buffer, attribute, lowKey, rid,index);
        unsigned nextPID = 0;
        nextPID =*(PageNum*)((byte*)nextKey-sizeof(PageNum));

        if(ixm->keyCompare(attribute, lowKey, rid, nextKey) > 0){
            byte* temp = (byte*)ixm->moveCursorToFirstIndexMetaEntires(buffer);
            temp -= index * RID_SIZE;
            unsigned len = *(unsigned*)temp;

            nextPID =*(PageNum*)((byte*)nextKey+len);
        }
        else{
            nextPID =*(PageNum*)((byte*)nextKey-sizeof(PageNum));
        }

        cur_pageNum = nextPID;
        if(nextPID == 0) return -1;
        ixfileHandle->readIxPage(nextPID,buffer);
        pageType = *((byte*)buffer + PAGE_SIZE - sizeof(pType));
    }

    unsigned keyOffset;

    unsigned freeSpace;
    ixm->getDataEntriesPageInformation(buffer, pageType, nextPagePointer,freeSpace,numberOfKeys);
    while(numberOfKeys == 0){
        if(nextPagePointer == 0) return 0;
        ixfileHandle->readIxPage(nextPagePointer,buffer);
        ixm->getDataEntriesPageInformation(buffer, pageType, nextPagePointer,freeSpace,numberOfKeys);
    }
    byte* cur = (byte*)buffer;
    cur += PAGE_SIZE - sizeof(pType) - NXT_PTR - RID_SIZE*2;
    keyOffset = *(unsigned*)(cur+sizeof(unsigned));
    void* compKey = (byte*)buffer+keyOffset;
    int index = 1;
    while(index < (int)numberOfKeys &&  ixm->keyCompare(attribute, lowKey, rid, compKey) > 0){
        index++;
        cur -= RID_SIZE;
        keyOffset = *(unsigned*)(cur+sizeof(unsigned));
        compKey = (byte*)buffer+keyOffset;
    }
    if(index == (int)numberOfKeys && ixm->keyCompare(attribute, lowKey, rid, compKey) > 0){
        cur_pageNum = -1;
        cur_slotNum = -1;
        return 0; //No suitable result, return 0;
    }
    cur_slotNum = index;
    curKey = (byte *)compKey;
    return 0;
}

RC IX_ScanIterator::getNextEntry(RID &rid, void *key) {

    if(curKey == nullptr){
        return -1;
    }

    RID compRid;
    if(!highKeyInclusive){
        compRid.pageNum = 0;
        compRid.slotNum = 0;
    }
    else{
        compRid.pageNum = UINT_MAX - 1;
        compRid.slotNum = UINT_MAX - 1;
    }
    if(ixm->keyCompare(attribute, highKey, compRid, curKey) < 0) return -1;
    byte* cur;
    switch(attribute.type){
        case TypeInt:
            memcpy(key,curKey,sizeof(int));
            cur = (byte*)curKey + sizeof(int);
            rid.pageNum = *(unsigned*)cur;
            rid.slotNum = *(unsigned*)(cur+sizeof(unsigned));
            break;
        case TypeReal:
            memcpy(key,curKey,sizeof(float));
            cur = (byte*)curKey + sizeof(float);
            rid.pageNum = *(unsigned*)cur;
            rid.slotNum = *(unsigned*)(cur+sizeof(unsigned));
            break;
        case TypeVarChar:
            int vcSize = *(int*)curKey;
            memcpy(key,curKey,sizeof(int)+vcSize);
            cur = (byte*)curKey + sizeof(int)+vcSize;
            rid.pageNum = *(unsigned*)cur;
            rid.slotNum = *(unsigned*)(cur+sizeof(unsigned));
            break;
    }
    if(cur_slotNum == (int)numberOfKeys){
        if(nextPagePointer == DUMMY_ID){
            curKey = nullptr;
            return 0;
        }
        ixfileHandle->readIxPage(nextPagePointer,buffer);
        cur_pageNum = nextPagePointer;
        unsigned freeSpace;
        pType pageType;
        ixm->getDataEntriesPageInformation(buffer, pageType, nextPagePointer,freeSpace,numberOfKeys);
        while(numberOfKeys == 0){

            if(nextPagePointer == 0) {
                curKey = nullptr;
                return 0;
            }
            ixfileHandle->readIxPage(nextPagePointer,buffer);
            ixm->getDataEntriesPageInformation(buffer, pageType, nextPagePointer,freeSpace,numberOfKeys);
        }

        byte* cur = (byte*)buffer;
        cur += PAGE_SIZE - sizeof(pType) - NXT_PTR - RID_SIZE * 2;
        unsigned keyOffset = *(unsigned*)(cur+sizeof(unsigned));
        curKey = (byte*) buffer + keyOffset;
        cur_slotNum = 1;
        return 0;
    }
    cur = (byte*)buffer;
    cur_slotNum++;
    cur += PAGE_SIZE - sizeof(pType) - NXT_PTR - RID_SIZE * (cur_slotNum+1);

    unsigned keyOffset = *(unsigned*)(cur+sizeof(unsigned));
    curKey = (byte*) buffer + keyOffset;
    return 0;
}

RC IX_ScanIterator::close() {
    free(buffer);
    return 0;
}

IXFileHandle::IXFileHandle() {
    ixReadPageCounter = 0;
    ixWritePageCounter = 0;
    ixAppendPageCounter = 0;
}

IXFileHandle::~IXFileHandle() {
}


RC IXFileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {
    IXFileHandle::fileHandler.collectCounterValues(ixReadPageCounter,ixWritePageCounter,ixAppendPageCounter);
    //if(ixAppendPageCounter > 0) ixAppendPageCounter--;
    readPageCount = ixReadPageCounter;
    writePageCount = ixWritePageCounter;
    appendPageCount = ixAppendPageCounter;

    return 0;
}

RC IXFileHandle::appendIxPage(const void *data) {
    if (IXFileHandle::fileHandler.appendPage(data) == -1) return -1;
    IXFileHandle::ixAppendPageCounter++;
    return 0;
}

RC IXFileHandle::readIxPage(PageNum pageNum, void *pageData) {
    //cout << "reading" << pageNum << endl;
    if (IXFileHandle::fileHandler.readPage(pageNum, pageData) == -1) return -1;
    IXFileHandle::ixReadPageCounter++;
    return 0;
}

RC IXFileHandle::writeIxPage(PageNum pageNum, const void *pageData) {
    if (IXFileHandle::fileHandler.writePage(pageNum, pageData) == -1) return -1;
    IXFileHandle::ixWritePageCounter++;

    return 0;
}
