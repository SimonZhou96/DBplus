#include "rm.h"

RelationManager *RelationManager::_relation_manager = nullptr;

RelationManager &RelationManager::instance() {
    static RelationManager _relation_manager = RelationManager();

    return _relation_manager;
}

RelationManager::RelationManager() = default;

RelationManager::~RelationManager() { delete _relation_manager; }

RelationManager::RelationManager(const RelationManager &) = default;

RelationManager &RelationManager::operator=(const RelationManager &) = default;

RC RelationManager::createCatalog() {
    _rbfm = &RecordBasedFileManager::instance();
    // initialize catalog

    vector<Attribute> tableAttr;
    prepareSystemTableDescriptor(tableAttr);
    if(createTable(systemTable, tableAttr) != 0) return -1;

    vector<Attribute> columnAttr;
    prepareSystemColumnDescriptor(columnAttr);
    if(createTable(systemColumn, columnAttr) != 0) return -1;
    systemTableCounter = 3;
    FileHandle fileHandle;

    return 0;
}

RC RelationManager::deleteCatalog() {
    return destroySystemTable();
}

RC RelationManager::destroySystemTable() {
    RC res = _rbfm->destroyFile(systemColumn);
    if(res != 0) return -1;
    res = _rbfm->destroyFile(systemTable);
    return res;
}
/**
 * Used to create the data that can be insert into the system table
 * 1. null indicator: sizeof(unsigned char)
 * 2. table id: sizeof(unsigned)
 * 3. tablename length: sizeof(unsigned)
 * 4. tablename: strlen(tableName.c_str())
 * 5. filename length: sizeof(unsigned)
 * 6. filename: strlen(fileName.c_str())
 * **/
RC RelationManager::prepareSystemTableData(void *data, unsigned tableid, std::string tableName, std::string fileName) {

    unsigned dataSize = 0;
    dataSize += sizeof(unsigned char) + sizeof(unsigned) + sizeof(unsigned) + strlen(tableName.c_str()) + sizeof(unsigned) + strlen(fileName.c_str());

    // copy the table meta data into data
    auto * ptr = (char *)data;
    unsigned offset = 0;

    memset(ptr + offset, 0, sizeof(unsigned char));
    offset += sizeof(unsigned char);

    memcpy(ptr + offset, &tableid, sizeof(unsigned));
    offset += sizeof(unsigned);

    unsigned tableNameLength = strlen(tableName.c_str());

    memcpy(ptr + offset, &tableNameLength, sizeof(unsigned));

    offset += sizeof(unsigned);

    memcpy(ptr + offset, tableName.c_str(), tableNameLength);

    offset += tableNameLength;

    memcpy(ptr + offset, &tableNameLength, sizeof(unsigned));
    offset += sizeof(unsigned);

    memcpy(ptr + offset, tableName.c_str(), strlen(tableName.c_str()));
    offset += tableNameLength;

    //cout << "table length in function: " << *(unsigned *)((char *)ptr + 5) << endl;
    return 0;
}

/**
 * Used to create the data that can be insert into the system column
 * 1. null indicator: sizeof(unsigned char)
 * 2. table id: sizeof(unsigned)
 * 3. columnName length: sizeof(unsigned)
 * 4. columnName: strlen(tableName.c_str())
 * 5. column type: sizeof(unsigned)
 * 6. column length: sizeof(unsigned)
 * 7. column position: sizeof(unsigned)
 * **/

RC RelationManager::prepareSystemColumnData(void *data, unsigned tableid, std::string columnName,
        unsigned columnType, unsigned columnLength, unsigned columnPos){

    unsigned dataSize = 0;
    dataSize += sizeof(unsigned char) + sizeof(unsigned) + sizeof(unsigned) + strlen(columnName.c_str()) + 3 * sizeof(unsigned);
    auto *ptr = (char *)data;
    unsigned offset = 0;

    memset(ptr + offset, 0, sizeof(unsigned char));
    offset += sizeof(unsigned char);

    memcpy(ptr + offset, &tableid, sizeof(unsigned));
    offset += sizeof(unsigned);

    unsigned colunNameLength = strlen(columnName.c_str());
    memcpy(ptr + offset, &colunNameLength, sizeof(unsigned));
    offset += sizeof(unsigned);
    memcpy(ptr + offset, columnName.c_str(), colunNameLength);
    offset += colunNameLength;

    memcpy(ptr + offset, &columnType, sizeof(int));
    offset += sizeof(int);

    memcpy(ptr + offset, &columnLength, sizeof(int));
    offset += sizeof(int);

    memcpy(ptr + offset, &columnPos, sizeof(int));
    offset += sizeof(int);

    return 0;
}

