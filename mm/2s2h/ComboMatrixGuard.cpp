// mm/2s2h/ComboMatrixGuard.cpp
// ComboShip: the reporting half of the matrix stack guard in sys_matrix.c. A push past the top of the stack, a pop past
// its bottom, or pushes still on the stack when the next frame starts are logged once per calling address, as
// "<module>+0x<offset>": the offset can be looked up in that build's binary (its .pdata / symbols) to name the
// function.
#ifdef COMBO_BUILD

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

// "2ship.dll+0x46c706": the module the address is in and the offset from where it was loaded.
std::string DescribeAddress(void* addr) {
    char buf[320];
#ifdef _WIN32
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &mod) &&
        (mod != nullptr)) {
        char path[MAX_PATH] = { 0 };
        GetModuleFileNameA(mod, path, MAX_PATH);
        const char* name = path;
        for (const char* p = path; *p != '\0'; p++) {
            if ((*p == '\\') || (*p == '/')) {
                name = p + 1;
            }
        }
        snprintf(buf, sizeof(buf), "%s+0x%llx", name, (unsigned long long)((uintptr_t)addr - (uintptr_t)mod));
        return buf;
    }
#else
    Dl_info info;
    if ((dladdr(addr, &info) != 0) && (info.dli_fbase != nullptr)) {
        const char* name = (info.dli_fname != nullptr) ? info.dli_fname : "?";
        for (const char* p = name; *p != '\0'; p++) {
            if (*p == '/') {
                name = p + 1;
            }
        }
        snprintf(buf, sizeof(buf), "%s+0x%llx", name,
                 (unsigned long long)((uintptr_t)addr - (uintptr_t)info.dli_fbase));
        return buf;
    }
#endif
    snprintf(buf, sizeof(buf), "0x%llx", (unsigned long long)(uintptr_t)addr);
    return buf;
}

std::mutex sMutex;
std::set<void*> sReported; // one line per calling address, however often it happens

bool FirstTime(void* addr) {
    std::lock_guard<std::mutex> lock(sMutex);
    return sReported.insert(addr).second;
}

} // namespace

// kind 0: a push with the stack full (refused); kind 1: a pop with the stack empty (refused).
extern "C" void Combo_Matrix_ReportFault(int32_t kind, void* caller) {
    if (!FirstTime(caller)) {
        return;
    }
    SPDLOG_ERROR("[ComboShip] matrix stack {}: refused, from {}",
                 (kind == 0) ? "overflow (push with the stack full)" : "underflow (pop with the stack empty)",
                 DescribeAddress(caller));
}

// `depth` pushes were left on the stack at the start of a frame; `callers` are the ones the stack held (bottom first).
extern "C" void Combo_Matrix_ReportLeak(int32_t depth, void* const* callers, int32_t numCallers) {
    for (int32_t i = 0; i < numCallers; i++) {
        if (FirstTime(callers[i])) {
            SPDLOG_ERROR("[ComboShip] matrix stack: {} push(es) not popped by the end of the frame; one from {}", depth,
                         DescribeAddress(callers[i]));
        }
    }
}

#endif // COMBO_BUILD
