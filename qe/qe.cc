
#include "qe.h"

Filter::Filter(Iterator *input, const Condition &condition) {
	Filter::input = input;
	Filter::condition = condition;
	input->getAttributes(attrs);
	index = -1;
	for(int i = 0;i<(int)attrs.size();i++){
		if(attrs[i].name.compare(Filter::condition.lhsAttr) == 0){
			index = i;
			break;
		}
	}
	_rbfm = &RecordBasedFileManager::instance();
}

RC Filter::getNextTuple(void *data) {
	if(index == -1) return -1;
	
	byte* encodedData = (byte*)malloc(PAGE_SIZE);
	while(input->getNextTuple(data) != QE_EOF){
		_rbfm->encodeData(data,encodedData,attrs);
		short offset = *(short*)(encodedData + sizeof(short) + sizeof(short) * index);
		void* realData = nullptr;
		if(offset != -1) realData = encodedData + offset;
		switch(condition.op){
			case NO_OP:
				free(encodedData);
				return 0;
				break;
			case EQ_OP:
				if (offset != -1){
					if(_rbfm->keyCompare(realData,condition.rhsValue.data,condition.rhsValue.type) == 0){
						free(encodedData);
						return 0;
					}
				}
				break;
			case LT_OP:
				if (offset != -1){
					if(_rbfm->keyCompare(realData,condition.rhsValue.data,condition.rhsValue.type) < 0){
						free(encodedData);
						return 0;
					}
				}
				break;
			case LE_OP:
				if (offset != -1){
					if(_rbfm->keyCompare(realData,condition.rhsValue.data,condition.rhsValue.type) <= 0){
						free(encodedData);
						return 0;
					}
				}
				break;
			case GT_OP:
				if (offset != -1){
					if(_rbfm->keyCompare(realData,condition.rhsValue.data,condition.rhsValue.type) > 0){
						free(encodedData);
						return 0;
					}
				}
				break;
			case GE_OP:
				if (offset != -1){
					if(_rbfm->keyCompare(realData,condition.rhsValue.data,condition.rhsValue.type) >= 0){
						free(encodedData);
						return 0;
					}
				}
				break;
			case NE_OP:
				if (offset != -1){
					if(_rbfm->keyCompare(realData,condition.rhsValue.data,condition.rhsValue.type) != 0){
						free(encodedData);
						return 0;
					}
				}
				break;
		}

	}
	free(encodedData);
	return -1;
}

void Filter::getAttributes(std::vector<Attribute> &attrs) const {
	input->getAttributes(attrs);
}


BNLJoin:: BNLJoin(Iterator *leftIn,            // Iterator of input R
            TableScan *rightIn,           // TableScan Iterator of input S
            const Condition &condition,   // Join condition
            const unsigned numPages       // # of pages that can be loaded into memory,
            //   i.e., memory block size (decided by the optimizer)
    ){
	BNLJoin::input_buffer = malloc(PAGE_SIZE);
	BNLJoin::buffers = malloc(numPages * PAGE_SIZE);
	BNLJoin::_rbfm = &RecordBasedFileManager::instance();
	leftIn->getAttributes(BNLJoin::left_attrs);
	rightIn->getAttributes(BNLJoin::right_attrs);
	BNLJoin::condition = condition;
	BNLJoin::numPages = numPages;
	left_index = -1;
	for(int i = 0;i<(int)left_attrs.size();i++){
		if(left_attrs[i].name.compare(BNLJoin::condition.lhsAttr) == 0){
			left_index = i;
			break;
		}
	}
	right_index = -1;
	for(int i = 0;i<(int)right_attrs.size();i++){
		if(right_attrs[i].name.compare(BNLJoin::condition.rhsAttr) == 0){
			right_index = i;
			break;
		}
	}
	cur_data_list = nullptr;
	list_index = -1;

	BNLJoin:: leftIn = leftIn;
	BNLJoin:: rightIn = rightIn;
	offset = 0;
	loadOuterPages();
}

BNLJoin::~BNLJoin(){
	free(input_buffer);
	free(buffers);
}

/**load the page from left Iterator to the memory buffer, stop when the memory buffer is full
 * Add the current key of data
 * **/
