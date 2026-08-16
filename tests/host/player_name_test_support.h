// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <string>
#include <string_view>

struct mm_host_player_name_buffers_t {
	std::string_view submitted;
	std::string_view persistent;
	bool submitted_terminated = true;
	bool persistent_terminated = true;
	bool bot = false;
};

std::string MM_HostPlayerDisplayName(
	const mm_host_player_name_buffers_t &buffers);
std::string MM_HostPlayerDisplayNameCString(
	const mm_host_player_name_buffers_t &buffers);
std::string MM_HostPlayerLocalizationName(
	const mm_host_player_name_buffers_t &buffers);
