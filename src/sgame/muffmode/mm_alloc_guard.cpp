// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// [MuffMode] Global allocation guard (diagnostic).
//
// Replaces this module's global operator new/delete so that any heap allocation
// that is either suspiciously large or actually fails is logged -- with the
// requested size, the game_x64.dll load base, and a native return-address
// backtrace -- BEFORE the resulting std::bad_alloc propagates.
//
// Log destinations (first writable wins):
//   1. Module directory next to game_x64.dll  (muffmode_alloc.log)
//   2. Process temp directory
//   3. Current working directory
// Every logged event is also mirrored to OutputDebugStringA (DebugView).
//
// On DLL load a LOADED breadcrumb is written so you can confirm the guard is
// active and see the resolved log path before any crash. Absence of that line
// after starting a build that includes this file means an old DLL is deployed
// or the process never loaded game_x64.dll.
//
// Why this exists: an intermittent "Standard exception caught in
// kexPlatformApp::Main: bad allocation" crash is caught by the engine, so no
// CRASHLOG.TXT / minidump is produced (especially under Wine/Proton). This gives
// us the throw site unattended, without a debugger. Resolve the logged
// "game_x64.dll+0x...." offsets against game_x64.map / the .pdb.
//
// Scope limit: only heap traffic through this module's global operator new is
// visible. Engine/quake2ex allocations and raw malloc elsewhere will still show
// the Kex dialog with no FAILED line here.
//
// This is intentionally a link-time global override (no public API / header). To
// disable, simply drop this file from the project. Overhead in the common path is
// a single size comparison.

#include "muffmode/mm_spawn_rules.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>

#if defined(_WIN32)
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
	#define NOMINMAX
	#endif
	#include <malloc.h>
	#include <windows.h>
#else
	#include <stdlib.h>
#endif

namespace {

// Nothing legitimate in the game module should ever request this much in one go;
// anything at/above it is logged even when it happens to succeed.
constexpr unsigned long long kSuspiciousAllocBytes = 256ull * 1024ull * 1024ull; // 256 MB

constexpr const char *kAllocLogFile = "muffmode_alloc.log";

// Allocation-free recursion guard. thread_local is zero-initialized before dynamic
// init, so it is safe during C++ static construction. Worst case across threads is
// a duplicated log line, never a crash or nested malloc inside the logger.
thread_local bool in_logger = false;

#if defined(_WIN32)
uintptr_t ModuleBase()
{
	static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA("game_x64.dll"));
	return base;
}

HMODULE SelfModule()
{
	HMODULE self = nullptr;
	GetModuleHandleExA(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCSTR>(&SelfModule), &self);
	return self;
}

// Loaded via GetProcAddress (kernel32, always linked) so we take no import-lib
// dependency on ntdll -- and it resolves fine under Wine.
using CaptureStackFn = USHORT(WINAPI *)(ULONG, ULONG, PVOID *, PULONG);

CaptureStackFn CaptureStack()
{
	static CaptureStackFn fn = []() -> CaptureStackFn {
		HMODULE ntdll = GetModuleHandleA("ntdll.dll");
		return ntdll ? reinterpret_cast<CaptureStackFn>(
			reinterpret_cast<void *>(GetProcAddress(ntdll, "RtlCaptureStackBackTrace"))) : nullptr;
	}();
	return fn;
}

bool ResolveLogPath(char *out, size_t out_cap)
{
	if (!out || out_cap == 0)
		return false;

	char module_path[MAX_PATH] = {};
	if (HMODULE self = SelfModule()) {
		const DWORD n = GetModuleFileNameA(self, module_path, MAX_PATH);
		if (n > 0 && n < MAX_PATH) {
			std::memcpy(out, module_path, n + 1);
			if (MM_ReplacePathFilename(out, out_cap, kAllocLogFile))
				return true;
		}
	}

	char temp_path[MAX_PATH] = {};
	const DWORD temp_len = GetTempPathA(MAX_PATH, temp_path);
	if (temp_len > 0 && temp_len < MAX_PATH) {
		if (MM_JoinDirectoryFile(out, out_cap, temp_path, kAllocLogFile))
			return true;
	}

	if (std::strlen(kAllocLogFile) + 1 <= out_cap) {
		std::memcpy(out, kAllocLogFile, std::strlen(kAllocLogFile) + 1);
		return true;
	}

	return false;
}
#endif

void EmitDebugLine(const char *line)
{
#if defined(_WIN32)
	OutputDebugStringA(line);
#else
	std::fputs(line, stderr);
#endif
}

void FormatTimestamp(char *ts, size_t ts_cap)
{
	if (!ts || ts_cap == 0)
		return;
	ts[0] = '\0';
	const std::time_t now = std::time(nullptr);
	std::tm tm_buf = {};
#if defined(_WIN32)
	localtime_s(&tm_buf, &now);
#else
	localtime_r(&now, &tm_buf);
#endif
	std::strftime(ts, ts_cap, "%Y-%m-%d %H:%M:%S", &tm_buf);
}

