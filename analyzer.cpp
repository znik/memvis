#ifdef _WIN32
#include <Windows.h>
#else
#include <memory.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <math.h>

#include <cstdio>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include "stopwatch.h"
#include "jsonreader.hpp"
#include "jsonwriter.hpp"


// We are analyzing 100k consecutive accesses by all threads
// to determine the metric that serves to determine false sharing.

#define SAMPLES_NUM		500000
#define ONE_PERCENT		(SAMPLES_NUM / 100)

//#define THREADS_NUM		7
//#define THREADS_NUM		29
static unsigned MY_THREADS_NUM	= 0;

#define MB	(1 << 20)

#define MIN_REF_THRESHOLD	50

// /O2	-- 497s


const static char* REPORT_VER = "0 4 0 0";


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
			bool em = true;
			for (auto e : _REFS) {
				if (MIN_REF_THRESHOLD <= e.second._refs)
					em = false;
			}
			if (em)
				return;

			jsonfile json(filename);
			for (auto e : _REFS) {
				
				if (MIN_REF_THRESHOLD > e.second._refs)
					continue;

				json.start_collection();
				json.write_to_collection("addr", std::to_string(e.first));
				json.write_to_collection("refs", std::to_string(e.second._refs));
				for (unsigned k = 0; k < e.second._hash_info.size(); ++k) {
					int hash = e.second._hash_info[k];
					json.write_to_collection(std::string("inf") + std::to_string(k), hash ? _INFO[hash] : "");
					json.write_to_collection(std::string("ref") + std::to_string(k), std::to_string(e.second._thr_refs[k]));
				}
				json.end_collection();
			}
		}
		void reset() { _REFS.clear(); _INFO.clear(); _CACHELINE_LOADS.clear(); MAX_WRITES = 0; _FUNC.clear(); _FUNC_REFS.clear();
			_FUNC_FALSESHARES.clear();
		};
		void ref(const int threadid, const ull_t addr,
			const std::string& info,					// src-location
			const std::string& varname,
			const std::string& type,
			const std::string& funcname,
			const std::string& allocloc)				// alloc-location
		{
/*			if (threadid >= THREADS_NUM || threadid < 0) {
				printf("ERROR: unexpected thread id (set THREAD_NUM at least to %d)\n", threadid + 1);
				assert(false && "ThreadId has unexpected value");
				return;
			}
*/
			const std::string info_str = info + "(" + varname + ", " + funcname + ", " + allocloc + ")";
			const int hash = hasher(info_str);
			const int funchash = hasher(funcname + ":" + varname + ":" + std::to_string(addr / 64));

			// WARNING (FIXME): Possible loss of data if the same address is used
			// for different vars after freeing
			
			std::string& str = _INFO[hash];
			if (str.empty())
				str = info_str;
			
			std::string& function = _FUNC[funchash];
			if (function.empty())
				function = funcname + ":" + varname + ":" + std::to_string(addr / 64);

			const short access_type = ("read" == type) ? READ_TYPE : WRITE_TYPE;

			try {
				// (Other thread wrote to this address)
				addr_info_t& addrinfo = _CACHELINE_LOADS.at(addr / 64);

				if (WRITE_TYPE == access_type && !addrinfo._updated.empty())
					memset(&addrinfo._updated[0], 0x0, addrinfo._updated.size());

				// (!) This is the core part: calculating metric that represents
				// the extent of sharing.

				if (addrinfo._updated.size() <= unsigned(threadid)) {
					addrinfo.resize(threadid + 1);
				}

				if (addrinfo._lastthread != threadid) {
					++addrinfo._thr_refs[threadid];
					addrinfo._lastthread = threadid;
					addrinfo._hash_info[threadid] = hash;

					if (0 == addrinfo._updated[threadid] || WRITE_TYPE == access_type)
					{
						single_address_ref(addr, threadid, hash);
						++addrinfo._refs;

						// (!) Another metric is how many refs increments
						// occured in each function context.
						++_FUNC_REFS[funchash];

						// FALSE SHARING SECTION {
						if (addrinfo._lastaddr != addr && 0 != addrinfo._lastaddr)
							++_FUNC_FALSESHARES[funchash];
						addrinfo._lastaddr = addr;
						// }

						if (addrinfo._refs > MAX_WRITES)
							MAX_WRITES = addrinfo._refs;
					}

					addrinfo._updated[threadid] = 1;
				}
			}
			catch (const std::out_of_range&) {
				// (First write to the address)
				addr_info_t& addrinfo = _CACHELINE_LOADS[addr / 64];

				addrinfo.resize(threadid + 1);

				addrinfo._lastthread = threadid;
				addrinfo._hash_info[threadid] = hash;

				addrinfo._lastaddr = addr;

				single_address_ref(addr, threadid, hash);

				addrinfo._updated[threadid] = 1;
			}
			catch (...) {};
		};

		void single_address_ref(const ull_t addr, const int threadid, const int hash) {
			try {
				addr_info_t& addrinfo = _REFS.at(addr);

				if (addrinfo._updated.size() <= unsigned(threadid))
					addrinfo.resize(threadid + 1);
				++addrinfo._thr_refs[threadid];
				addrinfo._hash_info[threadid] = hash;
				++addrinfo._refs;
			}
			catch (const std::out_of_range&) {
				addr_info_t& addrinfo = _REFS[addr];
				addrinfo.resize(threadid + 1);
				addrinfo._lastthread = threadid;
				addrinfo._hash_info[threadid] = hash;
			}
			catch (...) {};
		}

		void write_functions(jsonfile& where, const int num) const {
			const std::string& stringnum = std::to_string(num);
			auto write_zeroes = [](jsonfile& where, const std::string& stringnum) {
				where.start_collection();
				where.write_to_collection("num", stringnum);
				where.write_to_collection("max", "0");
				where.write_to_collection("func", "");
				where.write_to_collection("refs", "0");
				where.write_to_collection("fals", "0");
				where.end_collection();
			};

			// Fake records for ranges with little sharing
			if (0 == _FUNC_REFS.size()) {
				write_zeroes(where, stringnum);
				return;
			}
			
			const std::string& maxwrites = std::to_string(max_writes());
			bool written = false;
			for (auto i : _FUNC_REFS) {
				if (MIN_REF_THRESHOLD > i.second)
					continue;
				written = true;
				where.start_collection();
				where.write_to_collection("num", stringnum);
				where.write_to_collection("max", maxwrites);
				where.write_to_collection("func", _FUNC[i.first]);
				where.write_to_collection("refs", std::to_string(i.second));
				where.write_to_collection("fals", std::to_string(_FUNC_FALSESHARES[i.first]));
				where.end_collection();
			}

			if (!written) {
				write_zeroes(where, stringnum);
			}
		}

		inline const int max_writes() const { return MAX_WRITES;  }
		inline const bool empty() const { return _REFS.size() == 0; }
	private:
		enum access_type {
			READ_TYPE = 1,
			WRITE_TYPE
		};
		struct addr_info_t {				// describes a cache line
			void resize(size_t threads_count) {
				_updated.resize(threads_count);
				_thr_refs.resize(threads_count);
				_hash_info.resize(threads_count);
				if (MY_THREADS_NUM < threads_count)
					MY_THREADS_NUM = threads_count;
			}
			addr_info_t() : _lastthread(-1), _lastaddr(0), _refs(0) {
				//memset(_hash_info, 0x0, sizeof(_hash_info));
				//memset(_thr_refs, 0x0, sizeof(_thr_refs));
				//memset(_updated, 0x0, sizeof(_updated));
			}
			short _lastthread;				// the last thread that wrote/read
			ull_t _lastaddr;				// the last address accessed
			std::vector<char> _updated;		// CL has been read
			int _refs;						// all references to the CL
			std::vector<int> _hash_info;	// how each thread "call" variable in the CL
			std::vector<int> _thr_refs;		// number of re-writes per thread
			//char __padding[64 - (sizeof(__int32)*(2*THREADS_NUM + 2) % 64)];
		};
		std::map/*(addr, threadid)*/<ull_t, addr_info_t> _REFS;
		std::map<ull_t, addr_info_t> _CACHELINE_LOADS;
		mutable std::map/*(hash, info)*/<int, std::string> _INFO;

		std::map/*(funchash, refcount)*/<int, int> _FUNC_REFS;
		mutable std::map/*(funchash, false_shares)*/<int, int> _FUNC_FALSESHARES;
		mutable std::map/*(hash, funcname)*/<int, std::string> _FUNC;

		int MAX_WRITES;
	} MY_REFS;
}