void BNLJoin::loadOuterPages(){
	hash_map.clear();
	if(left_index == -1 || right_index == -1) return;
	memset(buffers,0,numPages * PAGE_SIZE);
	void* temp = malloc(PAGE_SIZE);
	offset = 0;
	byte* cur = (byte*)buffers;
	bool isFull = false;
	while(!isFull){
		if(leftIn->getNextTuple(temp) == QE_EOF){
			isFull = true;
			break;
		}
		short len = _rbfm->recordLength(left_attrs,temp) + (int)ceil((double)(left_attrs.size()) / CHAR_BIT) - (left_attrs.size() + 1 ) * sizeof(short);
		if(offset + len > (int)numPages * PAGE_SIZE){
			isFull = true;
			break;
		}
		offset+=len;
		memcpy(cur,temp,len);
		loadInMap(temp,cur);
		cur+=len;
		
	}

	free(temp);
}
/**
 * Map formula:
 * key + void * ---> key is the data in string format based on specific attribute, void * is the list of pointer directed to the pointer in record
 * **/
void BNLJoin::loadInMap(void* data, void* ptr){
	string key = getKey(data,left_index,left_attrs);
	if(key.compare("") == 0) return;
	if(hash_map.count(key) == 0){
		std::vector<void*> data_list;
		data_list.push_back(ptr);
		hash_map[key] = data_list;
	}
	else hash_map[key].push_back(ptr);
	
}

/**get specific data based on its attribute, index indicated the order of the attributes**/
string BNLJoin::getKey(void* data, int index, std::vector<Attribute> attrs){
	string key = "";
	byte* encodedData = (byte*)malloc(PAGE_SIZE);
	_rbfm->encodeData(data,encodedData,attrs);
	short pos = *(short*)(encodedData + sizeof(short) + sizeof(short) * index);
	if(pos == -1){  //The key data is null. Ignore this tuple
		free(encodedData);
		return key;
	}
	void* realData = encodedData + pos;

	switch(attrs[index].type){
		case TypeInt:
			key =  std::to_string(*(int*)realData);
			break;
		case TypeReal:
			key =  std::to_string(*(float*)realData);
			break;
		case TypeVarChar:
			int s_len = *(int*)realData;
			for(int i = 0;i<s_len;i++){
				key += *(char*)((byte*)realData+sizeof(int) + i);
			}
	}
	free(encodedData);
	return key;
}

RC BNLJoin::getNextTuple(void *data){
	if(list_index != -1){   //Used to handle duplicate key situation
		if(list_index >= int(cur_data_list->size())){
			list_index = -1;
			cur_data_list = nullptr;
		}
		else{
			formatReturnValue(data);
			list_index++;
			return 0;
		}
	}
	while(rightIn->getNextTuple(input_buffer) != QE_EOF){
		string key = getKey(input_buffer,right_index,right_attrs);
		if(hash_map.count(key) != 0){
			cur_data_list = &hash_map[key];
			list_index = 0;
			formatReturnValue(data);
			list_index++;
			return 0;
		}
	}

	offset = 0;
	loadOuterPages();
	if(offset == 0) return -1;
	rightIn->setIterator();
	return getNextTuple(data);
	
}

void BNLJoin::formatReturnValue(void* data){
	byte* left_ptr = (byte*)(cur_data_list->at(list_index));
	byte* right_ptr = (byte*)input_buffer;
	byte* ptr = (byte*)data;

	int nullBitSize = (int)ceil((double)(left_attrs.size() + right_attrs.size()) / CHAR_BIT);
	memset(data,0,nullBitSize);
	int leftNullBitSize = (int)ceil((double)left_attrs.size() / CHAR_BIT);
	int rightNullBitSize = (int)ceil((double)right_attrs.size() / CHAR_BIT);

	memcpy(data,left_ptr,leftNullBitSize);
	int pos = left_attrs.size();
	byte* nullBitIndicator;
    nullBitIndicator = (byte*) data;

    byte* right_nullBitIndicator;
    right_nullBitIndicator = (byte*) input_buffer;

    for(int i = 0;i<rightNullBitSize;i++){
    	unsigned char right_compareBit = *(right_nullBitIndicator + ((int)floor((double)i / CHAR_BIT)));
    	if ((right_compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - i % CHAR_BIT))){
    		unsigned char compareBit = *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT)));
            compareBit = compareBit ^ ((unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT));
            *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT))) = compareBit;
    	}
    	pos++;
    }

    short left_len = _rbfm->recordLength(left_attrs,left_ptr) - (left_attrs.size() + 1 ) * sizeof(short);
    short right_len = _rbfm->recordLength(right_attrs,right_ptr) - (right_attrs.size() + 1) * sizeof(short);
    ptr += nullBitSize;
    left_ptr += leftNullBitSize;
    right_ptr += rightNullBitSize;

    memcpy(ptr,left_ptr,left_len);
    ptr += left_len;
    memcpy(ptr,right_ptr,right_len);
}

