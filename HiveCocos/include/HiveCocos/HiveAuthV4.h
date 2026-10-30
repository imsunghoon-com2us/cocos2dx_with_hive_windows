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

    struct SignInResult
    {
        bool delivered = false;                 // false: the request could not be sent
        HiveResultAPI result;
        HivePlayerInfo playerInfo;              // meaningful only when result.isSuccess()
    };
    using SignInCallback = std::function<void(const SignInResult&)>;

    struct SignOutResult
    {
        bool delivered = false;
        HiveResultAPI result;
    };
    using SignOutCallback = std::function<void(const SignOutResult&)>;

    explicit HiveAuthV4(HiveBridge& bridge) : _bridge(bridge) {}

    // Asynchronous. The callback runs through the bridge's dispatcher. When the request cannot be sent, the
    // callback is invoked immediately with delivered == false.
    void setup(SetupCallback callback);

    // Asynchronous sign-in. providerType is the native name ("GUEST", "GOOGLE", "AUTO", ... as listed in
    // SetupResult::providerTypeList). "AUTO" signs in with the stored session when setup reported isAutoSignIn.
    // Callbacks follow the same rule as setup(): they run through the dispatcher, and when the request cannot
    // be sent they are invoked immediately with delivered == false.
    void signIn(const std::string& providerType, SignInCallback callback);

    // Shows the Hive sign-in UI (provider selection) and reports the result when the user finishes or cancels.
    void showSignIn(SignInCallback callback);

    void signOut(SignOutCallback callback);

    // Synchronous. Call before the process exits.
    bool terminateProcess();

    static SetupResult parseSetupReply(const HiveResponse& reply);
    static SignInResult parseSignInReply(const HiveResponse& reply);
    static SignOutResult parseSignOutReply(const HiveResponse& reply);

private:
    HiveBridge& _bridge;
};

} // namespace hivecocos

#endif // HIVECOCOS_HIVE_AUTHV4_H