/**
 * @param: attrs: used to store the data format the should be writtern to the system column
 * */
RC RelationManager::prepareSystemColumnDescriptor(vector<Attribute> &attrs) {
    Attribute attr4;
    attr4.type = TypeInt;
    attr4.name = "table-id";
    attr4.length = (AttrLength) 4;
    attrs.push_back(attr4);

    Attribute attr5;
    attr5.type = TypeVarChar;
    attr5.name = "column-name";
    attr5.length = (AttrLength) 50;
    attrs.push_back(attr5);

    Attribute attr6;
    attr6.type = TypeInt;
    attr6.name = "column-type";
    attr6.length = (AttrLength) 4;
    attrs.push_back(attr6);

    Attribute attr7;
    attr7.type = TypeInt;
    attr7.name = "column-length";
    attr7.length = (AttrLength) 4;
    attrs.push_back(attr7);

    Attribute attr8;
    attr8.type = TypeInt;
    attr8.name = "column-position";
    attr8.length = (AttrLength) 4;
    attrs.push_back(attr8);

    return 0;
}


/**
 * @param: tabVec: used to store the data format the should be writtern to the system table
 * */

RC RelationManager::prepareSystemTableDescriptor(vector<Attribute> &tabVec) {
    Attribute attr1;
    attr1.type = TypeInt;
    attr1.name = "table-id";
    attr1.length = (AttrLength) 4;

    Attribute attr2;
    attr2.type = TypeVarChar;
    attr2.name = "table-name";
    attr2.length = (AttrLength) 50;

    Attribute attr3;
    attr3.type = TypeVarChar;
    attr3.name = "file-name";
    attr3.length = (AttrLength) 50;

    tabVec.push_back(attr1);
    tabVec.push_back(attr2);
    tabVec.push_back(attr3);
    return 0;
}

/***
 * @par this function is for create the table.
 * @param tableName: the table name
 * @param attrs: the attrtibutes that the table should have
 */
