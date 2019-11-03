#ifndef _rm_h_
#define _rm_h_

#include <string>
#include <vector>

#include "../rbf/rbfm.h"

# define RM_EOF (-1)  // end of a scan operator

// RM_ScanIterator is an iterator to go through tuples
class RM_ScanIterator {
public:
    RM_ScanIterator() = default;

    ~RM_ScanIterator() = default;

    // "data" follows the same format as RelationManager::insertTuple()

    RC getNextTuple(RID &rid, void *data) { return rbfm_iter.getNextRecord(rid,data);};

    RC close() { return rbfm_iter.close(); };

    RBFM_ScanIterator rbfm_iter;
};

// Relation Manager
class RelationManager {
public:
    static RelationManager &instance();

    RC prepareSystemTableDescriptor(vector<Attribute> &attrs);

    RC prepareSystemColumnDescriptor(vector<Attribute> &attrs);

    RC prepareSystemTableData(void *data, unsigned tableid, std::string tableName, std::string fileName);

    RC prepareSystemColumnData(void *data, unsigned tableid, std::string columnName, unsigned columnType, unsigned columnLength, unsigned columnPos);

    RC initializeSystemTable(void *data);

    RC initializeSystemColumn(vector<Attribute> tabVec, vector<Attribute> colVec,void *data);

    RC createCatalog();

    RC deleteCatalog();

    RC encodeRecord(vector<Attribute> recordDescriptor, void *src, void *dst);

    RC initializeCatalog();

    RC destroySystemTable();

    RC createTable(const std::string &tableName, const std::vector<Attribute> &attrs);

    RC deleteTable(const std::string &tableName);

    RC getAttributes(const std::string &tableName, std::vector<Attribute> &attrs);

    RC insertTuple(const std::string &tableName, const void *data, RID &rid);

    RC deleteTuple(const std::string &tableName, const RID &rid);

    RC updateTuple(const std::string &tableName, const void *data, const RID &rid);

    RC readTuple(const std::string &tableName, const RID &rid, void *data);

    // Print a tuple that is passed to this utility method.
    // The format is the same as printRecord().
    RC printTuple(const std::vector<Attribute> &attrs, const void *data);

    RC readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName, void *data);

    // Scan returns an iterator to allow the caller to go through the results one by one.
    // Do not store entire results in the scan iterator.
    RC scan(const std::string &tableName,
            const std::string &conditionAttribute,
            const CompOp compOp,                  // comparison type such as "<" and "="
            const void *value,                    // used in the comparison
            const std::vector<std::string> &attributeNames, // a list of projected attributes
            RM_ScanIterator &rm_ScanIterator);

// Extra credit work (10 points)
    RC addAttribute(const std::string &tableName, const Attribute &attr);

    RC dropAttribute(const std::string &tableName, const std::string &attributeName);

    RC getAttrbutesLength(vector<Attribute> attrs);

    RC updateTableNumber();

protected:
    RelationManager();                                                  // Prevent construction
    ~RelationManager();                                                 // Prevent unwanted destruction
    RelationManager(const RelationManager &);                           // Prevent construction by copying
    RelationManager &operator=(const RelationManager &);                // Prevent assignment

private:
    static RelationManager *_relation_manager;
    RecordBasedFileManager* _rbfm;
    int systemTableCounter = -1;
    std::string systemTable = "Tables";
    std::string systemColumn = "Columns";
};

#endif