#include "HiveCocos/DllTransport.h"

#include <utility>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace hivecocos {

namespace {

typedef char* (*CallNativeFn)(const char*);
typedef void (*DestroyReturnFn)(char*);
typedef void(__stdcall* NativeMessageFn)(const char*);
typedef void(__stdcall* SetCallbackFn)(NativeMessageFn);

std::string toUtf8(const std::wstring& w)
{
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

std::string win32Message(DWORD code)
{
    wchar_t buf[512] = {};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, buf,
                   static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])), nullptr);
    std::wstring w(buf);
    while (!w.empty() && (w.back() == L'\r' || w.back() == L'\n' || w.back() == L' ')) w.pop_back();
    return toUtf8(w);
}

std::wstring exeDirectory()
{
    wchar_t path[MAX_PATH * 2] = {};
    const DWORD n = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(sizeof(path) / sizeof(path[0])));
    std::wstring p(path, n);
    const size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : p.substr(0, slash);
}

// The SDK holds one global callback; so does this trampoline (one DllTransport at a time).
std::mutex g_callbackMutex;
IHiveTransport::MessageCallback g_callback;

void __stdcall nativeMessageTrampoline(const char* message)
{
    if (!message) return;
    IHiveTransport::MessageCallback cb;
    {
        std::lock_guard<std::mutex> lock(g_callbackMutex);
        cb = g_callback;
    }
    if (cb) cb(std::string(message));
}

} // namespace

struct DllTransport::Impl
{
    HMODULE module = nullptr;
    CallNativeFn callNative = nullptr;
    DestroyReturnFn destroyReturn = nullptr;
    SetCallbackFn setCallback = nullptr;
};

DllTransport::DllTransport(std::wstring dllDir)
    : _impl(new Impl)
    , _dllDir(dllDir.empty() ? exeDirectory() : std::move(dllDir))
{
}

DllTransport::~DllTransport()
{
    stop();
    delete _impl;
}

std::wstring DllTransport::dllPath() const
{
    return _dllDir + L"\\HIVE_PLUGIN_CPP.dll";
}

bool DllTransport::start(MessageCallback onMessage, std::string* error)
{
    auto fail = [&](const std::string& msg) {
        if (error) *error = msg;
        return false;
    };
    if (_impl->module) return true;

    const std::wstring path = dllPath();
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return fail("HIVE_PLUGIN_CPP.dll not found: " + toUtf8(path));

    // LOAD_WITH_ALTERED_SEARCH_PATH: dependent DLLs are searched in the DLL's own directory first.
    HMODULE m = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m)
    {
        const DWORD e = GetLastError();
        std::string msg = "LoadLibrary failed (" + std::to_string(e) + "): " + win32Message(e) + " [" + toUtf8(path) + "]";
        if (e == ERROR_BAD_EXE_FORMAT)
            msg += " - the Hive SDK DLLs are x64 only; this process must be a 64-bit build";
        return fail(msg);
    }

    _impl->callNative = reinterpret_cast<CallNativeFn>(GetProcAddress(m, "HiveCPP_CallNative"));
    _impl->destroyReturn = reinterpret_cast<DestroyReturnFn>(GetProcAddress(m, "HiveCPP_destroyCallNativeReturn"));
    _impl->setCallback = reinterpret_cast<SetCallbackFn>(GetProcAddress(m, "SetHiveSendMessageCallback"));
    if (!_impl->callNative || !_impl->destroyReturn || !_impl->setCallback)
    {
        FreeLibrary(m);
        *_impl = Impl();
        return fail("HIVE_PLUGIN_CPP.dll does not export the expected functions");
    }

    _impl->module = m;
    {
        std::lock_guard<std::mutex> lock(g_callbackMutex);
        g_callback = std::move(onMessage);
    }
    _impl->setCallback(nativeMessageTrampoline);
    return true;
}

std::string DllTransport::call(const std::string& requestJson)
{
    if (!_impl->module) return std::string();
    char* res = _impl->callNative(requestJson.c_str());
    if (!res) return std::string();
    std::string out(res);
    _impl->destroyReturn(res);      // allocated with new[] inside the DLL
    return out;
}

void DllTransport::stop()
{
    // The DLL stays loaded: the SDK may still call the trampoline from its threads, and unloading would
    // leave it calling freed code. Only detach our callback.
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_callback = nullptr;
}

} // namespace hivecocos

#else // !_WIN32

namespace hivecocos {

struct DllTransport::Impl {};
DllTransport::DllTransport(std::wstring dllDir) : _impl(new Impl), _dllDir(std::move(dllDir)) {}
DllTransport::~DllTransport() { delete _impl; }
std::wstring DllTransport::dllPath() const { return std::wstring(); }
bool DllTransport::start(MessageCallback, std::string* error)
{
    if (error) *error = "the Hive SDK native DLL is only available on Windows";
    return false;
}
std::string DllTransport::call(const std::string&) { return std::string(); }
void DllTransport::stop() {}

} // namespace hivecocos

#endif
