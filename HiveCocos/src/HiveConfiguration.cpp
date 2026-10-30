#include "HiveCocos/HiveConfiguration.h"

namespace hivecocos {

bool HiveConfiguration::call(const char* method, const HiveParams& params)
{
    return _bridge.callSync("Configuration", method, params).ok;
}

std::string HiveConfiguration::getString(const char* method)
{
    HiveResponse r = _bridge.callSync("Configuration", method);
    return r.ok ? r.getString(method) : std::string();
}

bool HiveConfiguration::setAppId(const std::string& appId)
{
    return call("setAppId", { { "setAppId", appId } });
}

bool HiveConfiguration::setZone(HiveZone zone)
{
    return call("setZone", { { "setZone", zone == HiveZone::Sandbox ? "SANDBOX" : "REAL" } });
}

bool HiveConfiguration::setServerId(const std::string& serverId)
{
    return call("setServerId", { { "setServerId", serverId } });
}

bool HiveConfiguration::setGameLanguage(const std::string& language)
{
    return call("setGameLanguage", { { "setGameLanguage", language } });
}

bool HiveConfiguration::setUseLog(bool useLog)
{
    return call("setUseLog", { { "setUseLog", useLog } });
}

bool HiveConfiguration::setHiveCertificationKey(const std::string& appKey)
{
    return call("setHiveCertificationKey", { { "appKey", appKey } });
}

std::string HiveConfiguration::getHiveSDKVersion() { return getString("getHiveSDKVersion"); }
std::string HiveConfiguration::getAppId() { return getString("getAppId"); }
std::string HiveConfiguration::getGameLanguage() { return getString("getGameLanguage"); }

HiveZone HiveConfiguration::getZone()
{
    return getString("getZone") == "SANDBOX" ? HiveZone::Sandbox : HiveZone::Real;
}

} // namespace hivecocos