int main(int argc, char *argv[]) {

	if (argc != 3 && argc != 2) {
		printf("Usage: analyzer <trace_file> [destination_folder]\n"
			"\n*trace_file - a file containing a memory trace in a specific format,\n"
			"*destination_folder - folder name where to put the processed files.\n");
		return 0;
	}

	struct {
		std::string first;
		std::string second;
	} i = { argv[1], argc == 2 ? "data" : argv[2] };

	i.second = "server/" + i.second;

	swatch timer;

	//std::vector<std::pair<std::string, std::string>> inout_files;
	//for (auto i : inout_files) {
#ifdef _WIN32
		SHFILEOPSTRUCTA sh = { 0, FO_DELETE, i.second.c_str(), NULL,
			FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION,
			FALSE, NULL, NULL };
		SHFileOperationA(&sh);

		CreateDirectoryA((LPCSTR)i.second.c_str(), NULL);
#else // _WIN32
		rmdir(i.second.c_str());
		if (int er = mkdir(i.second.c_str(), 0777)) {
			printf("Cannot create a data dir %s (error: %s)\n", i.second.c_str(), strerror(er));
			return 0;
		}

#endif // _WIN32
		MY_REFS.reset();

		jsonreader reader(i.first.c_str());
		jsonreader::line_t line;
		int idx = 0;
		{
			jsonfile json(i.second + "/main.json");

			while (reader.readline(line)) {
				const std::string &saddr = line.get("address");
				const std::string &stid = line.get("thread-id");
				if (saddr.empty() || stid.empty())
					continue;

				if (0 != idx && 0 == idx % SAMPLES_NUM) {
					printf("\r#%d MAX_metric=%d\n", int(idx / SAMPLES_NUM), MY_REFS.max_writes());

					MY_REFS.dump(i.second + '/' + std::to_string(int(idx / SAMPLES_NUM)) + ".json");
					MY_REFS.write_functions(json, int(idx / SAMPLES_NUM));
					MY_REFS.reset();
				}
				++idx;

				ull_t addr = std::stoull(saddr, 0, 16);
				int tid = std::stoi(stid);

				MY_REFS.ref(tid, addr, line.get("source-location"),
					line.get("var-name"), line.get("type"), line.get("function"), line.get("alloc-location"));
			}

			jsonfile info(i.second + "/info.json");
			info.start_collection();
			std::string fname = i.first;
			std::replace(fname.begin(), fname.end(), '.', '#');
			std::replace(fname.begin(), fname.end(), '\\', '@');
			std::replace(fname.begin(), fname.end(), '/', '@');

			info.write_to_collection("file", fname);
			info.write_to_collection("ver", REPORT_VER);
			info.write_to_collection("threadnum", std::to_string(MY_THREADS_NUM));
			info.end_collection();
		}
	//}
	timer.stop("ELAPSED");
	getchar();
	return 1;
}