Aggregate::Aggregate(Iterator *input, const Attribute &aggAttr, AggregateOp op){
	isGroup = false;
	isFirst = true;
	isDone = false;

	_rbfm = &RecordBasedFileManager::instance();
	Aggregate::aggAttr = aggAttr;
	Aggregate:: op = op;
	Aggregate:: input = input;
	Aggregate::noGroupValue = 0;

	input->getAttributes(originAttrs);
	agg_index = -1;
	for(int i = 0;i < (int) originAttrs.size();i++){
		if(originAttrs[i].name.compare(aggAttr.name) == 0){
			agg_index = i;
			break;
		}
	}
}

Aggregate::Aggregate(Iterator *input, const Attribute &aggAttr, const Attribute &groupAttr,AggregateOp op){
	isGroup = true;
	isDone = false;

	_rbfm = &RecordBasedFileManager::instance();
	Aggregate::aggAttr = aggAttr;
	Aggregate::groupAttr = groupAttr;
	Aggregate:: op = op;
	Aggregate:: input = input;

	input->getAttributes(originAttrs);
	agg_index = -1;
	group_index = -1;
	for(int i = 0;i < (int) originAttrs.size();i++){
		if(originAttrs[i].name.compare(aggAttr.name) == 0){
			agg_index = i;
		}
		if(originAttrs[i].name.compare(groupAttr.name) == 0){
			group_index = i;
		}
	}
}


RC Aggregate::getNextTuple(void *data){
	if(!isGroup){
		if(isDone) return -1;
		return getNextNoGroup(data);
	}
	else{
		if(!isDone) getNextWithGroup();
		if(it == hash_map.end()) return -1;
		float value = it->second;
		string grp = it->first;
		if(op == AVG) value = value / count_map[grp];
		memset(data,0,1);
		byte* ptr = (byte*)data + 1;
		if(groupAttr.type == TypeInt){
			int grp_int = 0;
			grp_int = stoi(grp);
			memcpy(ptr, &grp_int,sizeof(int));
			ptr += sizeof(int);
		}
		else if(groupAttr.type == TypeReal){
			float grp_float = 0;
			grp_float = stof(grp);
			memcpy(ptr, &grp_float,sizeof(float));
			ptr += sizeof(float);
		}
		else{
			int size = grp.length();
			memcpy(ptr, &size,sizeof(int));
			ptr += sizeof(int);
			memcpy(ptr, grp.c_str(),size);
			ptr += size;
		}

		memcpy(ptr, &value,sizeof(float));
		it++;
		return 0;
	}
}

void Aggregate::getAttributes(std::vector<Attribute> &attrs) const{
	string name;
	switch(op){
		case MIN:
			name = "MIN";
			break;
		case MAX:
			name = "MAX";
			break;
		case COUNT:
			name = "COUNT";
			break;
		case SUM:
			name = "SUM";
			break;
		case AVG:
			name = "AVG";
			break;
	}
	if(isGroup){
		attrs.push_back(groupAttr);

	}

	name = name + "(" + aggAttr.name + ")";
	Attribute attr;
	attr.name = name;
	attr.type = TypeReal;
	attr.length = 4;
	attrs.push_back(attr);
	
}


