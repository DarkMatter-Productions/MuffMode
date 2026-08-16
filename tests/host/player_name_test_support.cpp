// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_player_name.h"
#include "player_name_test_support.h"

#include <algorithm>
#include <cstring>

namespace {

template<size_t Size>
void SetNameBuffer(
	char (&buffer)[Size], std::string_view value, bool terminated)
{
	std::fill(std::begin(buffer), std::end(buffer), terminated ? '\0' : 'X');
	const size_t limit = terminated ? Size - 1 : Size;
	const size_t count = std::min(value.size(), limit);
	if (count)
		std::memcpy(buffer, value.data(), count);
	if (terminated)
		buffer[count] = '\0';
}

gclient_t MakeClient(const mm_host_player_name_buffers_t &buffers)
{
	gclient_t client{};
	SetNameBuffer(client.resp.netname, buffers.submitted,
		buffers.submitted_terminated);
	SetNameBuffer(client.pers.netname, buffers.persistent,
		buffers.persistent_terminated);
	client.sess.is_a_bot = buffers.bot;
	return client;
}

} // namespace

std::string MM_HostPlayerDisplayName(
	const mm_host_player_name_buffers_t &buffers)
{
	const gclient_t client = MakeClient(buffers);
	return std::string(MM_PlayerDisplayName(&client));
}

std::string MM_HostPlayerDisplayNameCString(
	const mm_host_player_name_buffers_t &buffers)
{
	const gclient_t client = MakeClient(buffers);
	return MM_PlayerDisplayNameCString(&client);
}

std::string MM_HostPlayerLocalizationName(
	const mm_host_player_name_buffers_t &buffers)
{
	const gclient_t client = MakeClient(buffers);
	return MM_PlayerLocalizationName(&client);
}
