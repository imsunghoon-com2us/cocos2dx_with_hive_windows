#ifndef __HIVE_SERVICE_H__
#define __HIVE_SERVICE_H__

#include "HiveCocos/HiveAuthV4.h"

#include <functional>
#include <memory>
#include <string>

// Starts the Hive SDK (Windows, x64), signs in/out, deletes the account, and keeps one-line statuses for the title
// screen. Cocos thread only.
//
// Flow: start() -> AuthV4.setup -> sign-in. The sign-in asks isAutoSignIn(): when true the stored session is used
// (signIn "AUTO"); when that fails, or when isAutoSignIn() is false, the Hive sign-in UI (showSignIn) is shown.
//
// Failure policy: a failed initialization or sign-in (cancelling the Hive UI counts as a failure) raises a Notice. The
// UI shows it as a popup and calls acknowledgeNotice(): a non-fatal notice then retries (at most kMaxRetries retries
// per stage), a fatal one (retries used up) ends the game. Initialization and sign-in count their retries separately,
// and a success resets the counter.
//
// Optional overrides, read from environment variables so no credentials live in the repository
// (without them the SDK uses Resources/hive_config.xml next to the executable, see tools/setup_hive.ps1):
//   HIVE_APP_ID, HIVE_ZONE (SANDBOX|REAL), HIVE_SERVER_ID, HIVE_GAME_LANGUAGE
//   HIVE_BRIDGE_ENGINE, HIVE_BRIDGE_PLATFORM   ("engine"/"platform" fields of bridge requests)
class HiveService
{
public:
    static const int kMaxRetries = 2;

    struct Notice
    {
        std::string text;
        bool fatal = false;     // OK ends the game instead of retrying
    };

    // Result of sign-out / account deletion: ok, and a short reason when it failed.
    using ResultCallback = std::function<void(bool ok, const std::string& reason)>;

    static HiveService& instance();

    void start();               // idempotent: begins initialization (and the sign-in after it)
    void shutdown();

    const std::string& status() const { return _status; }               // SDK / setup line
    const std::string& loginStatus() const { return _loginStatus; }     // login line (empty until setup is ready)
    bool isBusy() const { return _busy; }                               // setup, sign-in, sign-out or deletion in progress
    bool isSignedIn() const { return _signedIn; }
    // Signed in with a linked (non-guest) account. A guest cannot sign in again after signOut, and when the reply did
    // not say which providers are linked the player is treated like a guest (the safe side).
    bool canSignOut() const;
    bool isGuest() const { return _signedIn && !canSignOut(); }
    std::string playerLabel() const;                                    // "player <id>" while signed in

    // A popup the UI has to show. acknowledgeNotice() is called when the player pressed OK.
    bool hasNotice() const { return _hasNotice; }
    const Notice& notice() const { return _notice; }
    void acknowledgeNotice();

    // Starts a (fresh) sign-in attempt, e.g. after sign-out. Ignored while busy, signed in, or a notice is waiting.
    void requestSignIn();
    // Both ignore the call (callback with ok == false) unless allowed: sign-out needs canSignOut(), deletion needs a
    // signed-in player. The callback runs on the cocos thread.
    void requestSignOut(ResultCallback done);
    void requestDeleteAccount(ResultCallback done);

    // Test aid (the "Reset terms" menu entry): AuthV4.resetAgreement, then sign out (a linked account) or delete the
    // account (a guest: the caller must have asked the player first), then run the initialization (setup) again,
    // which is what brings the terms back. The callback gets ok == true once the sign-out / deletion succeeded and
    // the initialization has been started again (it continues on its own, with the usual failure policy). Refused
    // (ok == false) when not signed in or when the account type is unknown (it could be a linked account that must
    // not be deleted).
    void requestResetTerms(ResultCallback done);

    enum class AccountKind { None, Unknown, Guest, Linked };     // None: not signed in
    AccountKind accountKind() const;

private:
    enum class Stage { Init, SignIn };

    HiveService() = default;
    void setStatus(const std::string& status);
    void setLoginStatus(const std::string& status);
    void beginInit();
    void reinitialize();        // runs the initialization (setup) again after sign-out / account deletion
    void beginSignIn();
    void showSignInUi();
    void onSignInResult(const hivecocos::HiveAuthV4::SignInResult& result);
    void fail(Stage stage, const std::string& text);
    void resetPlayer();

    std::shared_ptr<hivecocos::HiveBridge> _bridge;
    std::string _status = "Hive: not started";
    std::string _loginStatus;
    std::string _sdkVersion;
    bool _started = false;
    bool _configured = false;
    bool _setupReady = false;
    bool _busy = false;
    bool _signedIn = false;
    hivecocos::HivePlayerInfo _player;

    int _initRetries = 0;
    int _signInRetries = 0;
    bool _hasNotice = false;
    Notice _notice;
    std::function<void()> _afterNotice;     // retry to run when a non-fatal notice is acknowledged
};

#endif // __HIVE_SERVICE_H__
