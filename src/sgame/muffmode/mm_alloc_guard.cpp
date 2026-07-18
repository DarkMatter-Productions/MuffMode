// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// [MuffMode] Global allocation guard (diagnostic).
//
// Replaces this module's global operator new/delete so that any heap allocation
// that is either suspiciously large or actually fails is logged -- with the
// requested size, the game_x64.dll load base, and a native return-address
// backtrace -- to "muffmode_alloc.log" BEFORE the resulting std::bad_alloc
// propagates.
//
// Why this exists: an intermittent "Standard exception caught in
// kexPlatformApp::Main: bad allocation" crash is caught by the engine, so no
// CRASHLOG.TXT / minidump is produced (especially under Wine/Proton). This gives
// us the throw site unattended, without a debugger. Resolve the logged
// "game_x64.dll+0x...." offsets against game_x64.map / the .pdb.
//
// This is intentionally a link-time global override (no public API / header). To
// disable, simply drop this file from the project. Overhead in the common path is
// a single size comparison.

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <new>

#if defined(_WIN32)
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
	#define NOMINMAX
	#endif
	#include <windows.h>
#endif

namespace {

// Nothing legitimate in the game module should ever request this much in one go;
// anything at/above it is logged even when it happens to succeed.
constexpr unsigned long long kSuspiciousAllocBytes = 256ull * 1024ull * 1024ull; // 256 MB

constexpr const char *kAllocLogFile = "muffmode_alloc.log";

// The logger itself allocates (fopen); guard against re-entering it if that inner
// work also trips the size threshold or fails. Zero-initialized at load, so it is
// safe even during C++ static-init before TLS is up. Racy across threads by design
// -- the worst case is a duplicated log line, never a crash.
bool in_logger = false;

#if defined(_WIN32)
uintptr_t ModuleBase()
{
	static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA("game_x64.dll"));
	return base;
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
#endif

void LogAlloc(const char *kind, unsigned long long size)
{
	if (in_logger)
		return;
	in_logger = true;

	if (FILE *f = std::fopen(kAllocLogFile, "ab")) {
		char ts[32] = {};
		const std::time_t now = std::time(nullptr);
		std::tm tm_buf = {};
#if defined(_WIN32)
		localtime_s(&tm_buf, &now);
#else
		localtime_r(&now, &tm_buf);
#endif
		std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

		std::fprintf(f, "[%s] %s alloc: %llu bytes (0x%llx)\n", ts, kind, size, size);

#if defined(_WIN32)
		const uintptr_t base = ModuleBase();
		std::fprintf(f, "    game_x64.dll base = 0x%llx\n", static_cast<unsigned long long>(base));

		if (CaptureStackFn capture = CaptureStack()) {
			void *frames[48] = {};
			const USHORT n = capture(1, 48, frames, nullptr); // skip LogAlloc's own frame
			std::fprintf(f, "    backtrace (%u frames):\n", static_cast<unsigned>(n));
			for (USHORT i = 0; i < n; i++) {
				const uintptr_t addr = reinterpret_cast<uintptr_t>(frames[i]);
				std::fprintf(f, "      %2u  0x%llx", static_cast<unsigned>(i),
					static_cast<unsigned long long>(addr));
				if (base && addr >= base)
					std::fprintf(f, "   game_x64.dll+0x%llx",
						static_cast<unsigned long long>(addr - base));
				std::fputc('\n', f);
			}
		}
#endif
		std::fputc('\n', f);
		std::fflush(f);
		std::fclose(f);
	}

	in_logger = false;
}

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

} // namespace

void *operator new(std::size_t size)   { return GuardedAlloc(size, true); }
void *operator new[](std::size_t size) { return GuardedAlloc(size, true); }

void *operator new(std::size_t size, const std::nothrow_t &) noexcept   { return GuardedAlloc(size, false); }
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept { return GuardedAlloc(size, false); }

void operator delete(void *p) noexcept   { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t) noexcept   { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

void operator delete(void *p, const std::nothrow_t &) noexcept   { std::free(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { std::free(p); }
