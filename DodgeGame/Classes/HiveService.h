#ifndef __HIVE_SERVICE_H__
#define __HIVE_SERVICE_H__

#include "HiveCocos/HiveAuthV4.h"

#include <memory>
#include <string>

// Starts the Hive SDK (Windows, x64), signs in/out, and keeps one-line statuses for the HUD. Cocos thread only.
//
// Flow: start() -> AuthV4.setup. When setup reports isAutoSignIn the stored session is used (signIn "AUTO");
// otherwise the player presses L (requestSignIn -> Hive sign-in UI). requestSignOut signs out.
//
// Optional overrides, read from environment variables so no credentials live in the repository
// (without them the SDK uses Resources/hive_config.xml next to the executable, see tools/setup_hive.ps1):
//   HIVE_APP_ID, HIVE_ZONE (SANDBOX|REAL), HIVE_SERVER_ID, HIVE_GAME_LANGUAGE
//   HIVE_BRIDGE_ENGINE, HIVE_BRIDGE_PLATFORM   ("engine"/"platform" fields of bridge requests)
class HiveService
{
public:
    static HiveService& instance();

    void start();
    void shutdown();

    const std::string& status() const { return _status; }               // SDK / setup line
    const std::string& loginStatus() const { return _loginStatus; }     // login line (empty until setup is ready)
    bool isLoginPending() const { return _loginPending; }
    bool isSignedIn() const { return _signedIn; }

    // Key actions. Ignored unless setup finished successfully and no login is in progress.
    void requestSignIn();       // shows the Hive sign-in UI
    void requestSignOut();

private:
    HiveService() = default;
    void setStatus(const std::string& status);
    void setLoginStatus(const std::string& status);
    void beginLogin(const std::string& pendingText);
    void onSignInResult(const hivecocos::HiveAuthV4::SignInResult& result, bool autoAttempt);

    std::shared_ptr<hivecocos::HiveBridge> _bridge;
    std::string _status = "Hive: not started";
    std::string _loginStatus;
    bool _setupReady = false;
    bool _loginPending = false;
    bool _signedIn = false;
};

#endif // __HIVE_SERVICE_H__