RC RelationManager::createTable(const std::string &tableName, const std::vector<Attribute> &attrs) {
    
    if (strcmp(tableName.c_str(), systemTable.c_str()) == 0) {
        if (_rbfm->createFile(systemTable) != 0) {
            //perror("the system table was already been created");
            return -1;
        }
        FileHandle fileHandle;
        _rbfm->openFile(systemTable, fileHandle);

        void *data = malloc(PAGE_SIZE);
        // write system table meta data
        prepareSystemTableData(data, 1,systemTable, systemTable);
        RID rid;
        _rbfm->insertRecord(fileHandle, attrs, data, rid);
        free(data);
        data = malloc(PAGE_SIZE);
        // write system column meta data
        prepareSystemTableData(data, 2, systemColumn, systemColumn);
        _rbfm->insertRecord(fileHandle, attrs, data, rid);
        free(data);
        _rbfm->closeFile(fileHandle);

    } else if (strcmp(tableName.c_str(), systemColumn.c_str()) == 0) {

        if (_rbfm->createFile(systemColumn) != 0) {
            //perror("the system column was already been created");
            return -1;
        }
        FileHandle fileHandle;
        _rbfm->openFile(systemColumn, fileHandle);
        void *data = nullptr;

        data = malloc(PAGE_SIZE);
        // write table meta data to column table
        prepareSystemColumnData(data,1,"table-id",TypeInt,4,1);
        RID rid;
        _rbfm->insertRecord(fileHandle, attrs, data, rid);
        free(data);

        data = malloc(PAGE_SIZE);
        prepareSystemColumnData(data,1,"table-name",TypeVarChar,50,2);
        _rbfm->insertRecord(fileHandle, attrs, data, rid);
        free(data);

        data = malloc(PAGE_SIZE);
        prepareSystemColumnData(data,1,"file-name",TypeVarChar,50,3);
        _rbfm->insertRecord(fileHandle, attrs, data, rid);
        free(data);

        data = malloc(PAGE_SIZE);
        // write column meta data to column table
        prepareSystemColumnData(data,2,"table-id",TypeInt,4,1);
        _rbfm->insertRecord(fileHandle, attrs, data, rid);
        free(data);

        data = malloc(PAGE_SIZE);
        prepareSystemColumnData(data,2,"column-name",TypeVarChar,50,2);
        _rbfm->insertRecord(fileHandle, attrs, data, rid);
        free(data);

        data = malloc(PAGE_SIZE);
        prepareSystemColumnData(data,2,"column-type",TypeInt,4,3);
        _rbfm->insertRecord(fileHandle, attrs, data, rid);
        free(data);

        data = malloc(PAGE_SIZE);
        prepareSystemColumnData(data,2,"column-length",TypeInt,4,4);
        _rbfm->insertRecord(fileHandle, attrs, data, rid);
        free(data);

        data = malloc(PAGE_SIZE);
        prepareSystemColumnData(data,2,"column-position",TypeInt,4,5);
        _rbfm->insertRecord(fileHandle, attrs, data, rid);
        free(data);

        _rbfm->closeFile(fileHandle);

    } else {
        

        if (_rbfm->createFile(tableName) != 0) {
            //perror("the target table was already been created!");
            return -1;
        }


        //Check whether the table has already existed in system table or not
        RM_ScanIterator rm_iter;
        std::vector<string> conditionAttribute;
        conditionAttribute.push_back("table-id");
        byte* temp = (byte*)malloc(sizeof(int) + tableName.length());
        *(int*)temp = tableName.length();
        memcpy(temp+sizeof(int),tableName.c_str(),tableName.length());

        if(scan(systemTable,"table-name",EQ_OP,temp,conditionAttribute,rm_iter) != 0) return -1;
        byte* temp_value = (byte*)malloc(1+sizeof(int));
        RID temp_rid;
        if(rm_iter.getNextTuple(temp_rid,temp_value) != RM_EOF){
            free(temp_value);
            rm_iter.close();
            return -1;
        }
        free(temp_value);
        rm_iter.close();
        free(temp);

        if(systemTableCounter < 0){
            updateTableNumber();
            
        }
        FileHandle fileHandle;
        _rbfm->openFile(systemTable, fileHandle);

        void *data = nullptr;
        data = malloc(PAGE_SIZE);
        // other table written to the system table
        vector<Attribute> tabVec;
        prepareSystemTableDescriptor(tabVec);
//        auto * ptr = (char *)data;
        prepareSystemTableData(data,systemTableCounter,tableName,tableName);

        RID rid;
        _rbfm->insertRecord(fileHandle,tabVec, data, rid);

        free(data);
        _rbfm->closeFile(fileHandle);

        // other table written to the system column
        FileHandle columnFileHandle;
        vector<Attribute> colVec;
        prepareSystemColumnDescriptor(colVec);
        _rbfm->openFile(systemColumn, columnFileHandle);
        unsigned pos = 1;
        for (auto attr: attrs) {
            data = malloc(PAGE_SIZE);
            switch (attr.type) {
                case TypeInt:
                    prepareSystemColumnData(data,systemTableCounter,attr.name, attr.type, attr.length,pos++);
                    _rbfm->insertRecord(columnFileHandle, colVec, data, rid);
                    free(data);
                    break;
                case TypeVarChar:
                    prepareSystemColumnData(data,systemTableCounter,attr.name,attr.type,attr.length,pos++);
                    _rbfm->insertRecord(columnFileHandle, colVec, data, rid);
                    free(data);
                    break;
                case TypeReal:
                    prepareSystemColumnData(data,systemTableCounter,attr.name,attr.type,attr.length,pos++);
                    _rbfm->insertRecord(columnFileHandle, colVec, data, rid);
                    free(data);
                    break;
            }
        }
        systemTableCounter++;
        _rbfm->closeFile(columnFileHandle);

    }

    // add attribute to the system column table
    return 0;
}
/**
 * this function is used to delete the table and the table meta data in the system table and system column
 * */
