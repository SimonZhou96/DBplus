#ifndef _qe_h_
#define _qe_h_

#include "../rbf/rbfm.h"
#include "../rm/rm.h"
#include "../ix/ix.h"

#include <map>

#include <unordered_set>

#define QE_EOF (-1)  // end of the index scan

typedef enum {
    MIN = 0, MAX, COUNT, SUM, AVG
} AggregateOp;

// The following functions use the following
// format for the passed data.
//    For INT and REAL: use 4 bytes
//    For VARCHAR: use 4 bytes for the length followed by the characters

struct Value {
    AttrType type;          // type of value
    void *data;             // value
};

struct Condition {
    std::string lhsAttr;        // left-hand side attribute
    CompOp op;                  // comparison operator
    bool bRhsIsAttr;            // TRUE if right-hand side is an attribute and not a value; FALSE, otherwise.
    std::string rhsAttr;        // right-hand side attribute if bRhsIsAttr = TRUE
    Value rhsValue;             // right-hand side value if bRhsIsAttr = FALSE
};

class Iterator {
    // All the relational operators and access methods are iterators.
public:
    virtual RC getNextTuple(void *data) = 0;

    virtual void getAttributes(std::vector<Attribute> &attrs) const = 0;

    virtual ~Iterator() = default;
};

class TableScan : public Iterator {
    // A wrapper inheriting Iterator over RM_ScanIterator
public:
    RelationManager &rm;
    RM_ScanIterator *iter;
    std::string tableName;
    std::vector<Attribute> attrs;
    std::vector<std::string> attrNames;
    RID rid{};

    TableScan(RelationManager &rm, const std::string &tableName, const char *alias = NULL) : rm(rm) {
        //Set members
        this->tableName = tableName;

        // Get Attributes from RM
        rm.getAttributes(tableName, attrs);

        // Get Attribute Names from RM
        for (Attribute &attr : attrs) {
            // convert to char *
            attrNames.push_back(attr.name);
        }

        // Call RM scan to get an iterator
        iter = new RM_ScanIterator();
        rm.scan(tableName, "", NO_OP, NULL, attrNames, *iter);

        // Set alias
        if (alias) this->tableName = alias;
    };

    // Start a new iterator given the new compOp and value
    void setIterator() {
        iter->close();
        delete iter;
        iter = new RM_ScanIterator();
        rm.scan(tableName, "", NO_OP, NULL, attrNames, *iter);
    };

    RC getNextTuple(void *data) override {
        return iter->getNextTuple(rid, data);
    };

    void getAttributes(std::vector<Attribute> &attributes) const override {
        attributes.clear();
        attributes = this->attrs;

        // For attribute in std::vector<Attribute>, name it as rel.attr
        for (Attribute &attribute : attributes) {
            std::string tmp = tableName;
            tmp += ".";
            tmp += attribute.name;
            attribute.name = tmp;
        }
    };

    ~TableScan() override {
        iter->close();
    };
};

class IndexScan : public Iterator {
    // A wrapper inheriting Iterator over IX_IndexScan
public:
    RelationManager &rm;
    RM_IndexScanIterator *iter;
    std::string tableName;
    std::string attrName;
    std::vector<Attribute> attrs;
    char key[PAGE_SIZE]{};
    RID rid{};

    IndexScan(RelationManager &rm, const std::string &tableName, const std::string &attrName, const char *alias = NULL)
            : rm(rm) {
        // Set members
        this->tableName = tableName;
        this->attrName = attrName;


        // Get Attributes from RM
        rm.getAttributes(tableName, attrs);

        // Call rm indexScan to get iterator
        iter = new RM_IndexScanIterator();
        rm.indexScan(tableName, attrName, NULL, NULL, true, true, *iter);

        // Set alias
        if (alias) this->tableName = alias;
    };

    // Start a new iterator given the new key range
    void setIterator(void *lowKey, void *highKey, bool lowKeyInclusive, bool highKeyInclusive) {
        iter->close();
        delete iter;
        iter = new RM_IndexScanIterator();
        rm.indexScan(tableName, attrName, lowKey, highKey, lowKeyInclusive, highKeyInclusive, *iter);
    };

