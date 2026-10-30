#ifndef HIVECOCOS_DLL_TRANSPORT_H
#define HIVECOCOS_DLL_TRANSPORT_H

#include "HiveCocos/HiveTransport.h"

#include <mutex>
#include <string>

namespace hivecocos {

// Loads HIVE_PLUGIN_CPP.dll at runtime and talks to it through its three C exports:
//   char* HiveCPP_CallNative(const char* json)
//   void  HiveCPP_destroyCallNativeReturn(char* ret)
//   void  SetHiveSendMessageCallback(void (*cb)(const char* message))
// The DLL is loaded by absolute path with its own directory first in the dependency search order, so the
// rest of the SDK (HIVE_SERVICE.dll, ...) is found next to it. Windows only; elsewhere start() fails.
class DllTransport : public IHiveTransport
{
public:
    // dllDir: directory containing HIVE_PLUGIN_CPP.dll. Empty = the directory of the running executable.
    explicit DllTransport(std::wstring dllDir = std::wstring());
    ~DllTransport() override;

    bool start(MessageCallback onMessage, std::string* error) override;
    std::string call(const std::string& requestJson) override;
    void stop() override;

    // For diagnostics: full path that start() tries to load.
    std::wstring dllPath() const;

private:
    struct Impl;
    Impl* _impl;
    std::wstring _dllDir;
};

} // namespace hivecocos

#endif // HIVECOCOS_DLL_TRANSPORT_H