RC RelationManager::deleteTable(const std::string &tableName) {
    /**
     * we cannot delete the system table from here , this is user level operation, but for debug purpose on test 10, we first let delete
     * can delete every table
     * **/
//    if (strcmp(tableName.c_str(), systemTable.c_str()) == 0) return -1;
//    if (strcmp(tableName.c_str(), systemColumn.c_str()) == 0) return -1;

    
    /**
     * 1. delete the records in the System table and System column
     * 2. delete the table file
     * **/
    if(tableName.compare(systemTable) == 0 || tableName.compare(systemColumn) == 0) return -1;
    if (_rbfm->destroyFile(tableName) != 0) return -1;
    //find the descripter from the system table first
    RM_ScanIterator rm_iter;
    std::vector<string> conditionAttribute;
    conditionAttribute.push_back("table-id");
    
    byte* temp = (byte*)malloc(sizeof(int) + tableName.length());
    *(int*)temp = tableName.length();
    memcpy(temp+sizeof(int),tableName.c_str(),tableName.length());

    if(scan(systemTable,"table-name",EQ_OP,temp,conditionAttribute,rm_iter) != 0) return -1;
    byte* data = (byte*)malloc(sizeof(int)+1);
    RID rid;
    if(rm_iter.getNextTuple(rid,data) == RM_EOF){
       rm_iter.close();
       free(data);
       return -1;
    }

    if(*(unsigned char*) data != (unsigned char)0 ){//in case the result is null
       rm_iter.close();
       free(data);
       return -1;
    }
    int t_id = *(int*)(data+1);
    rm_iter.close();
    free(temp);
    conditionAttribute.clear();
    if(deleteTuple(systemTable,rid) != 0){
        free(data);
        return -1;
    }


    std::vector<Attribute> attrs;
    prepareSystemColumnDescriptor(attrs);
    for(Attribute a:attrs) conditionAttribute.push_back(a.name);
    scan(systemColumn,"table-id",EQ_OP,&t_id,conditionAttribute,rm_iter);
    void* r_data = malloc(PAGE_SIZE);
    while(rm_iter.getNextTuple(rid,r_data) != RM_EOF){
       if(deleteTuple(systemColumn,rid) != 0){
        free(data);
        free(r_data);
        rm_iter.close();
        return -1;
       } 
    }
    free(data);
    free(r_data);
    rm_iter.close();
    conditionAttribute.clear();
    updateTableNumber();

    return 0;
}

RC RelationManager::getAttributes(const std::string &tableName, std::vector<Attribute> &attrs) {
    if(tableName.compare(systemTable) == 0) return prepareSystemTableDescriptor(attrs);
    else if(tableName.compare(systemColumn) == 0) return prepareSystemColumnDescriptor(attrs);
    else{
        //find the descripter from the system table first
        RM_ScanIterator rm_iter;
        std::vector<string> conditionAttribute;
        conditionAttribute.emplace_back("table-id");
        
        byte* temp = (byte*)malloc(sizeof(int) + tableName.length());
        *(int*)temp = tableName.length();
        memcpy(temp+sizeof(int),tableName.c_str(),tableName.length());

        if(scan(systemTable,"table-name",EQ_OP,temp,conditionAttribute,rm_iter) != 0){
            return -1;
        }
        byte* data = (byte*)malloc(sizeof(int)+1);
        RID rid;
        if(rm_iter.getNextTuple(rid,data) == RM_EOF){
            rm_iter.close();
            free(data);
            return -1;
        }
        if(*(unsigned char*) data != (unsigned char)0 ){//in case the result is null
            rm_iter.close();
            free(data);
            return -1; 
        }
        int t_id = *(int*)(data+1);
        rm_iter.close();
        free(temp);
        conditionAttribute.clear();
        std::vector<Attribute> attributesDescriptor;
        prepareSystemColumnDescriptor(attributesDescriptor);
        for(auto att:attributesDescriptor) conditionAttribute.push_back(att.name);
        if(scan(systemColumn,"table-id",EQ_OP,&t_id,conditionAttribute,rm_iter) != 0){
            free(data);
            return -1; 
        }
        void* t_data = malloc(63); //maximum possible size: 1+4 + 50 + 4 + 4 + 4 = 63


        while(rm_iter.getNextTuple(rid,t_data) != RM_EOF){
            if(*(unsigned char*) data != (unsigned char)0 ){
                rm_iter.close();
                free(data);
                free(t_data);
                return -1; 
            }
            auto* cur = (byte*)t_data;
            cur++; //pass the null indicator
            cur += sizeof(int); //ignore the table id
            Attribute a;
            int name_len = *(int*)cur;
            cur += sizeof(int);
            char* s = (char*)malloc(sizeof(char) * (name_len+1));
            memcpy(s,cur,name_len);
            s[name_len] = '\0';
            std::string name(s);
            free(s);
            cur += name_len;
            int type = *(int*)cur;
            cur+=sizeof(int);
            AttrLength len = *(AttrLength*)cur;
            a.name = name;
            a.type = (AttrType)type;
            a.length = (AttrLength)len; 
            attrs.push_back(a);
            
        }
        free(data);
        free(t_data);
        rm_iter.close();
        return 0;

    }
}

RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid) {
    FileHandle fileHandle;
    if (_rbfm->openFile(tableName, fileHandle) != 0) return -1;

    vector<Attribute> attrs;
    if(getAttributes(tableName, attrs) != 0) return -1;

    _rbfm->insertRecord(fileHandle, attrs, data, rid);
    _rbfm->closeFile(fileHandle);
    return 0;
}

RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid) {
    FileHandle fileHandle;
    if (_rbfm->openFile(tableName, fileHandle) != 0) return -1;

    vector<Attribute> attrs;
    if(getAttributes(tableName, attrs) != 0) return -1;

    _rbfm->deleteRecord(fileHandle, attrs, rid);
    _rbfm->closeFile(fileHandle);
    return 0;
}

RC RelationManager::updateTuple(const std::string &tableName, const void *data, const RID &rid) {
    FileHandle fileHandle;
    if (_rbfm->openFile(tableName, fileHandle) != 0) return -1;

    vector<Attribute> attrs;
    if(getAttributes(tableName, attrs) != 0) return -1;
    _rbfm->updateRecord(fileHandle, attrs, data, rid);
    _rbfm->closeFile(fileHandle);
    return 0;
}

RC RelationManager::readTuple(const std::string &tableName, const RID &rid, void *data) {
    FileHandle fileHandle;
    if (_rbfm->openFile(tableName, fileHandle) != 0) return -1;

    vector<Attribute> attrs;
    if(getAttributes(tableName, attrs) != 0) return -1;

    RC res = _rbfm->readRecord(fileHandle, attrs, rid, data);
    _rbfm->closeFile(fileHandle);
    return res;
}

RC RelationManager::printTuple(const std::vector<Attribute> &attrs, const void *data) {
    _rbfm->printRecord(attrs, data);
    return 0;

    
}

RC RelationManager::readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName,
                                  void *data) {
    std::vector<Attribute> attrs;
    if(getAttributes(tableName, attrs) != 0) return -1;
    FileHandle fileHandle;
    if(_rbfm->openFile(tableName,fileHandle) != 0) return -1;
    RC res = _rbfm->readAttribute(fileHandle,attrs,rid,attributeName,data);
    _rbfm->closeFile(fileHandle);
    return res;
}

RC RelationManager::scan(const std::string &tableName,
                         const std::string &conditionAttribute,
                         const CompOp compOp,
                         const void *value,
                         const std::vector<std::string> &attributeNames,
                         RM_ScanIterator &rm_ScanIterator) {
    FileHandle fileHandle;
    std::vector<Attribute> descriptor;

    if(tableName.compare(systemTable) == 0){
        prepareSystemTableDescriptor(descriptor);
    }
    else if (tableName.compare(systemColumn) == 0){
        prepareSystemColumnDescriptor(descriptor);
    }
    else{
        if(getAttributes(tableName, descriptor) != 0) return -1;
    }
    if(_rbfm->openFile(tableName,fileHandle) != 0) return -1;


    return _rbfm->scan(fileHandle,descriptor,conditionAttribute,compOp,value,attributeNames,rm_ScanIterator.rbfm_iter);
}


// Extra credit work
RC RelationManager::dropAttribute(const std::string &tableName, const std::string &attributeName) {
    return -1;
}

// Extra credit work
RC RelationManager::addAttribute(const std::string &tableName, const Attribute &attr) {
    return -1;
}

RC RelationManager::updateTableNumber(){
    RM_ScanIterator rm_iter;
    std::vector<string> conditionAttribute;
    conditionAttribute.push_back("table-id");
    
    void* temp = nullptr;

    if(scan(systemTable,"table-name",NO_OP,temp,conditionAttribute,rm_iter) != 0 ) return -1;
    byte* data = (byte*)malloc(1+sizeof(int));
    RID rid;
    int next = 0;
    while(rm_iter.getNextTuple(rid,data) != RM_EOF){
        int temp = *(int*)(data+1);
        if(next < temp) next = temp;
    }
    systemTableCounter = next + 1;
    rm_iter.close();
    free(data);
    return 0;
}


