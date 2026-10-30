#include "HiveService.h"

#include "HiveCocos/CocosDispatcher.h"
#include "HiveCocos/DllTransport.h"
#include "HiveCocos/HiveBridge.h"
#include "HiveCocos/HiveConfiguration.h"

#include "cocos2d.h"

#include <cstdlib>

USING_NS_CC;
using namespace hivecocos;

namespace
{
std::string getEnv(const char* name)
{
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || !value) return std::string();
    std::string s(value);
    free(value);
    return s;
}
}

HiveService& HiveService::instance()
{
    static HiveService s;
    return s;
}

void HiveService::setStatus(const std::string& status)
{
    _status = status;
    log("[Hive] %s", status.c_str());
}

void HiveService::setLoginStatus(const std::string& status)
{
    _loginStatus = status;
    log("[Hive] %s", status.c_str());
}

void HiveService::start()
{
    if (_bridge) return;

    HiveBridge::Options options;
    if (!getEnv("HIVE_BRIDGE_ENGINE").empty()) options.engine = getEnv("HIVE_BRIDGE_ENGINE");
    if (!getEnv("HIVE_BRIDGE_PLATFORM").empty()) options.platform = getEnv("HIVE_BRIDGE_PLATFORM");

    auto bridge = std::make_shared<HiveBridge>(std::make_shared<DllTransport>(), makeCocosDispatcher(), options);
    std::string error;
    if (!bridge->start(&error))
    {
        log("[Hive] %s", error.c_str());
        setStatus("Hive: SDK unavailable");
        return;
    }
    _bridge = bridge;

    HiveConfiguration config(*_bridge);
    const std::string appId = getEnv("HIVE_APP_ID");
    if (!appId.empty()) config.setAppId(appId);
    const std::string zone = getEnv("HIVE_ZONE");
    if (!zone.empty()) config.setZone(zone == "SANDBOX" ? HiveZone::Sandbox : HiveZone::Real);
    const std::string serverId = getEnv("HIVE_SERVER_ID");
    if (!serverId.empty()) config.setServerId(serverId);
    const std::string language = getEnv("HIVE_GAME_LANGUAGE");
    if (!language.empty()) config.setGameLanguage(language);

    const std::string version = config.getHiveSDKVersion();
    if (version.empty())
    {
        setStatus("Hive: no reply from SDK");
        return;
    }
    setStatus("Hive " + version + ": setup...");

    HiveAuthV4(*_bridge).setup([this, version](const HiveAuthV4::SetupResult& result) {
        if (!result.delivered)
        {
            setStatus("Hive " + version + ": setup request failed");
            return;
        }
        if (!result.result.isSuccess())
        {
            log("[Hive] setup failed: %s", result.result.toString().c_str());
            setStatus("Hive " + version + ": setup failed (code " + std::to_string(result.result.code) + ")");
            return;
        }

        _setupReady = true;
        setStatus("Hive " + version + ": ready (providers " + std::to_string(result.providerTypeList.size()) + ")");

        if (result.isAutoSignIn)
        {
            // A stored session exists: sign in with it without showing any UI.
            beginLogin("Login: auto sign-in...");
            HiveAuthV4(*_bridge).signIn("AUTO", [this](const HiveAuthV4::SignInResult& r) { onSignInResult(r, true); });
        }
        else
        {
            setLoginStatus("Login: press L to sign in");
        }
    });
}

void HiveService::beginLogin(const std::string& pendingText)
{
    _loginPending = true;
    setLoginStatus(pendingText);
}

void HiveService::onSignInResult(const HiveAuthV4::SignInResult& result, bool autoAttempt)
{
    _loginPending = false;

    if (!result.delivered)
    {
        setLoginStatus("Login: request failed");
        return;
    }
    // Never log tokens: HivePlayerInfo::toString() leaves them out.
    log("[Hive] sign-in reply: %s | %s", result.result.toString().c_str(), result.playerInfo.toString().c_str());

    if (result.result.isSuccess())
    {
        _signedIn = true;
        setLoginStatus("Login: signed in (player " + std::to_string(result.playerInfo.playerId) +
                       (result.playerInfo.isNewUser ? ", new user" : "") + ") - O: sign out");
    }
    else
    {
        _signedIn = false;
        setLoginStatus(std::string("Login: ") + (autoAttempt ? "auto sign-in failed" : "failed") +
                       " (code " + std::to_string(result.result.code) + ") - press L to sign in");
    }
}

void HiveService::requestSignIn()
{
    if (!_bridge || !_setupReady || _loginPending || _signedIn) return;

    beginLogin("Login: Hive sign-in UI...");
    HiveAuthV4(*_bridge).showSignIn([this](const HiveAuthV4::SignInResult& r) { onSignInResult(r, false); });
}

void HiveService::requestSignOut()
{
    if (!_bridge || !_setupReady || _loginPending || !_signedIn) return;

    beginLogin("Login: signing out...");
    HiveAuthV4(*_bridge).signOut([this](const HiveAuthV4::SignOutResult& r) {
        _loginPending = false;
        if (r.delivered && r.result.isSuccess())
        {
            _signedIn = false;
            setLoginStatus("Login: signed out - press L to sign in");
        }
        else
        {
            log("[Hive] sign-out reply: %s", r.result.toString().c_str());
            setLoginStatus("Login: sign-out failed (code " + std::to_string(r.result.code) + ")");
        }
    });
}

void HiveService::shutdown()
{
    if (!_bridge) return;
    HiveAuthV4(*_bridge).terminateProcess();
    _bridge->shutdown();
    _bridge.reset();
}
