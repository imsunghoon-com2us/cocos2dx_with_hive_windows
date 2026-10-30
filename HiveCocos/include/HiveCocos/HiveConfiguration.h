#ifndef HIVECOCOS_HIVE_CONFIGURATION_H
#define HIVECOCOS_HIVE_CONFIGURATION_H

#include "HiveCocos/HiveBridge.h"

#include <string>

namespace hivecocos {

// "Configuration" class of the native bridge. Every call is synchronous. The request parameter key and the
// reply key both equal the method name (e.g. setAppId -> {"setAppId": "..."}, getAppId -> {"getAppId": "..."}).
class HiveConfiguration
{
public:
    explicit HiveConfiguration(HiveBridge& bridge) : _bridge(bridge) {}

    bool setAppId(const std::string& appId);
    bool setZone(HiveZone zone);
    bool setServerId(const std::string& serverId);
    bool setGameLanguage(const std::string& language);
    bool setUseLog(bool useLog);
    bool setHiveCertificationKey(const std::string& appKey);   // parameter key is "appKey"

    std::string getHiveSDKVersion();
    std::string getAppId();
    HiveZone getZone();
    std::string getGameLanguage();

private:
    bool call(const char* method, const HiveParams& params);
    std::string getString(const char* method);

    HiveBridge& _bridge;
};

} // namespace hivecocos

#endif // HIVECOCOS_HIVE_CONFIGURATION_H