RC Aggregate::getNextNoGroup(void *data){
	if(agg_index == -1) return -1;

	void* readData = malloc(PAGE_SIZE);
	byte* encodedData = (byte*)malloc(PAGE_SIZE);

	int count = 0;

	while(input->getNextTuple(readData) != QE_EOF){
		_rbfm->encodeData(readData,encodedData,originAttrs);
		short offset = *(short*)(encodedData + sizeof(short) + sizeof(short) * agg_index);
		if(offset == -1) continue;
		byte* ptr = (byte*)encodedData + offset;
		if(aggAttr.type == TypeInt){
			int value = *(int*)ptr;
			if(isFirst && op != COUNT){
				noGroupValue = (float)value;
				isFirst = false;
			}
			else valueAggregate((float)value,noGroupValue);
		}
		else if(aggAttr.type == TypeReal){
			float value = *(float*)ptr;
			if(isFirst && op != COUNT){
				noGroupValue = value;
				isFirst = false;
			}
			else valueAggregate(value,noGroupValue);
		}
		count++;
	}
	if(op == AVG) noGroupValue = noGroupValue / count;
	memset(data,0,1);
	memcpy((byte*)data + 1, &noGroupValue,sizeof(float));
	isDone = true;
	free(readData);
	free(encodedData);
	return 0;

}

RC Aggregate::getNextWithGroup(){
	if(agg_index == -1 || group_index == -1) return -1;

	void* readData = malloc(PAGE_SIZE);
	byte* encodedData = (byte*)malloc(PAGE_SIZE);


	while(input->getNextTuple(readData) != QE_EOF){
		_rbfm->encodeData(readData,encodedData,originAttrs);
		short offset = *(short*)(encodedData + sizeof(short) + sizeof(short) * agg_index);
		if(offset == -1) continue;
		byte* ptr = (byte*)encodedData + offset;

		short group_offset = *(short*)(encodedData + sizeof(short) + sizeof(short) * group_index);
		if(group_offset == -1) continue;
		string grp = getKey((byte*)encodedData + group_offset, groupAttr);

		if(aggAttr.type == TypeInt){
			int value = *(int*)ptr;
			if(hash_map.count(grp) == 0){
				if(op == COUNT) hash_map[grp] = 1;
				else hash_map[grp] = (float)value;
				count_map[grp] = 1;
			}
			else{
				float aggValue = hash_map[grp];
				valueAggregate((float)value,aggValue);
				hash_map[grp] = aggValue;
				count_map[grp]++;
			}
		}
		else if(aggAttr.type == TypeReal){
			float value = *(float*)ptr;
			if(hash_map.count(grp) == 0){
				if(op == COUNT) hash_map[grp] = 1;
				else hash_map[grp] = value;
				count_map[grp] = 1;
			}
			else{
				float aggValue = hash_map[grp];
				valueAggregate(value,aggValue);
				hash_map[grp] = aggValue;
				count_map[grp]++;
			}
		}
	}

	isDone = true;
	free(readData);
	free(encodedData);
	it = hash_map.begin();
	return 0;

}


void Aggregate::valueAggregate(float value, float &aggValue){
	switch(op){
		case MIN:
			if(aggValue > value) aggValue = value;
			break;
		case MAX:
			if(aggValue < value) aggValue = value;
			break;
		case COUNT:
			aggValue++;
			break;
		case SUM:
			aggValue += value;
			break;
		case AVG:
			aggValue += value;
			break;
	}

}

string Aggregate::getKey(void* data, Attribute attr){
	string key = "";

	switch(attr.type){
		case TypeInt:
			key =  std::to_string(*(int*)data);
			break;
		case TypeReal:
			key =  std::to_string(*(float*)data);
			break;
		case TypeVarChar:
			int s_len = *(int*)data;
			for(int i = 0;i<s_len;i++){
				key += *(char*)((byte*)data+sizeof(int) + i);
			}
	}
	return key;
}


