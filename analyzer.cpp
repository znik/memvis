#ifdef _WIN32
#include <Windows.h>
#endif

#include <math.h>

#include <cstdio>
#include <string>
#include <vector>
#include <functional>
#include "stopwatch.h"
#include "jsonreader.hpp"
#include "jsonwriter.hpp"


// We are analyzing 100k consecutive accesses by all threads
// to determine the metric that serves to determine false sharing.

#define SAMPLES_NUM		100000
#define ONE_PERCENT		(SAMPLES_NUM / 100)

//#define THREADS_NUM		7
#define THREADS_NUM		29

#define MB	(1 << 20)


// /O2	-- 497s



namespace /*memory regions information*/ {
	/*
	__declspec(align(64)) struct xs_desc_t {
		__int32 _all;				// how many accesses to the address averall
		__int32 _thr[THREADS_NUM];
		char __padding[64 - (sizeof(__int32)*(THREADS_NUM + 1) % 64)];
	} MEM_REGIONS;
	*/
	typedef unsigned long long ull_t;
}


namespace /*variables information*/ {
	auto hasher = std::hash<std::string>();

	struct {
		void dump(const std::string& filename) const {
			if (0 == MAX_WRITES)
				return;
			jsonfile json(filename);
			for (auto e : _REFS) {
				if (50 > e.second._refs)
					continue;

				json.start_collection();
				json.write_to_collection("addr", std::to_string(e.first));
				json.write_to_collection("refs", std::to_string(e.second._refs));
				for (unsigned k = 0; k < THREADS_NUM; ++k) {
					int hash = e.second._hash_info[k];
					json.write_to_collection(std::string("inf") + std::to_string(k), hash ? _INFO[hash] : "");
					json.write_to_collection(std::string("ref") + std::to_string(k), std::to_string(e.second._thr_refs[k]));
				}
				json.end_collection();
			}
		}
		void reset() { _REFS.clear(); _INFO.clear(); _CACHELINE_LOADS.clear(); MAX_WRITES = 0; };
		void ref(const int threadid, const ull_t addr, const std::string& info,
			const std::string& varname,
			const std::string& type,
			const std::string& funcname)
		{
			if (threadid >= THREADS_NUM || threadid < 0) {
				printf("ERROR: unexpected thread id (set THREAD_NUM at least to %d)\n", threadid + 1);
				assert(false && "ThreadId has unexpected value");
				return;
			}
			const std::string info_str = info + "(" + varname + ", " + funcname + ")";
			const int hash = hasher(info_str);
			// WARNING (FIXME): Possible loss of data if the same address is used
			// for different vars after freeing
			std::string& str = _INFO[hash];
			if (str.empty())
				str = info_str;
			const short access_type = ("read" == type) ? READ_TYPE : WRITE_TYPE;

			try {
				// Other thread wrote to this address
				addr_info_t& addrinfo = _CACHELINE_LOADS.at(addr / 64);

				if (WRITE_TYPE == access_type)
					memset(addrinfo._updated, 0x0, sizeof(addrinfo._updated));

				if (addrinfo._lastthread != threadid) {
					++addrinfo._thr_refs[threadid];
					addrinfo._lastthread = threadid;
					addrinfo._hash_info[threadid] = hash;

					if (0 == addrinfo._updated[threadid] || WRITE_TYPE == access_type)
					{
						single_address_ref(addr, threadid, hash);
						++addrinfo._refs;
						if (addrinfo._refs > MAX_WRITES)
							MAX_WRITES = addrinfo._refs;
					}

					addrinfo._updated[threadid] = 1;
				}
			}
			catch (const std::out_of_range&) {
				// First write to the address
				addr_info_t& addrinfo = _CACHELINE_LOADS[addr / 64];
				addrinfo._lastthread = threadid;
				addrinfo._hash_info[threadid] = hash;

				single_address_ref(addr, threadid, hash);

				addrinfo._updated[threadid] = 1;
			}
			catch (...) {};
		};