    RC getNextTuple(void *data) override {
        int rc = iter->getNextEntry(rid, key);
        if (rc == 0) {
            rc = rm.readTuple(tableName, rid, data);
        }
        return rc;
    };

    void getAttributes(std::vector<Attribute> &attributes) const override {
        attributes.clear();
        attributes = this->attrs;


        // For attribute in std::vector<Attribute>, name it as rel.attr
        for (Attribute &attribute : attributes) {
            std::string tmp = tableName;
            tmp += ".";
            tmp += attribute.name;
            attribute.name = tmp;
        }
    };

    ~IndexScan() override {
        iter->close();
    };
};

class Filter : public Iterator {
    // Filter operator
public:
    Filter(Iterator *input,               // Iterator of input R
           const Condition &condition     // Selection condition
    );

    ~Filter() override = default;

    RC getNextTuple(void *data) override;

    // For attribute in std::vector<Attribute>, name it as rel.attr
    void getAttributes(std::vector<Attribute> &attrs) const override;

private:
    Iterator *input;
    Condition condition;
    std::vector<Attribute> attrs;
    Attribute leftAttr;
    int index;
    RecordBasedFileManager* _rbfm;

};

class Project : public Iterator {
    // Projection operator
public:
    Project(Iterator *input,                    // Iterator of input R
            const std::vector<std::string> &attrNames) {
        this->ts = input;
        this->attrNames = attrNames;
        _rm = &RelationManager::instance();
        _rbfm = &RecordBasedFileManager::instance();


    };   // std::vector containing attribute names
    ~Project() override = default;

    RC getColumnIndexAndType(vector<Attribute> wholeAttr, std::string attr, int &index, int &type) {
        for(int i = 0;i<(int)wholeAttr.size();i++){
            if(wholeAttr[i].name.compare(attr) == 0){
                index = i;
                type = wholeAttr[i].type;
                return 0;
            }
        }
        return -1;
    }

    RC getNextTuple(void *data) override {
        void *wholeData = malloc(PAGE_SIZE);
        vector<Attribute> wholeAttrs;
        std::string tableName;

        /**get the actual whole data from the scan**/
        if (ts->getNextTuple(wholeData) == EOF) {
            free(wholeData);
            return -1;
        }

        void *returnData = malloc(PAGE_SIZE);

        ts->getAttributes(wholeAttrs);
        _rbfm->encodeData(wholeData,returnData,wholeAttrs);

        //short numberOfAttributes = *(short *)returnData;
        /**get the mapping relationship between column and its offset, type**/
        unsigned numberOfNullIndicators = (int)ceil((double)wholeAttrs.size() / CHAR_BIT);

        int currentBit = numberOfNullIndicators * CHAR_BIT - 1;
        auto *nullIndicator = (unsigned char *)malloc(numberOfNullIndicators);

        memset(nullIndicator, 0, numberOfNullIndicators);

        int realDataOffset = numberOfNullIndicators;
        /**we should follow the order of the attributes strictly**/
        for (auto &attr: attrNames) {
            int index;
            int columnType;
            if (getColumnIndexAndType(wholeAttrs,attr,index, columnType) == -1) return -1;
            short offset = *(short*)((char *)returnData + sizeof(short) + sizeof(short) * index);
            if (offset == -1) {
                nullIndicator[currentBit / CHAR_BIT] ^= 1UL << (currentBit);
            } else {
                /**copy the real data**/
                switch (columnType) {
                    case TypeInt:
                        memcpy((char *)data + realDataOffset, (char *)returnData + offset, sizeof(int));
                        realDataOffset += sizeof(int);
                        break;
                    case TypeReal:
                        memcpy((char *)data + realDataOffset, (char *)returnData + offset, sizeof(float));
                        realDataOffset += sizeof(float);
                        break;
                    case TypeVarChar:
                        unsigned varCharLength = *(unsigned *)((char *)returnData + offset);
                        memcpy((char *)data + realDataOffset, (char *)returnData + offset, sizeof(unsigned) + varCharLength);
                        realDataOffset += sizeof(unsigned) + varCharLength;
                        break;
                }
            }
            currentBit--;
        }

        memcpy((char *)data, nullIndicator, numberOfNullIndicators);
        free(wholeData);
        return 0;
    }
    // For attribute in std::vector<Attribute>, name it as rel.attr
    void getAttributes(std::vector<Attribute> &attrs) const override {
        vector<Attribute> parentAttrs;
        ts->getAttributes(parentAttrs);

        unordered_set<string> set;
        for (auto &attr: attrNames) set.insert(attr);

        for (auto &attr: parentAttrs)
            if (set.find(attr.name) != set.end())
                attrs.push_back(attr);
    };

private:
    Iterator *ts;
    std::vector<std::string> attrNames;
    RelationManager* _rm;
    RecordBasedFileManager* _rbfm;
};