GHJoin::GHJoin(Iterator *leftIn, Iterator *rightIn, const Condition &condition, const unsigned numPartitions){
	GHJoin::numPartitions = numPartitions;
	_rbfm = &RecordBasedFileManager::instance();

	//Get both attributes fisrt
	leftIn->getAttributes(left_attrs);
	rightIn->getAttributes(right_attrs);

	for(Attribute a:left_attrs) left_attrs_names.push_back(a.name);
	for(Attribute a:right_attrs) right_attrs_names.push_back(a.name);

	left_index = -1;
	for(int i = 0;i<(int)left_attrs.size();i++){
		if(left_attrs[i].name.compare(condition.lhsAttr) == 0){
			left_index = i;
			break;
		}
	}
	right_index = -1;
	for(int i = 0;i<(int)right_attrs.size();i++){
		if(right_attrs[i].name.compare(condition.rhsAttr) == 0){
			right_index = i;
			break;
		}
	}
	left_iter = nullptr;
	right_iter = nullptr;
	index = 0;
	list_index = -1;
	reverse = false;
	initialize(leftIn, rightIn,condition);
	input_buffer = malloc(PAGE_SIZE);
}

GHJoin::~GHJoin(){
	free(input_buffer);
	for(string s: left_temp_names) _rbfm->destroyFile(s);
	for(string s: right_temp_names) _rbfm->destroyFile(s);
}

void GHJoin::initialize(Iterator *leftIn, Iterator *rightIn,const Condition &condition){
	if(left_index == -1 || right_index == -1) return;
	

	//initialize all the file handler:
	for(unsigned i = 0; i<numPartitions;i++){
		FileHandle left_fh;
		string left_name = "left_join_"+condition.lhsAttr+"_partition_"+ std::to_string(i);
		_rbfm->createFile(left_name);
		_rbfm->openFile(left_name,left_fh);
		left_temp_names.push_back(left_name);
		left_fileHandlers.push_back(left_fh);

		FileHandle right_fh;
		string right_name = "right_join_"+condition.rhsAttr+"_partition_"+std::to_string(i);
		_rbfm->createFile(right_name);
		_rbfm->openFile(right_name,right_fh);
		right_temp_names.push_back(right_name);
		right_fileHandlers.push_back(right_fh);

	}

	void* data = malloc(PAGE_SIZE);
	//partition the left input:
	std::hash<std::string> h;
	while(leftIn->getNextTuple(data) != QE_EOF){
		string key = getKey(data,left_index,left_attrs);
		if(key.compare("") == 0) continue;
		unsigned part_num = 0;
		size_t hash_val = h(key);
		part_num = (unsigned)(hash_val % numPartitions);
		
		RID rid;
		_rbfm->insertRecord(left_fileHandlers[part_num], left_attrs, data, rid);
	}

	while(rightIn->getNextTuple(data) != QE_EOF){
		string key = getKey(data,right_index,right_attrs);
		if(key.compare("") == 0) continue;
		size_t hash_val = h(key);
		unsigned part_num = (unsigned)(hash_val % numPartitions);
		RID rid;
		_rbfm->insertRecord(right_fileHandlers[part_num], right_attrs, data, rid);
	}

	free(data);
}

string GHJoin::getKey(void* data, int index, std::vector<Attribute> attrs){
	string key = "";
	byte* encodedData = (byte*)malloc(PAGE_SIZE);
	_rbfm->encodeData(data,encodedData,attrs);
	short pos = *(short*)(encodedData + sizeof(short) + sizeof(short) * index);
	if(pos == -1){  //The key data is null. Ignore this tuple
		free(encodedData);
		return key;
	}
	void* realData = encodedData + pos;

	switch(attrs[index].type){
		case TypeInt:
			key =  std::to_string(*(int*)realData);
			break;
		case TypeReal:
			key =  std::to_string(*(float*)realData);
			break;
		case TypeVarChar:
			int s_len = *(int*)realData;
			for(int i = 0;i<s_len;i++){
				key += *(char*)((byte*)realData+sizeof(int) + i);
			}
	}
	free(encodedData);
	return key;
}

