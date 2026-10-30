#ifndef HIVECOCOS_HIVE_AUTHV4_H
#define HIVECOCOS_HIVE_AUTHV4_H

#include "HiveCocos/HiveBridge.h"

#include <functional>
#include <string>
#include <vector>

namespace hivecocos {

// "AuthV4" class of the native bridge (setup only for now).
class HiveAuthV4
{
public:
    struct SetupResult
    {
        bool delivered = false;                 // false: the request could not be sent
        HiveResultAPI result;
        bool isAutoSignIn = false;
        std::string did;
        std::vector<std::string> providerTypeList;   // e.g. "GOOGLE", "GUEST"
    };
    using SetupCallback = std::function<void(const SetupResult&)>;

    explicit HiveAuthV4(HiveBridge& bridge) : _bridge(bridge) {}

    // Asynchronous. The callback runs through the bridge's dispatcher. When the request cannot be sent, the
    // callback is invoked immediately with delivered == false.
    void setup(SetupCallback callback);

    // Synchronous. Call before the process exits.
    bool terminateProcess();

    static SetupResult parseSetupReply(const HiveResponse& reply);

private:
    HiveBridge& _bridge;
};

} // namespace hivecocos

#endif // HIVECOCOS_HIVE_AUTHV4_H