class BNLJoin : public Iterator {
    // Block nested-loop join operator
public:
    BNLJoin(Iterator *leftIn,            // Iterator of input R
            TableScan *rightIn,           // TableScan Iterator of input S
            const Condition &condition,   // Join condition
            const unsigned numPages       // # of pages that can be loaded into memory,
            //   i.e., memory block size (decided by the optimizer)
    );

    ~BNLJoin() override;

    RC getNextTuple(void *data) override;

    // For attribute in std::vector<Attribute>, name it as rel.attr
    void getAttributes(std::vector<Attribute> &attrs) const override {
        for(Attribute attr: left_attrs) attrs.push_back(attr);
        for(Attribute attr: right_attrs) attrs.push_back(attr);
    };


private:
    void* input_buffer;
    void* buffers;
    RecordBasedFileManager* _rbfm;
    std::vector<Attribute> left_attrs;
    std::vector<Attribute> right_attrs;
    int left_index;
    int right_index;
    unsigned numPages;
    Iterator *leftIn;
    TableScan *rightIn;
    int offset;
    std::map<string, vector<void*>> hash_map;  //use vector to avoid duplicate keys
    Condition condition;
    std::vector<void*> *cur_data_list;
    int list_index;

    void loadOuterPages();
    void loadInMap(void* data, void* ptr);
    string getKey(void* data, int index, std::vector<Attribute> attrs);
    void formatReturnValue(void* data);
};

class INLJoin : public Iterator {
    // Index nested-loop join operator
public:
    INLJoin(Iterator *leftIn,           // Iterator of input R
            IndexScan *rightIn,          // IndexScan Iterator of input S
            const Condition &condition   // Join condition
    ) {
        this->leftIn = leftIn;
        this->rightIn = rightIn;
        this->condition = condition;
        this->_rbfm = &RecordBasedFileManager::instance();

        vector<Attribute> leftDataAttrs;
        this->leftIn->getAttributes(leftDataAttrs);
        for (int i = 0; i < (int)leftDataAttrs.size(); i++) {
            if (leftDataAttrs[i].name.compare(condition.lhsAttr) == 0) {
                this->leftAttrIndex = i;
                this->attrType = leftDataAttrs[i].type;
                break;
            }
        }

        if (condition.bRhsIsAttr) {
            vector<Attribute> rightDataAttrs;
            this->rightIn->getAttributes(rightDataAttrs);
            for (int i = 0; i < (int)rightDataAttrs.size(); i++) {
                if (rightDataAttrs[i].name.compare(condition.rhsAttr) == 0) {
                    this->rightAttrIndex = i;
                    break;
                }
            }
        }

    };

    RC mergeData(void *leftEncodeData, void *rightEncodeData,
                 vector<Attribute> leftAttrs, vector<Attribute> rightAttrs, void *mergedData);
    ~INLJoin() override = default;

    RC getNextTuple(void *data) override;