RC GHJoin::loadOuterPages(){
	hash_map.clear();

	if(index >= (int)numPartitions) return -1;
	FileHandle left_fh = left_fileHandlers[index];
	FileHandle right_fh = right_fileHandlers[index];
	index++;
	while(left_fh.getNumberOfPages() == 0 || right_fh.getNumberOfPages() == 0){

		_rbfm->closeFile(left_fh);
		_rbfm->closeFile(right_fh);
		if(index >= (int)numPartitions) return -1;
		left_fh = left_fileHandlers[index];
		right_fh = right_fileHandlers[index];
		index++;
	}
    reverse = left_fh.getNumberOfPages() > right_fh.getNumberOfPages();
	left_iter = new RBFM_ScanIterator();
	right_iter = new RBFM_ScanIterator();
	if(reverse){
		_rbfm->scan(right_fh,right_attrs, "", NO_OP, nullptr, right_attrs_names, *left_iter);
		_rbfm->scan(left_fh,left_attrs, "", NO_OP, nullptr, left_attrs_names, *right_iter);
	}
	else{
		_rbfm->scan(left_fh,left_attrs, "", NO_OP, nullptr, left_attrs_names, *left_iter);
		_rbfm->scan(right_fh,right_attrs, "", NO_OP, nullptr, right_attrs_names, *right_iter);
	}

	void* data = malloc(PAGE_SIZE);
	RID rid;
	while(left_iter->getNextRecord(rid, data) != QE_EOF){
		RID newRID;
		newRID.pageNum = rid.pageNum;
		newRID.slotNum = rid.slotNum;
		string key = "";
		if(reverse) key = getKey(data,right_index,right_attrs);
		else key = getKey(data,left_index,left_attrs);
		if(key.compare("") == 0) continue;
		if(hash_map.count(key) == 0){
			std::vector<RID> data_list;
			data_list.push_back(newRID);
			hash_map[key] = data_list;
		}
		else hash_map[key].push_back(newRID);
	}
	free(data);
	return 0;
}

RC GHJoin::getNextTuple(void *data){
	if(left_index == -1 || right_index == -1) return -1;
	void* left_data = malloc(PAGE_SIZE);

	if(list_index != -1){   //Used to handle duplicate key situation
		if(list_index >= int(cur_data_list->size())){
			list_index = -1;
			cur_data_list = nullptr;
		}
		else{
			RID left_rid = cur_data_list->at(list_index);
			if(!reverse){
				_rbfm->readRecord(left_fileHandlers[index-1], left_attrs, left_rid, left_data);
				formatReturnValue(left_data,input_buffer,data);
			}
			else{
				_rbfm->readRecord(right_fileHandlers[index-1], right_attrs, left_rid, left_data);
				formatReturnValue(input_buffer,left_data,data);
			}
			list_index++;
			free(left_data);
			return 0;
		}
	}
	if(right_iter != nullptr){
		RID right_rid;
		while(right_iter->getNextRecord(right_rid,input_buffer) != QE_EOF){
			string key = "";
			if(!reverse) key = getKey(input_buffer,right_index,right_attrs);
			else key = getKey(input_buffer,left_index,left_attrs);
			
			if(hash_map.count(key) != 0){
				cur_data_list = &hash_map[key];
				list_index = 0;

				RID left_rid = cur_data_list->at(list_index);
				if(!reverse){
					_rbfm->readRecord(left_fileHandlers[index-1], left_attrs, left_rid, left_data);

					formatReturnValue(left_data,input_buffer,data);
				}
				else{
					_rbfm->readRecord(right_fileHandlers[index-1], right_attrs, left_rid, left_data);

					formatReturnValue(input_buffer,left_data,data);
				}
				list_index++;
				free(left_data);
				return 0;
			}
		}
	}
	if(right_iter != nullptr) {
		right_iter->close();
		delete right_iter;
		right_iter = nullptr;

		left_iter->close();
		delete left_iter;
		left_iter = nullptr;
	}
	free(left_data);
	if(loadOuterPages() == -1){
		return -1;
	}
	return getNextTuple(data);
}

