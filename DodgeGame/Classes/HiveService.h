#ifndef __HIVE_SERVICE_H__
#define __HIVE_SERVICE_H__

#include <memory>
#include <string>

namespace hivecocos { class HiveBridge; }

// Starts the Hive SDK (Windows, x64) and keeps a one-line status for the HUD. Cocos thread only.
//
// Optional overrides, read from environment variables so no credentials live in the repository
// (without them the SDK uses Resources/hive_config.xml next to the executable):
//   HIVE_APP_ID, HIVE_ZONE (SANDBOX|REAL), HIVE_SERVER_ID, HIVE_GAME_LANGUAGE
//   HIVE_BRIDGE_ENGINE, HIVE_BRIDGE_PLATFORM   ("engine"/"platform" fields of bridge requests)
class HiveService
{
public:
    static HiveService& instance();

    void start();
    void shutdown();

    const std::string& status() const { return _status; }

private:
    HiveService() = default;
    void setStatus(const std::string& status);

    std::shared_ptr<hivecocos::HiveBridge> _bridge;
    std::string _status = "Hive: not started";
};

#endif // __HIVE_SERVICE_H__
