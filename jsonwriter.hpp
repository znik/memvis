/*
{"event": "memory-access", "type" : "write", "thread-id" : "0", "address" : "0x00007fff15bb25a0", "size" : "8", "function" : "__vsscanf", "source-location" : "<unknown>", "allo
c - location": "<unknown>", "var - name": "<unknown>", "var - type": "<unknown>"},
*/

/*
	WARNING:
	!Writing to the file is synchroneous and makes other threads to wait if there are any.
*/

#pragma once


#include <cstdio>
#include <string>
#include <mutex>

struct jsonfile {
	jsonfile(const std::string& path_to_json) : _json(nullptr), _more_than_1_collection(false),
		_more_than_1_entry(false), _in_collection(false) {
		_json = fopen(path_to_json.c_str(), "a+");
		if (_json)
			fprintf(_json, "[\n");
		else
			printf("[JSONWRITER] Cannot create/open file %s\n", path_to_json.c_str());
	}

	bool write(const int tid, const void* const addr, const int size) const {
		if (nullptr == _json)
			return false;

		_lk.lock();
		fprintf(_json, "{\"event\": \"memory-access\", \"type\" : \"write\", \"thread-id\" : \"%d\", ", tid);
		fprintf(_json, "\"address\" : \"0x%p\", \"size\" : \"%d\", \"function\" : \"[stub]\"},\n", addr, size);
		_lk.unlock();
		return true;
	}

	bool start_collection() {
		if (nullptr == _json)
			return false;
		if (_in_collection) {
			printf("[JSONWRITER] Nested collections are not implemented!\n");
			assert(false && "Nested collections are not implemented!\n");
			return false;
		}
		_in_collection = true;
		if (_more_than_1_collection)
			fprintf(_json, ",");
		fprintf(_json, "{");
		_more_than_1_collection = true;
		return true;
	}

	bool write_to_collection(const std::string& key, const std::string& value) {
		if (nullptr == _json)
			return false;
		if (!_in_collection) {
			printf("[JSONWRITER] Writing to collection w/o starting one!\n");
			assert(false && "Writing to collection w/o starting one!\n");
			return false;
		}
		if (_more_than_1_entry)
			fprintf(_json, ",");
		else
			_more_than_1_entry = true;
		fprintf(_json, "\"%s\": \"%s\"", key.c_str(), value.c_str());
		return true;
	}

	bool end_collection() {
		if (nullptr == _json)
			return false;
		if (!_in_collection) {
			printf("[JSONWRITER] Ending collection w/o starting one!\n");
			assert(false && "Ending collection w/o starting one!\n");
			return false;
		}

		fprintf(_json, "}\n");
		_more_than_1_entry = false;
		_in_collection = false;
		return true;
	}

	~jsonfile() {
		if (_json) {
			fprintf(_json, "\n]");
			fclose(_json);
		}
		_json = 0;
	}
private:
	mutable std::mutex _lk;
	FILE * _json;

	bool _more_than_1_collection;
	bool _more_than_1_entry;
	bool _in_collection;
};
