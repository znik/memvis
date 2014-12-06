#ifdef _WIN32
#include <Windows.h>
#else
#include <memory.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#endif

#include <math.h>

#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <functional>
#include <algorithm>
#include "stopwatch.h"
#include "jsonreader.hpp"
#include "jsonwriter.hpp"


// We are analyzing 500k consecutive accesses by all threads
// to determine the metric that serves to determine sharing/false sharing.

#define SAMPLES_NUM		500000
#define ONE_PERCENT		(SAMPLES_NUM / 100)

static unsigned MY_THREADS_NUM	= 0;

#define MB	(1 << 20)
#define MIN_REF_THRESHOLD	50


const static char* REPORT_VER = "0 5 0 0";


namespace {
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
			const std::string& allocloc,				// alloc-location
			const std::string& vartype)					// var-type: type, field offset
		{
			const std::string info_str = info + "(" + varname + ", " + funcname + ", " + allocloc + ", " + vartype + ")";
			const int hash = hasher(info_str);
			const int funchash = hasher(funcname + ":" + varname + ":" + std::to_string(addr / 64));
			
			std::string& str = _INFO[hash];
			if (str.empty())
				str = info_str;

			std::string tmp;
			std::string& function = _FUNC[funchash];
			if (function.empty())
				tmp = funcname + ":" + varname + ":" + std::to_string(addr / 64);

			size_t pos;

			if (std::string::npos != (pos = varname.find('>'))) {
				if (pos + 1 < varname.size())
					tmp += ":" + varname.substr(pos + 1);
				else
					fprintf(stderr, "WARNING! unexpected var-name: %s\n", varname.c_str());
			}
			function = tmp;

			const short access_type = ("read" == type) ? READ_TYPE : WRITE_TYPE;

			try {
				// FIXME DOCUMENTME
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
			addr_info_t() : _lastthread(-1), _lastaddr(0), _refs(0) {}

			short _lastthread;				// the last thread that wrote/read
			ull_t _lastaddr;				// the last address accessed
			std::vector<char> _updated;		// CL has been read
			int _refs;						// all references to the CL
			std::vector<int> _hash_info;	// how each thread "call" variable in the CL
			std::vector<int> _thr_refs;		// number of re-writes per thread
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


void processingBody(const std::string& out_data, std::istream& injson, const std::string& in_file = std::string()) {

	printf("*writing output data to folder \"%s\"...\n", out_data.c_str());
	swatch timer;

#ifdef _WIN32
	std::vector<char> path_dbl0(out_data.size() + 2, '\0');
	memcpy(&path_dbl0[0], out_data.c_str(), out_data.size());
	SHFILEOPSTRUCTA sh = { 0, FO_DELETE, &path_dbl0[0], NULL,
		FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION,
		FALSE, NULL, NULL };
	SHFileOperationA(&sh);

	CreateDirectoryA((LPCSTR)out_data.c_str(), NULL);
#else // _WIN32
	std::string cmd = "rm -rf " + out_data;
	int er = system(cmd.c_str());
	(void)er;
	if ((er = mkdir(out_data.c_str(), 0777))) {
		printf("Cannot create a data dir %s (error: %s)\n", out_data.c_str(), strerror(er));
		return;
	}

#endif // _WIN32
	MY_REFS.reset();

	jsonreader reader(injson);
	jsonreader::line_t line;
	int idx = 0;
	{
		jsonfile json(out_data + "/main.json");

		while (reader.readline(line)) {
			const std::string &saddr = line.get("address");
			const std::string &stid = line.get("thread-id");
			if (saddr.empty() || stid.empty())
				continue;

			if (0 != idx && 0 == idx % SAMPLES_NUM) {
				printf("\r#%d MAX_metric=%d\n", int(idx / SAMPLES_NUM), MY_REFS.max_writes());

				MY_REFS.dump(out_data + '/' + std::to_string(int(idx / SAMPLES_NUM)) + ".json");
				MY_REFS.write_functions(json, int(idx / SAMPLES_NUM));
				MY_REFS.reset();
			}
			++idx;

			ull_t addr = std::stoull(saddr, 0, 16);
			int tid;
			try {
				tid = std::stoi(stid);
				MY_REFS.ref(tid, addr, line.get("source-location"),
					line.get("var-name"), line.get("type"), line.get("function"),
					line.get("alloc-location"), line.get("var-type"));
			}
			catch (const std::exception& e) {
				fprintf(stderr, "WARNING! exception happened: %s\n", e.what());
				if (0 == strcmp(e.what(), "stoi"))
					fprintf(stderr, "WARNING! wrong thread id: %s\n", stid.c_str());
			}
		}

		jsonfile info(out_data + "/info.json");
		info.start_collection();

		std::string fname = in_file;
		std::replace(fname.begin(), fname.end(), '.', '#');
		std::replace(fname.begin(), fname.end(), '\\', '@');
		std::replace(fname.begin(), fname.end(), '/', '@');

		info.write_to_collection("file", fname);
		info.write_to_collection("ver", REPORT_VER);
		info.write_to_collection("threadnum", std::to_string(MY_THREADS_NUM));
		info.end_collection();
	}

	timer.stop("ELAPSED");
	return;
}

int main(int argc, char *argv[]) {
	std::string ver(REPORT_VER);
	std::replace(ver.begin(), ver.end(), ' ', '.');
	printf("Ver: %s, %s, Nik Zaborovsky [nzaborov@sfu.ca], Oct-Nov, 2014\n\n", ver.c_str(), argv[0]);
	auto usage = []() {
		printf("Usage:\n"
			"\t analyzer [-f trace_file] [-d destination_folder] [-p trace_descr]\n"
			"\n"
			"\t <trace_file> - a file containing the access trace in the defined format,\n"
			"\t if omitted, stdin will be read;\n\n"
			"\t <destination_folder> - a folder to put the processed results in,\n"
			"\t if omitted, default folder \"data\" will be used;\n\n"
			"\t <trace_descr> - whatever info to be associated with the trace file.\n\n");
	};

	switch (argc) {
		case 3: case 5: case 7: break;
		default: usage(); return 0;
	}

	std::string source, destination, info;
	/*
	auto correct_str = [](const std::string& str) {
		unsigned i = 0;
		for(; i < str.size() && std::isalnum(str[i]); ++i);
		return (i == str.size() - 1);
	};
	*/
	bool bSrc = false, bDst = false, bInfo = false;
	for (int i = 1; i < argc; ++i) {
		if (bSrc) {
			source = argv[i];
			//if (!correct_str(source)) break;
			bSrc = false;
			continue;
		}
		if (bDst) {
			destination = argv[i];
			//if (!correct_str(destination)) break;
			bDst = false;
			continue;
		}
		if (bInfo) {
			info = argv[i];
			bInfo = false;
			continue;
		}
		switch (argv[i][0]) {
			case '-': break;
			default: usage(); return 0;
		};
		switch (argv[i][1]) {
		case 'f':
			bSrc = true;
			break;
		case 'd':
			bDst = true;
			break;
		case 'p':
			bInfo = true;
			break;
		default: usage(); return 0;
		};
	}
	if (bDst || bSrc || bInfo) { usage(); return 0; };

	if (!source.empty()) {
		printf("*running in src-file mode...\n");
		std::ifstream injson;
		injson.open(source);
		if (!injson.is_open()) {
			printf("Cannot open file %s\n", source.c_str());
			assert(false && "No input file found..\n");
			return 0;
		}
		processingBody(destination.empty() ? "server/data" : destination, injson, info.empty() ? source : info);
		injson.close();
	}
	else {
		printf("*running in stdin-mode...\n");
		processingBody(destination.empty() ? "server/data" : destination, std::cin, info);
	}
	getchar();
	return 1;
}
