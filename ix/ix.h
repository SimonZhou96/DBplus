#ifndef _ix_h_
#define _ix_h_

#include <vector>
#include <string>
#include <queue>
#include <unordered_map>
#include "../rbf/rbfm.h"

# define IX_EOF (-1)  // end of the index scan

# define NXT_PTR 4
# define TYPE_PTR 1
# define PTR_SIZE (sizeof(unsigned))
# define RID_SIZE (2*sizeof(unsigned))
# define DATAENTRIES 1
# define INDEXENTRIES 0

# define DUMMY_ID 0

typedef unsigned char pType;

class IX_ScanIterator;

class IXFileHandle;

class IndexManager {

public:
    static IndexManager &instance();

    // Create an index file.
    RC createFile(const std::string &fileName);

    // Delete an index file.
    RC destroyFile(const std::string &fileName);

    // Open an index and return an ixFileHandle.
    RC openFile(const std::string &fileName, IXFileHandle &ixFileHandle);

    // Close an ixFileHandle for an index.
    RC closeFile(IXFileHandle &ixFileHandle);

    // Insert an entry into the given index that is indicated by the given ixFileHandle.
    RC insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid);

    // Delete an entry from the given index that is indicated by the given ixFileHandle.
    RC deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid);

    RC deleteHelper(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &rid, const unsigned curPageID);

    RC deleteDataEntriesKey(void *page, const void *searchMetaDataPtr, const unsigned numberOfUpdatedKey);
    // get the current page type based on its indicator
    unsigned char getPageType(const void *page);
    RC splitIndexNode(void *page, void *overflowPage, void* returnedKey,
                         const Attribute &attribute, const void *key, const PageNum overflowPageID,const PageNum PID);
    RC splitDataNode(void *page, void *overflowPage, void* returnedKey, const Attribute &attribute, const void *key, const RID &rid, const PageNum overflowPageID,const PageNum PID);


    void dataEntryBinarySearch(void *page, const Attribute &attribute, const void *key, const RID &rid, int &index);

    void* indexBinarySearch(void* page, const Attribute &attribute, const void *key, const RID &rid, int &index);

    RC getIndexEntriesPageInformation(const void* page, unsigned char &pageType, unsigned &freeSpace, unsigned &numberOfKeys);
    RC keyCompare(const Attribute &attribute, const void *key1, const RID &rid, const void *key2);

    RC insertDataEntriesKey(void *page, const Attribute &attribute, const void *key, const RID &rid);
    RC insertIndexEntriesKey(void *page, const Attribute &attribute, const void* key);

    void *getIndexEntryKey(const Attribute &attribute, const void *page, const void *mid, unsigned &leftPagePointer);

    RC getInsertingDataKeySize(const Attribute &attribute, const void *key, unsigned &keySize);

    void* moveCursorToFirstIndexMetaEntires(void *indexPage);
    void* moveCursorToFirstDataMetaEntries(void *dataPage);
    RC getDataEntriesPageInformation(void *dataPage, unsigned char &pageType, PageNum &nextPagePointer, unsigned &freeSpace, unsigned &numberOfKeys);
    RC initialIndexEntriesPage(IXFileHandle &ixFileHandle, void *indexPage, PageNum &pageID);
    RC getKeySize(const Attribute &attribute, const void *key);
    // initialize a data page to store the data entry
    RC initialDataEntriesPage(IXFileHandle &ixFileHandle, void *dataPage, PageNum &pageID);
    // return the root key
    RC getRootPageID(IXFileHandle &ixFileHandle, unsigned &rootPageID);
    // change root key the pointer points to
    RC alterRootPointer(IXFileHandle &ixFileHandle, const Attribute &attribute, const unsigned newPageID);
    // Initialize the dummy page
    RC initialDummyPage(IXFileHandle &ixFileHandle, const Attribute &attribute);
    // Auxiliary function to implement the INSERTENTRY
    void *helper(IXFileHandle &ixFileHandle, const Attribute &attribute, const void *key, const RID &ridconst, const unsigned &currentNode);

    // Initialize and IX_ScanIterator to support a range search
    RC scan(IXFileHandle &ixFileHandle,
            const Attribute &attribute,
            const void *lowKey,
            const void *highKey,
            bool lowKeyInclusive,
            bool highKeyInclusive,
            IX_ScanIterator &ix_ScanIterator);

    // Print the B+ tree in pre-order (in a JSON record format)
    void printBtree(IXFileHandle &ixFileHandle, const Attribute &attribute) const;

    void printHelper(IndexManager ixm, IXFileHandle &ixFileHandle, const Attribute &attribute, const PageNum curPageID) const;
protected:
    IndexManager() = default;                                                   // Prevent construction
    ~IndexManager() = default;                                                  // Prevent unwanted destruction
    IndexManager(const IndexManager &) = default;                               // Prevent construction by copying
    IndexManager &operator=(const IndexManager &) = default;                    // Prevent assignment
    RecordBasedFileManager* _rbfm = &RecordBasedFileManager::instance();
};

class IX_ScanIterator {
public:

    // Constructor
    IX_ScanIterator() = default;

    // Destructor
    ~IX_ScanIterator() = default;


    RC initialize(IXFileHandle &ixfileHandle,const Attribute &attribute,
    const void *lowKey, const void *highKey, bool lowKeyInclusive, bool highKeyInclusive);

    // Get next matching entry
    RC getNextEntry(RID &rid, void *key);

    // Terminate index scan
    RC close();

private:
    int cur_pageNum;
    int cur_slotNum;
    IXFileHandle *ixfileHandle;
    Attribute attribute;
    void *lowKey;
    void *highKey;
    bool lowKeyInclusive;
    bool highKeyInclusive;
    void* buffer;
    unsigned numberOfKeys;
    PageNum nextPagePointer;
    byte* curKey;
    IndexManager* ixm;
};

class IXFileHandle {
public:
    FileHandle fileHandler;
    // variables to keep counter for each operation
    unsigned ixReadPageCounter;
    unsigned ixWritePageCounter;
    unsigned ixAppendPageCounter;


    // Constructor
    IXFileHandle();

    // Destructor
    ~IXFileHandle();

    // Put the current counter values of associated PF FileHandles into variables
    RC collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount);

    // Put the current counter values of associated PF FileHandles into variables
    RC appendIxPage(const void *data);

    // read the page as rbfm operation
    RC readIxPage(PageNum pageNum, void *pageData);

    // write the page as rbfm operation
    RC writeIxPage(PageNum pageNum, const void *pageData);
};

#endif