		void single_address_ref(const ull_t addr, const int threadid, const int hash) {
			try {
				addr_info_t& addrinfo = _REFS.at(addr);
				++addrinfo._thr_refs[threadid];
				addrinfo._hash_info[threadid] = hash;
				++addrinfo._refs;
			}
			catch (const std::out_of_range&) {
				addr_info_t& addrinfo = _REFS[addr];
				addrinfo._lastthread = threadid;
				addrinfo._hash_info[threadid] = hash;
			}
			catch (...) {};
		}

		inline const int max_writes() const { return MAX_WRITES;  }
		inline const bool empty() const { return _REFS.size() == 0; }
	private:
		enum access_type {
			READ_TYPE = 1,
			WRITE_TYPE
		};
		struct addr_info_t {				// describes a cache line
			addr_info_t() : _lastthread(-1), _refs(0) {
				memset(_hash_info, 0x0, sizeof(_hash_info));
				memset(_thr_refs, 0x0, sizeof(_thr_refs));
				memset(_updated, 0x0, sizeof(_updated));
			}
			short _lastthread;				// the last thread that wrote/read
			char _updated[THREADS_NUM];		// CL has been read
			int _refs;						// all references to the CL
			int _hash_info[THREADS_NUM];	// how each thread "call" variable in the CL (FIXME: many vars in CL!)
			int _thr_refs[THREADS_NUM];		// number of re-writes per thread
			//char __padding[64 - (sizeof(__int32)*(2*THREADS_NUM + 2) % 64)];
		};
		std::map/*(addr, threadid)*/<ull_t, addr_info_t> _REFS;
		std::map<ull_t, addr_info_t> _CACHELINE_LOADS;
		mutable std::map/*(hash, info)*/<int, std::string> _INFO;

		int MAX_WRITES;
	} MY_REFS;
}


void main() {
	swatch timer;

	std::vector<std::pair<std::string, std::string>> inout_files;
	//inout_files.push_back(std::make_pair("../../ldb-readrandom-24t-with-frees.json", "bigdata.3"));
	//inout_files.push_back(std::make_pair("../../sharing-mystery-1.json", "data.mystery-1"));
	//inout_files.push_back(std::make_pair("../../sharing-mystery-2.json", "data.mystery-2"));
	inout_files.push_back(std::make_pair("../../sharing-mystery1-new.json", "data.mystery-adv"));
	for (auto i : inout_files) {

		SHFILEOPSTRUCTA sh = { 0, FO_DELETE, i.second.c_str(), NULL,
			FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION,
			FALSE, NULL, NULL };
		SHFileOperationA(&sh);

		CreateDirectoryA((LPCSTR)i.second.c_str(), NULL);
		MY_REFS.reset();

		//jsonreader reader("../../data/false_sharing.json");
		//jsonreader reader("../../wt-ldb-2.json");
		jsonreader reader(i.first.c_str());
		jsonreader::line_t line;
		int idx = 0;
		int percent = 0;
		{
			jsonfile json(i.second + "/main.json");
			while (reader.readline(line)) {
				if (0 != idx && 0 == idx % SAMPLES_NUM) {
					printf("\rMAX=%d\n", MY_REFS.max_writes());

					MY_REFS.dump(i.second + '/' + std::to_string(int(idx / SAMPLES_NUM)) + ".json");

					if (!MY_REFS.empty()) {
						json.start_collection();
						json.write_to_collection("num", std::to_string(int(idx / SAMPLES_NUM)));
						json.write_to_collection("max", std::to_string(MY_REFS.max_writes()));
						json.end_collection();
					}

					MY_REFS.reset();
				}
				++idx;
				const std::string &saddr = line.get("address");
				const std::string &stid = line.get("thread-id");
				if (saddr.empty() || stid.empty())
					continue;
				ull_t addr = std::stoull(saddr, 0, 16);
				int tid = std::stoi(stid);

				MY_REFS.ref(tid, addr, line.get("source-location"),
					line.get("var-name"), line.get("type"), line.get("function"));
			}
		}
	}
	timer.stop("ELAPSED");
	getchar();
}
