#pragma once

#include <string>
#include <fstream>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <map>


struct jsonreader {
	struct line_t {
		line_t() {};
		std::string get(const char *const key) const {
			try {
				return _dict.at(key);
			}
			catch (...) {
				return std::string();
			}
		};
		const std::string& get(const char *const key) {
			return _dict[key];
		};
	private:
		std::map <std::string, std::string> _dict;
		void add_param(const std::string& key, const std::string& value) {
			_dict[key] = value;
		}
		line_t(const line_t&);
		friend struct jsonreader;
	};

	jsonreader(std::istream &istream) : _json(istream) {};

	bool readline(line_t &line_rep) {
		std::string line;
		line_rep._dict.clear();
		std::getline(_json, line);
		if (line.empty())
			return false;

		std::stringstream s(line);
		std::string lexem;
		std::string key, value;
		while (std::getline(s, lexem, ',')) {
			if (lexem.empty()) continue;

			std::stringstream params(lexem);
			// \"key\": \"value\"
			std::getline(params, key, '\"');
			std::getline(params, key, '\"');
			std::getline(params, value, '\"');
			std::getline(params, value, '\"');

			line_rep.add_param(key, value);
		}
		return true;
	}

	~jsonreader() {
	}
private:
	std::istream& _json;
};