    // For attribute in std::vector<Attribute>, name it as rel.attr
    void getAttributes(std::vector<Attribute> &attrs) const override {
        vector<Attribute> leftAttrs;
        vector<Attribute> rightAttrs;
        this->leftIn->getAttributes(leftAttrs);
        this->rightIn->getAttributes(rightAttrs);

        for (auto &attr: leftAttrs) attrs.push_back(attr);
        for (auto &attr: rightAttrs) attrs.push_back(attr);
    };
private:
    void *curLeftData = nullptr;
    Iterator *leftIn;
    IndexScan *rightIn;
    Condition condition;
    RecordBasedFileManager *_rbfm;
    int leftAttrIndex;
    int rightAttrIndex;
    AttrType attrType;
};
// Optional for everyone. 10 extra-credit points
class GHJoin : public Iterator {
    // Grace hash join operator
public:
    GHJoin(Iterator *leftIn,               // Iterator of input R
           Iterator *rightIn,               // Iterator of input S
           const Condition &condition,      // Join condition (CompOp is always EQ)
           const unsigned numPartitions     // # of partitions for each relation (decided by the optimizer)
    );

    ~GHJoin() override;

    RC getNextTuple(void *data) override;

    // For attribute in std::vector<Attribute>, name it as rel.attr
    void getAttributes(std::vector<Attribute> &attrs) const override {
        for(Attribute attr: left_attrs) attrs.push_back(attr);
        for(Attribute attr: right_attrs) attrs.push_back(attr);
    };

private:
    unsigned numPartitions;
    int left_index;
    int right_index;
    int list_index;
    RBFM_ScanIterator* left_iter;
    RBFM_ScanIterator* right_iter;
    void* input_buffer;
    int index;
    bool reverse;

    std::vector<FileHandle> left_fileHandlers;
    std::vector<FileHandle> right_fileHandlers;

    std::vector<string> left_temp_names;
    std::vector<string> right_temp_names;

    RecordBasedFileManager* _rbfm;

    std::vector<Attribute> left_attrs;
    std::vector<Attribute> right_attrs;

    std::vector<string> left_attrs_names;
    std::vector<string> right_attrs_names;

    

    std::map<string, std::vector<RID>> hash_map;
    std::vector<RID> *cur_data_list;


    void initialize(Iterator *leftIn, Iterator *rightIn,const Condition &condition);
    void formatReturnValue(void* left_data, void* right_data, void* data);
    RC loadOuterPages();
    string getKey(void* data, int index, std::vector<Attribute> attrs);

    
};

class Aggregate : public Iterator {
    // Aggregation operator
public:
    // Mandatory
    // Basic aggregation
    Aggregate(Iterator *input,          // Iterator of input R
              const Attribute &aggAttr,        // The attribute over which we are computing an aggregate
              AggregateOp op            // Aggregate operation
    );

    // Optional for everyone: 5 extra-credit points
    // Group-based hash aggregation
    Aggregate(Iterator *input,             // Iterator of input R
              const Attribute &aggAttr,           // The attribute over which we are computing an aggregate
              const Attribute &groupAttr,         // The attribute over which we are grouping the tuples
              AggregateOp op              // Aggregate operation
    );

    ~Aggregate() override = default;

    RC getNextTuple(void *data) override;

    // Please name the output attribute as aggregateOp(aggAttr)
    // E.g. Relation=rel, attribute=attr, aggregateOp=MAX
    // output attrname = "MAX(rel.attr)"
    void getAttributes(std::vector<Attribute> &attrs) const override;

private:
    RecordBasedFileManager* _rbfm;
    string agg_attr_name;
    Attribute aggAttr;
    Attribute groupAttr;
    AggregateOp op;
    Iterator *input;

    int agg_index;
    int group_index;

    float noGroupValue;

    bool isGroup;
    bool isFirst;
    bool isDone;

    std::vector<Attribute> originAttrs;

    std::map<string, float> hash_map;
    std::map<string, int> count_map;

    std::map<string,float>::iterator it;

    RC getNextNoGroup(void *data);
    void valueAggregate(float value, float &aggValue);
    RC getNextWithGroup();
    string getKey(void* data, Attribute attr);

};

#endif
