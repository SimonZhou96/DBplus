//
// Created by MENG ZHOU on 10/23/19.
//
#include "rbfm.h"
#include "test_util.h"
int main() {
    RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();
    RC rc;
    std::string fileName = "test_delete";

    rc = rbfm.createFile(fileName);
    assert(rc == 0 && "Creating the file should not fail.");

    rc = createFileShouldSucceed(fileName);
    assert(rc == success && "Creating the file should not fail.");
    RID rid;
    // Open the file
    FileHandle fileHandle;
    rc = rbfm.openFile(fileName, fileHandle);

    std::vector<Attribute> recordDescriptor;
    createRecordDescriptor(recordDescriptor);
    assert(rc == success && "Opening the file should not fail.");

    // Initialize a NULL field indicator
    int nullFieldsIndicatorActualSize = getActualByteForNullsIndicator(recordDescriptor.size());
    auto *nullsIndicator = (unsigned char *) malloc(nullFieldsIndicatorActualSize);
    memset(nullsIndicator, 0, nullFieldsIndicatorActualSize);

    for (int i = 0; i < 5000; i ++) {
        int recordSize = 0;
        void *data = malloc(PAGE_SIZE);
        // Insert a record into a file and print the record
        prepareRecord(recordDescriptor.size(), nullsIndicator, 8, "Testcase", 25, 177.8, 6200, data,
                      &recordSize);
        rbfm.insertRecord(fileHandle, recordDescriptor, data, rid);
        void *readData = malloc(PAGE_SIZE);
        rbfm.readRecord(fileHandle, recordDescriptor, rid, readData);
        if (memcmp(readData,data, recordSize) != 0) perror("the insert and read failed!!");
    }

    cout << "test finished successfully!!" << endl;
    return 0;

}