FILE *OpenAllocLog(char *log_path, size_t log_path_cap)
{
	FILE *f = nullptr;
	if (log_path && log_path_cap > 0)
		log_path[0] = '\0';
#if defined(_WIN32)
	if (log_path && log_path_cap > 0 && ResolveLogPath(log_path, log_path_cap))
		f = std::fopen(log_path, "ab");
	if (!f)
		f = std::fopen(kAllocLogFile, "ab");
#else
	(void)log_path;
	(void)log_path_cap;
	f = std::fopen(kAllocLogFile, "ab");
#endif
	return f;
}

#if defined(_WIN32)
// Local copy of PROCESS_MEMORY_COUNTERS so we can call K32GetProcessMemoryInfo
// via GetProcAddress without pulling in psapi.h / psapi.lib.
struct MmProcessMemoryCounters {
	DWORD cb;
	DWORD PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
};

void AppendMemorySnapshot(FILE *f)
{
	auto emit = [&](const char *line) {
		if (f)
			std::fputs(line, f);
		EmitDebugLine(line);
	};

	MEMORYSTATUSEX st = {};
	st.dwLength = sizeof(st);
	if (GlobalMemoryStatusEx(&st)) {
		char mem_line[256] = {};
		std::snprintf(mem_line, sizeof(mem_line),
			"    memory: load=%lu%% phys_avail=%llu/%llu virt_avail=%llu/%llu\n",
			st.dwMemoryLoad,
			static_cast<unsigned long long>(st.ullAvailPhys),
			static_cast<unsigned long long>(st.ullTotalPhys),
			static_cast<unsigned long long>(st.ullAvailVirtual),
			static_cast<unsigned long long>(st.ullTotalVirtual));
		emit(mem_line);
	}

	using GpmFn = BOOL(WINAPI *)(HANDLE, MmProcessMemoryCounters *, DWORD);
	static GpmFn gpm = []() -> GpmFn {
		HMODULE k32 = GetModuleHandleA("kernel32.dll");
		return k32 ? reinterpret_cast<GpmFn>(
			reinterpret_cast<void *>(GetProcAddress(k32, "K32GetProcessMemoryInfo"))) : nullptr;
	}();
	if (gpm) {
		MmProcessMemoryCounters pmc = {};
		pmc.cb = sizeof(pmc);
		if (gpm(GetCurrentProcess(), &pmc, sizeof(pmc))) {
			char proc_line[192] = {};
			std::snprintf(proc_line, sizeof(proc_line),
				"    process: working_set=%llu pagefile=%llu peak_ws=%llu\n",
				static_cast<unsigned long long>(pmc.WorkingSetSize),
				static_cast<unsigned long long>(pmc.PagefileUsage),
				static_cast<unsigned long long>(pmc.PeakWorkingSetSize));
			emit(proc_line);
		}
	}
}
#endif

// kind examples: "LOADED", "HUGE", "FAILED", "FAILED_ALIGNED"
// size_bytes is ignored for LOADED (pass 0).
void LogEvent(const char *kind, unsigned long long size_bytes, bool include_backtrace)
{
	if (in_logger)
		return;
	in_logger = true;

	char ts[32] = {};
	FormatTimestamp(ts, sizeof(ts));

	char header[192] = {};
	if (std::strcmp(kind, "LOADED") == 0) {
		std::snprintf(header, sizeof(header), "[%s] %s mm_alloc_guard active\n", ts, kind);
	} else {
		std::snprintf(header, sizeof(header), "[%s] %s alloc: %llu bytes (0x%llx)\n",
			ts, kind, size_bytes, size_bytes);
	}

	char log_path[512] = {};
	FILE *f = OpenAllocLog(log_path, sizeof(log_path));

	auto write_line = [&](const char *line) {
		if (f)
			std::fputs(line, f);
		// Always mirror so DebugView works even when the file write succeeds
		// (and still works when fopen fails).
		EmitDebugLine(line);
	};

	write_line(header);

#if defined(_WIN32)
	{
		char pid_line[64] = {};
		std::snprintf(pid_line, sizeof(pid_line), "    pid = %lu\n",
			static_cast<unsigned long>(GetCurrentProcessId()));
		write_line(pid_line);

		char base_line[96] = {};
		const uintptr_t base = ModuleBase();
		std::snprintf(base_line, sizeof(base_line), "    game_x64.dll base = 0x%llx\n",
			static_cast<unsigned long long>(base));
		write_line(base_line);

		if (log_path[0]) {
			char path_line[560] = {};
			std::snprintf(path_line, sizeof(path_line), "    log_path = %s\n", log_path);
			write_line(path_line);
		}

		AppendMemorySnapshot(f);

		if (include_backtrace) {
			if (CaptureStackFn capture = CaptureStack()) {
				void *frames[48] = {};
				// Skip LogEvent's own frame.
				const USHORT n = capture(1, 48, frames, nullptr);
				char count_line[64] = {};
				std::snprintf(count_line, sizeof(count_line), "    backtrace (%u frames):\n",
					static_cast<unsigned>(n));
				write_line(count_line);

				for (USHORT i = 0; i < n; i++) {
					const uintptr_t addr = reinterpret_cast<uintptr_t>(frames[i]);
					char frame_line[128] = {};
					if (base && addr >= base) {
						std::snprintf(frame_line, sizeof(frame_line),
							"      %2u  0x%llx   game_x64.dll+0x%llx\n",
							static_cast<unsigned>(i),
							static_cast<unsigned long long>(addr),
							static_cast<unsigned long long>(addr - base));
					} else {
						std::snprintf(frame_line, sizeof(frame_line),
							"      %2u  0x%llx\n",
							static_cast<unsigned>(i),
							static_cast<unsigned long long>(addr));
					}
					write_line(frame_line);
				}
			}
		}
	}
#else
	(void)include_backtrace;
#endif

	write_line("\n");

	if (f) {
		std::fflush(f);
		std::fclose(f);
	}

	in_logger = false;
}