void GHJoin::formatReturnValue(void* left_data, void* right_data, void* data){
	byte* left_ptr = (byte*)left_data;
	byte* right_ptr = (byte*)right_data;
	byte* ptr = (byte*)data;

	int nullBitSize = (int)ceil((double)(left_attrs.size() + right_attrs.size()) / CHAR_BIT);
	memset(data,0,nullBitSize);


    int leftNullBitSize = (int)ceil((double)left_attrs.size() / CHAR_BIT);
	int rightNullBitSize = (int)ceil((double)right_attrs.size() / CHAR_BIT);
	short left_len = _rbfm->recordLength(left_attrs,left_ptr) - (left_attrs.size() + 1 ) * sizeof(short);
	short right_len = _rbfm->recordLength(right_attrs,right_ptr) - (right_attrs.size() + 1) * sizeof(short);

	memcpy(data,left_ptr,leftNullBitSize);
	int pos = 0;
	pos = left_attrs.size();
	byte* nullBitIndicator;
    nullBitIndicator = (byte*) data;

    byte* right_nullBitIndicator;
    right_nullBitIndicator = (byte*) right_data;

    for(int i = 0;i<rightNullBitSize;i++){
    	unsigned char right_compareBit = *(right_nullBitIndicator + ((int)floor((double)i / CHAR_BIT)));
    	if ((right_compareBit & (unsigned) 1 << (unsigned) (CHAR_BIT - 1 - i % CHAR_BIT))){
    		unsigned char compareBit = *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT)));
            compareBit = compareBit ^ ((unsigned) 1 << (unsigned) (CHAR_BIT - 1 - pos % CHAR_BIT));
            *(nullBitIndicator + ((int)floor((double)pos / CHAR_BIT))) = compareBit;
    	}
    	pos++;
    }
    
    
    ptr += nullBitSize;
    left_ptr += leftNullBitSize;
    right_ptr += rightNullBitSize;


    memcpy(ptr,left_ptr,left_len);
    ptr += left_len;
    memcpy(ptr,right_ptr,right_len);
}
RC INLJoin::getNextTuple(void *data) {
    if (this->curLeftData == nullptr) {
        curLeftData = malloc(PAGE_SIZE);
        if (this->leftIn->getNextTuple(curLeftData) == EOF) {
            free(curLeftData);
            return EOF;
        }
    }

    void *rightData = malloc(PAGE_SIZE);

    /**if the right attributes exists, we scan the inner table and open the B+ tree of the right tablet**/
    vector<Attribute> leftAttrs;
    this->leftIn->getAttributes(leftAttrs);

    void *encodeLeftData = malloc(PAGE_SIZE);
    this->_rbfm->encodeData(curLeftData, encodeLeftData, leftAttrs);
    while (true) {
        if (this->rightIn->getNextTuple(rightData) == EOF) { // if the right table search is done. We get the next data from the left table.
            if (this->leftIn->getNextTuple(curLeftData) == EOF) {
                free(curLeftData);
                free(encodeLeftData);
                free(rightData);
                return EOF;
            }

            /**reset the right table scan to the initial position, move left table to plus one**/
            this->_rbfm->encodeData(curLeftData, encodeLeftData, leftAttrs);
            this->rightIn->setIterator(NULL, NULL, true, true);
            if (this->rightIn->getNextTuple(rightData) == EOF) return EOF;
        }
        vector<Attribute> rightAttrs;
        this->rightIn->getAttributes(rightAttrs);

        void *encodeRightData = malloc(PAGE_SIZE);
        this->_rbfm->encodeData(rightData, encodeRightData, rightAttrs);

        short leftDataOffset = *(short *)((char *)encodeLeftData + sizeof(short) + sizeof(short) * this->leftAttrIndex);
        short rightDataOffset = *(short *)((char *)encodeRightData + sizeof(short) + sizeof(short) * this->rightAttrIndex);

        switch (this->condition.op) {
            case EQ_OP:
                if (this->_rbfm->keyCompare((char *)encodeLeftData + leftDataOffset, (char *)encodeRightData + rightDataOffset, this->attrType) == 0) {
                    mergeData(encodeLeftData, encodeRightData, leftAttrs, rightAttrs, data);
                    free(encodeLeftData);
                    free(encodeRightData);
                    return 0;
                } else if (this->_rbfm->keyCompare((char *)encodeLeftData + leftDataOffset, (char *)encodeRightData + rightDataOffset, this->attrType) < 0) {
                    /**if the key is already greater than current left key, we reset the right scan, and get the nwe left key**/
                    if (this->leftIn->getNextTuple(curLeftData) == EOF) {
                        free(curLeftData);
                        free(encodeLeftData);
                        free(rightData);
                        return EOF;
                    }
                    _rbfm->encodeData(curLeftData, encodeLeftData, leftAttrs);
                    this->rightIn->setIterator(NULL, NULL, true, true);
                    continue;
                }

                break;
            case LE_OP:
                if (this->_rbfm->keyCompare((char *)encodeLeftData + leftDataOffset, (char *)encodeRightData + rightDataOffset, this->attrType) <= 0) {
                    mergeData(encodeLeftData, encodeRightData, leftAttrs, rightAttrs, data);
                    free(encodeLeftData);
                    free(encodeRightData);
                    return 0;
                }
                break;
            case LT_OP:
                if (this->_rbfm->keyCompare((char *)encodeLeftData + leftDataOffset, (char *)encodeRightData + rightDataOffset, this->attrType) < 0) {
                    mergeData(encodeLeftData, encodeRightData, leftAttrs, rightAttrs, data);
                    free(encodeLeftData);
                    free(encodeRightData);
                    return 0;
                }
                break;
            case NO_OP:
                if (this->_rbfm->keyCompare((char *)encodeLeftData + leftDataOffset, (char *)encodeRightData + rightDataOffset, this->attrType) == 0) {
                    mergeData(encodeLeftData, encodeRightData, leftAttrs, rightAttrs, data);
                    free(encodeLeftData);
                    free(encodeRightData);
                    return 0;
                }
                break;
            default:
            	break;
        }
    }
    return 0;
}

