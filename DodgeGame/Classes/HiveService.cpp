#include "HiveService.h"

#include "HiveCocos/CocosDispatcher.h"
#include "HiveCocos/DllTransport.h"
#include "HiveCocos/HiveBridge.h"
#include "HiveCocos/HiveConfiguration.h"

#include "cocos2d.h"

#include <cstdlib>
#include <tlhelp32.h>

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

// The Hive UI runs in CEF child processes (cef.subprocess.exe). After a normal exit they were seen to stay alive for
// minutes (2+ minutes measured): they show up in the task manager and keep files of the build folder locked. At exit
// they are still our children, so only those are terminated here.
void terminateCefChildren()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    const DWORD self = GetCurrentProcessId();
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    int terminated = 0;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (entry.th32ParentProcessID == self && _wcsicmp(entry.szExeFile, L"cef.subprocess.exe") == 0)
            {
                HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                if (process)
                {
                    if (TerminateProcess(process, 0)) ++terminated;
                    CloseHandle(process);
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    log("[Hive] terminated %d cef.subprocess child process(es)", terminated);
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

HiveService::AccountKind HiveService::accountKind() const
{
    if (!_signedIn) return AccountKind::None;
    if (!_player.providerInfoKnown) return AccountKind::Unknown;
    return _player.connectedProviders.empty() ? AccountKind::Guest : AccountKind::Linked;
}

bool HiveService::canSignOut() const
{
    return accountKind() == AccountKind::Linked;
}

std::string HiveService::playerLabel() const
{
    return _signedIn ? "player " + std::to_string(_player.playerId) : std::string();
}

void HiveService::resetPlayer()
{
    _signedIn = false;
    _player = HivePlayerInfo();
}

void HiveService::start()
{
    if (_started) return;
    _started = true;
    beginInit();
}

// ---------------------------------------------------------------- failure policy
void HiveService::fail(Stage stage, const std::string& reason)
{
    _busy = false;
    const bool init = (stage == Stage::Init);
    int& retries = init ? _initRetries : _signInRetries;
    const std::string head = init ? "Hive initialization failed" : "Login failed";

    if (init) setStatus("Hive: initialization failed"); else setLoginStatus("Login: failed");
    log("[Hive] %s: %s", head.c_str(), reason.c_str());

    _hasNotice = true;
    if (retries < kMaxRetries)
    {
        ++retries;
        _notice.text = head + " (" + reason + ").\nRetrying (" + std::to_string(retries) + "/" +
                       std::to_string(kMaxRetries) + ")...";
        _notice.fatal = false;
        _afterNotice = [this, init]() { if (init) beginInit(); else beginSignIn(); };
    }
    else
    {
        _notice.text = head + " (" + reason + ").\nThe game will exit.";
        _notice.fatal = true;
        _afterNotice = nullptr;
    }
}

void HiveService::acknowledgeNotice()
{
    if (!_hasNotice) return;
    _hasNotice = false;
    const bool fatal = _notice.fatal;
    auto retry = std::move(_afterNotice);
    _afterNotice = nullptr;

    if (fatal)
    {
        Director::getInstance()->end();
    }
    else if (retry)
    {
        retry();
    }
}

// ---------------------------------------------------------------- initialization
void HiveService::beginInit()
{
    _busy = true;

    if (!_bridge)
    {
        HiveBridge::Options options;
        if (!getEnv("HIVE_BRIDGE_ENGINE").empty()) options.engine = getEnv("HIVE_BRIDGE_ENGINE");
        if (!getEnv("HIVE_BRIDGE_PLATFORM").empty()) options.platform = getEnv("HIVE_BRIDGE_PLATFORM");

        auto bridge = std::make_shared<HiveBridge>(std::make_shared<DllTransport>(), makeCocosDispatcher(), options);
        std::string error;
        if (!bridge->start(&error))
        {
            log("[Hive] %s", error.c_str());
            fail(Stage::Init, "SDK unavailable");
            return;
        }
        _bridge = bridge;
    }

    HiveConfiguration config(*_bridge);
    if (!_configured)
    {
        const std::string appId = getEnv("HIVE_APP_ID");
        if (!appId.empty()) config.setAppId(appId);
        const std::string zone = getEnv("HIVE_ZONE");
        if (!zone.empty()) config.setZone(zone == "SANDBOX" ? HiveZone::Sandbox : HiveZone::Real);
        const std::string serverId = getEnv("HIVE_SERVER_ID");
        if (!serverId.empty()) config.setServerId(serverId);
        const std::string language = getEnv("HIVE_GAME_LANGUAGE");
        if (!language.empty()) config.setGameLanguage(language);
        _configured = true;
    }

    if (_sdkVersion.empty()) _sdkVersion = config.getHiveSDKVersion();
    if (_sdkVersion.empty())
    {
        fail(Stage::Init, "no reply from SDK");
        return;
    }
    setStatus("Hive " + _sdkVersion + ": setup...");

    HiveAuthV4(*_bridge).setup([this](const HiveAuthV4::SetupResult& result) {
        if (!result.delivered)
        {
            fail(Stage::Init, "setup request failed");
            return;
        }
        if (!result.result.isSuccess())
        {
            log("[Hive] setup failed: %s", result.result.toString().c_str());
            fail(Stage::Init, "setup code " + std::to_string(result.result.code));
            return;
        }

        _setupReady = true;
        _initRetries = 0;
        setStatus("Hive " + _sdkVersion + ": ready (providers " + std::to_string(result.providerTypeList.size()) + ")");
        beginSignIn();
    });
}

// ---------------------------------------------------------------- sign-in
void HiveService::beginSignIn()
{
    _busy = true;

    // The stored session can only be used when the SDK says so. If AUTO fails, fall back to the sign-in UI within the
    // same attempt (as the SDK documentation asks); only a failing UI counts as a failed attempt.
    if (HiveAuthV4(*_bridge).isAutoSignIn())
    {
        setLoginStatus("Login: auto sign-in...");
        HiveAuthV4(*_bridge).signIn("AUTO", [this](const HiveAuthV4::SignInResult& r) {
            if (r.delivered && r.result.isSuccess())
            {
                onSignInResult(r);
                return;
            }
            log("[Hive] auto sign-in failed (%s); showing the sign-in UI",
                r.delivered ? r.result.toString().c_str() : "request not delivered");
            showSignInUi();
        });
    }
    else
    {
        showSignInUi();
    }
}

void HiveService::showSignInUi()
{
    setLoginStatus("Login: Hive sign-in UI...");
    HiveAuthV4(*_bridge).showSignIn([this](const HiveAuthV4::SignInResult& r) {
        if (r.delivered && r.result.isSuccess())
        {
            onSignInResult(r);
        }
        else
        {
            fail(Stage::SignIn, r.delivered ? "code " + std::to_string(r.result.code) : std::string("request failed"));
        }
    });
}

void HiveService::onSignInResult(const HiveAuthV4::SignInResult& result)
{
    // Never log tokens: HivePlayerInfo::toString() leaves them out (and lists provider names only).
    log("[Hive] sign-in reply: %s | %s", result.result.toString().c_str(), result.playerInfo.toString().c_str());
    if (!result.playerInfo.providerInfoKnown)
    {
        log("[Hive] sign-in reply has no provider info: the player is treated as a guest (sign-out hidden)");
    }

    _busy = false;
    _signInRetries = 0;
    _signedIn = true;
    _player = result.playerInfo;
    // Provider names only (never ids or e-mails). This line also shows whether the guest detection matched the real reply.
    std::string linked = "link info unknown";
    if (_player.providerInfoKnown)
    {
        linked = "guest";
        for (size_t i = 0; i < _player.connectedProviders.size(); ++i)
        {
            linked = (i == 0 ? "linked: " : linked + ", ") + _player.connectedProviders[i];
        }
    }
    setLoginStatus("Login: signed in (" + playerLabel() + (_player.isNewUser ? ", new user" : "") + ", " + linked + ")");
}

void HiveService::requestSignIn()
{
    if (!_bridge || !_setupReady || _busy || _signedIn || _hasNotice) return;

    _signInRetries = 0;     // a fresh attempt requested by the player
    beginSignIn();
}

void HiveService::reinitialize()
{
    // The terms come back with the next setup. Start from a clean state, as at the start of the game.
    _setupReady = false;
    resetPlayer();
    _initRetries = 0;
    _signInRetries = 0;
    _hasNotice = false;
    _afterNotice = nullptr;
    setStatus("Hive " + _sdkVersion + ": setup...");
    setLoginStatus(std::string());
    beginInit();
}

void HiveService::requestResetTerms(ResultCallback done)
{
    const AccountKind kind = accountKind();
    if (!_bridge || !_setupReady || _busy || (kind != AccountKind::Guest && kind != AccountKind::Linked))
    {
        if (done) done(false, kind == AccountKind::Unknown ? "account type unknown" : "not available");
        return;
    }

    const bool sent = HiveAuthV4(*_bridge).resetAgreement();
    log("[Hive] resetAgreement requested (sent=%d), account kind: %s", sent ? 1 : 0,
        kind == AccountKind::Guest ? "guest (deleting it)" : "linked (signing out)");

    // Signing out would lose a guest account for good, so a guest is deleted instead (the player agreed before).
    _busy = true;
    setLoginStatus(kind == AccountKind::Guest ? "Login: deleting guest account..." : "Login: signing out...");
    auto finished = [this, done](const HiveAuthV4::SignOutResult& r) {
        _busy = false;
        if (r.delivered && r.result.isSuccess())
        {
            reinitialize();
            if (done) done(true, std::string());
        }
        else
        {
            log("[Hive] reset terms: sign-out / deletion reply: %s", r.result.toString().c_str());
            const std::string reason = r.delivered ? "code " + std::to_string(r.result.code) : std::string("request failed");
            setLoginStatus("Login: reset failed (" + reason + ")");
            if (done) done(false, reason);
        }
    };
    if (kind == AccountKind::Guest)
    {
        HiveAuthV4(*_bridge).playerDelete(finished);
    }
    else
    {
        HiveAuthV4(*_bridge).signOut(finished);
    }
}

// ---------------------------------------------------------------- sign-out / account deletion
void HiveService::requestSignOut(ResultCallback done)
{
    if (!_bridge || !_setupReady || _busy || !canSignOut())
    {
        if (done) done(false, "not available");
        return;
    }

    _busy = true;
    setLoginStatus("Login: signing out...");
    HiveAuthV4(*_bridge).signOut([this, done](const HiveAuthV4::SignOutResult& r) {
        _busy = false;
        if (r.delivered && r.result.isSuccess())
        {
            resetPlayer();
            setLoginStatus("Login: signed out");
            if (done) done(true, std::string());
        }
        else
        {
            log("[Hive] sign-out reply: %s", r.result.toString().c_str());
            const std::string reason = r.delivered ? "code " + std::to_string(r.result.code) : std::string("request failed");
            setLoginStatus("Login: sign-out failed (" + reason + ")");
            if (done) done(false, reason);
        }
    });
}

void HiveService::requestDeleteAccount(ResultCallback done)
{
    if (!_bridge || !_setupReady || _busy || !_signedIn)
    {
        if (done) done(false, "not available");
        return;
    }

    _busy = true;
    setLoginStatus("Login: deleting account...");
    HiveAuthV4(*_bridge).playerDelete([this, done](const HiveAuthV4::SignOutResult& r) {
        _busy = false;
        if (r.delivered && r.result.isSuccess())
        {
            resetPlayer();
            setLoginStatus("Login: account deleted");
            if (done) done(true, std::string());
        }
        else
        {
            log("[Hive] account deletion reply: %s", r.result.toString().c_str());
            const std::string reason = r.delivered ? "code " + std::to_string(r.result.code) : std::string("request failed");
            setLoginStatus("Login: account deletion failed (" + reason + ")");
            if (done) done(false, reason);
        }
    });
}

void HiveService::shutdown()
{
    if (!_bridge) return;
    HiveAuthV4(*_bridge).terminateProcess();
    _bridge->shutdown();
    _bridge.reset();
    terminateCefChildren();
}