void LogAlloc(const char *kind, unsigned long long size)
{
	LogEvent(kind, size, true);
}

struct AllocGuardBootstrap {
	AllocGuardBootstrap()
	{
		LogEvent("LOADED", 0, false);
	}
};

// Runs when game_x64.dll is loaded so muffmode_alloc.log exists before any crash.
AllocGuardBootstrap g_alloc_guard_bootstrap;

void *GuardedAlloc(std::size_t size, bool throw_on_fail)
{
	if (static_cast<unsigned long long>(size) >= kSuspiciousAllocBytes)
		LogAlloc("HUGE", size);

	void *p = std::malloc(size ? size : 1);
	if (!p) {
		LogAlloc("FAILED", size);
		if (throw_on_fail)
			throw std::bad_alloc();
	}
	return p;
}

void *GuardedAlignedAlloc(std::size_t size, std::size_t alignment, bool throw_on_fail)
{
	if (alignment < sizeof(void *))
		alignment = sizeof(void *);

	if (static_cast<unsigned long long>(size) >= kSuspiciousAllocBytes)
		LogAlloc("HUGE_ALIGNED", size);

#if defined(_WIN32)
	void *p = _aligned_malloc(size ? size : 1, alignment);
#else
	void *p = nullptr;
	if (posix_memalign(&p, alignment, size ? size : 1) != 0)
		p = nullptr;
#endif

	if (!p) {
		LogAlloc("FAILED_ALIGNED", size);
		if (throw_on_fail)
			throw std::bad_alloc();
	}
	return p;
}

void GuardedFree(void *p) noexcept
{
	std::free(p);
}

void GuardedAlignedFree(void *p) noexcept
{
	if (!p)
		return;
#if defined(_WIN32)
	_aligned_free(p);
#else
	std::free(p);
#endif
}

} // namespace

void *operator new(std::size_t size)   { return GuardedAlloc(size, true); }
void *operator new[](std::size_t size) { return GuardedAlloc(size, true); }

void *operator new(std::size_t size, const std::nothrow_t &) noexcept   { return GuardedAlloc(size, false); }
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept { return GuardedAlloc(size, false); }

void *operator new(std::size_t size, std::align_val_t alignment)
{
	return GuardedAlignedAlloc(size, static_cast<std::size_t>(alignment), true);
}

void *operator new[](std::size_t size, std::align_val_t alignment)
{
	return GuardedAlignedAlloc(size, static_cast<std::size_t>(alignment), true);
}

void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
	return GuardedAlignedAlloc(size, static_cast<std::size_t>(alignment), false);
}

void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
	return GuardedAlignedAlloc(size, static_cast<std::size_t>(alignment), false);
}

void operator delete(void *p) noexcept   { GuardedFree(p); }
void operator delete[](void *p) noexcept { GuardedFree(p); }

void operator delete(void *p, std::size_t) noexcept   { GuardedFree(p); }
void operator delete[](void *p, std::size_t) noexcept { GuardedFree(p); }

void operator delete(void *p, const std::nothrow_t &) noexcept   { GuardedFree(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { GuardedFree(p); }

void operator delete(void *p, std::align_val_t) noexcept   { GuardedAlignedFree(p); }
void operator delete[](void *p, std::align_val_t) noexcept { GuardedAlignedFree(p); }

void operator delete(void *p, std::size_t, std::align_val_t) noexcept   { GuardedAlignedFree(p); }
void operator delete[](void *p, std::size_t, std::align_val_t) noexcept { GuardedAlignedFree(p); }

void operator delete(void *p, std::align_val_t, const std::nothrow_t &) noexcept
{
	GuardedAlignedFree(p);
}

void operator delete[](void *p, std::align_val_t, const std::nothrow_t &) noexcept
{
	GuardedAlignedFree(p);
}