RC INLJoin::mergeData(void *leftEncodeData, void *rightEncodeData, vector<Attribute> leftAttrs,
                      vector<Attribute> rightAttrs, void *mergedData) {
    short numberOfLeftAttributes = *(short *)leftEncodeData;
    short numberOfRightAttributes = *(short *)rightEncodeData;
    unsigned numberOfNullIndicators = (int)ceil((double)(numberOfLeftAttributes + numberOfRightAttributes)/ CHAR_BIT);

    auto *nullIndicator = (unsigned char *)malloc(numberOfNullIndicators);
    memset(nullIndicator, 0, numberOfNullIndicators);

    int currentBit = numberOfNullIndicators * CHAR_BIT - 1;

    int realDataOffset = numberOfNullIndicators;

    int attrIndex = 0;
    for (auto &attr: leftAttrs) {
        short offset = *(short *)((char *)leftEncodeData + sizeof(short) + sizeof(short) * attrIndex++);
        if (offset == -1) {
            nullIndicator[currentBit / CHAR_BIT]
                    ^= 1UL << (currentBit);
            continue;
        }
        auto *copyPtr = (char *)leftEncodeData + offset;
        switch (attr.type) {
            case TypeInt:
                memcpy((char *)mergedData + realDataOffset, copyPtr, sizeof(int));
                realDataOffset += sizeof(int);
                break;
            case TypeReal:
                memcpy((char *)mergedData + realDataOffset, copyPtr, sizeof(float));
                realDataOffset += sizeof(float);
                break;
            case TypeVarChar:
                unsigned varcharLength = *(unsigned *)(copyPtr);
                memcpy((char *)mergedData + realDataOffset, copyPtr, sizeof(int) + varcharLength);
                realDataOffset += varcharLength + sizeof(int);
                break;
        }
        currentBit--;
    }

    attrIndex = 0;
    for (auto &attr: rightAttrs) {
        short offset = *(short *)((char *)rightEncodeData + sizeof(short) + sizeof(short) * attrIndex++);
        auto *copyPtr = (char *)rightEncodeData + offset;
        if (offset == -1) {
            nullIndicator[currentBit / CHAR_BIT]
                    ^= 1UL << (currentBit);
            continue;
        }
        switch (attr.type) {
            case TypeInt:
                memcpy((char *)mergedData + realDataOffset, copyPtr, sizeof(int));
                realDataOffset += sizeof(int);
                break;
            case TypeReal:
                memcpy((char *)mergedData + realDataOffset, copyPtr, sizeof(float));
                realDataOffset += sizeof(float);
                break;
            case TypeVarChar:
                unsigned varcharLength = *(unsigned *)(copyPtr);
                memcpy((char *)mergedData + realDataOffset, copyPtr, sizeof(unsigned) + varcharLength);
                realDataOffset += varcharLength + sizeof(unsigned);
                break;
        }
        currentBit--;
    }
    memcpy((char *)mergedData, (char *)nullIndicator, numberOfNullIndicators);
    return 0;
}


