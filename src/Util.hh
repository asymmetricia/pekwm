//
// Util.hh for pekwm
// Copyright (C) 2002-2021 Claes Nästén <pekdon@gmail.com>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#ifndef _PEKWM_UTIL_HH_
#define _PEKWM_UTIL_HH_

#include "config.h"

#include "Types.hh"

#include <algorithm>
#include <string>
#include <cstring>
#include <map>
#include <functional>
#include <sstream>
#include <vector>

extern "C" {
#include <string.h>
}

/**
 * String utilities, convenience functions for making life easier
 * when working with strings.
 */
namespace StringUtil {
	class Key {
	public:
		Key(const char *key);
		Key(const std::string &key);
		~Key(void);

		const std::string& str(void) const { return _key; }

		bool operator==(const std::string &rhs) const;
		bool operator!=(const std::string &rhs) const;
		bool operator<(const Key &rhs) const;
		bool operator>(const Key &rhs) const;

	private:
		std::string _key;
	};

	size_t safe_position(size_t pos, size_t fallback = 0, size_t add = 0);
	std::vector<std::string> shell_split(const std::string& str);

	int ascii_tolower(int chr);
	int ascii_ncase_cmp(const std::string &lhs, const std::string &rhs);
	int ascii_ncase_cmp(const std::string &lhs, const char *rhs);
	int ascii_ncase_cmp(const char *lhs, const std::string &rhs);
	int ascii_ncase_cmp(const char *lhs, const char *rhs);
	bool ascii_ncase_equal(const std::string &lhs, const std::string &rhs);
	bool ascii_ncase_equal(const std::string &lhs, const char *rhs);
	bool ascii_ncase_equal(const char *lhs, const std::string &rhs);
	bool ascii_ncase_equal(const char *lhs, const char *rhs);
}

class OverlapRange {
public:
	OverlapRange(int start, int num)
		: _start(start),
		  _num(num)
	{
		reset();
	}

	bool end(void) const {return _end; }

	void reset(void)
	{
		_end = false;
		_curr = _start;
	}

	int operator*(void) const { return _curr; }

	OverlapRange& operator++(void)
	{
		if (++_curr == _num) {
			_curr = 0;
		}
		_end = _curr == _start;
		return *this;
	}

private:
	int _start;
	int _curr;
	int _num;
	bool _end;
};

namespace Util {    
	template<typename T>
	class StringMap : public std::map<StringUtil::Key, T> {
	public:
		StringMap(void) { }
		virtual ~StringMap(void) { }

		T& get(const std::string& key) {
			typename StringMap<T>::iterator it = this->find(key);
			if (it == this->end()) {
				return this->operator[]("");
			}
			return it->second;
		}
	};

	std::string getEnv(const std::string& key);
	void setEnv(const std::string& key, const std::string &value);
	std::string getConfigDir(void);

	void forkExec(std::string command);
	pid_t forkExec(const std::vector<std::string>& args);
	std::string getHostname(void);
	bool setNonBlock(int fd);

	bool isFile(const std::string &file);
	bool isExecutable(const std::string &file);
	time_t getMtime(const std::string &file);

	bool copyTextFile(const std::string &from, const std::string &to);

	std::string getUserName(void);

	std::string getFileExt(const std::string &file);
	std::string getDir(const std::string &file);
	void expandFileName(std::string &file);

	const char* spaceChars(char escape);

	uint splitString(const std::string &str,
			 std::vector<std::string> &toks,
			 const char *sep, uint max = 0,
			 bool include_empty = false, char escape = 0);

	std::string to_string(void* v);
	void to_upper(std::string &str);
	void to_lower(std::string &str);

	/**
	 * Return value within bounds of min and max value.
	 */
	template<typename T>
	T between(T value, T min_val, T max_val) {
		if (value < min_val) {
			value = min_val;
		} else if (value > max_val) {
			value = max_val;
		}
		return value;
	}

	//! @brief Removes leading blanks( \n\t) from string.
	inline void trimLeadingBlanks(std::string &trim) {
		std::string::size_type first = trim.find_first_not_of(" \n\t");
		if ((first != std::string::npos) &&
		    (first != (std::string::size_type) trim[0])) {
			trim = trim.substr(first, trim.size() - first);
		}
	}

	bool isTrue(const std::string &value);

	//! @brief for_each delete utility.
	template<class T> struct Free : public std::unary_function<T, void> {
		void operator ()(T t) { delete t; }
	};

	template<typename T>
	struct StringTo {
		const char *name;
		T value;
	};

	template<typename T>
	T StringToGet(const Util::StringTo<T> *map, const std::string& key)
	{
		int i = 0;
		for (; map[i].name != nullptr; i++) {
			if (StringUtil::ascii_ncase_equal(map[i].name, key)) {
				return map[i].value;
			}
		}
		return map[i].value;
	}

}

#endif // _PEKWM_UTIL_HH_
