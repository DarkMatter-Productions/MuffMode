// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <string>
#include <vector>

struct fake_command_import_t {
	std::vector<std::string> argv_storage;
	std::string args_storage;

	void set_args(std::initializer_list<const char *> args) {
		argv_storage.clear();
		args_storage.clear();

		for (const char *arg : args)
			argv_storage.emplace_back(arg ? arg : "");

		for (size_t i = 1; i < argv_storage.size(); i++) {
			if (i > 1)
				args_storage += ' ';
			args_storage += argv_storage[i];
		}
	}

	int argc() const {
		return static_cast<int>(argv_storage.size());
	}

	const char *argv(int index) const {
		if (index < 0 || static_cast<size_t>(index) >= argv_storage.size())
			return "";

		return argv_storage[static_cast<size_t>(index)].c_str();
	}

	const char *args() const {
		return args_storage.c_str();
	}
};
