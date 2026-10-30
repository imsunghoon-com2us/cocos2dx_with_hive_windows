#include "HiveCocos/HiveAuthV4.h"

#include <sstream>

namespace hivecocos {

std::string HivePlayerInfo::toString() const
{
    // Tokens are deliberately left out so this is always safe to log.
    std::ostringstream os;
    os << "PlayerInfo { playerId=" << playerId << ", playerName=" << playerName
       << ", isNewUser=" << (isNewUser ? "true" : "false") << ", providers=";
    if (!providerInfoKnown) os << "unknown";
    else if (connectedProviders.empty()) os << "none(guest)";
    else for (size_t i = 0; i < connectedProviders.size(); ++i) os << (i ? "," : "") << connectedProviders[i];
    os << " }";
    return os.str();
}

namespace {

std::string memberString(const rapidjson::Value& obj, const char* key)
{
    auto it = obj.FindMember(key);
    return (it != obj.MemberEnd() && it->value.IsString()) ? std::string(it->value.GetString(), it->value.GetStringLength())
                                                          : std::string();
}

HivePlayerInfo parsePlayerInfo(const HiveResponse& reply)
{
    HivePlayerInfo p;
    if (!reply.doc) return p;
    auto it = reply.doc->FindMember("playerInfo");
    if (it == reply.doc->MemberEnd() || !it->value.IsObject()) return p;

    const rapidjson::Value& o = it->value;
    auto id = o.FindMember("playerId");
    if (id != o.MemberEnd() && id->value.IsNumber()) p.playerId = id->value.GetInt64();
    p.playerName = memberString(o, "playerName");
    p.playerImageUrl = memberString(o, "playerImageUrl");
    p.did = memberString(o, "did");
    p.playerToken = memberString(o, "playerToken");
    p.accessToken = memberString(o, "accessToken");
    auto nu = o.FindMember("isNewUser");
    p.isNewUser = nu != o.MemberEnd() && nu->value.IsBool() && nu->value.GetBool();

    // Provider names only (the object values hold user ids / e-mails and are not read).
    for (const char* key : { "providerInfoData", "customProviderInfoData" })
    {
        auto pi = o.FindMember(key);
        if (pi == o.MemberEnd() || !pi->value.IsObject()) continue;
        p.providerInfoKnown = true;
        for (auto m = pi->value.MemberBegin(); m != pi->value.MemberEnd(); ++m)
        {
            p.connectedProviders.emplace_back(m->name.GetString(), m->name.GetStringLength());
        }
    }
    return p;
}

} // namespace

HiveAuthV4::SetupResult HiveAuthV4::parseSetupReply(const HiveResponse& reply)
{
    SetupResult s;
    s.delivered = reply.ok;
    if (!reply.ok) return s;

    s.result = reply.resultAPI();
    s.isAutoSignIn = reply.getBool("isAutoSignIn");
    s.did = reply.getString("did");

    auto it = reply.doc->FindMember("providerTypeList");
    if (it != reply.doc->MemberEnd() && it->value.IsArray())
    {
        for (const auto& v : it->value.GetArray())
        {
            if (v.IsString()) s.providerTypeList.emplace_back(v.GetString(), v.GetStringLength());
        }
    }
    return s;
}

HiveAuthV4::SignInResult HiveAuthV4::parseSignInReply(const HiveResponse& reply)
{
    SignInResult s;
    s.delivered = reply.ok;
    if (!reply.ok) return s;
    s.result = reply.resultAPI();
    s.playerInfo = parsePlayerInfo(reply);
    return s;
}

HiveAuthV4::SignOutResult HiveAuthV4::parseSignOutReply(const HiveResponse& reply)
{
    SignOutResult s;
    s.delivered = reply.ok;
    if (!reply.ok) return s;
    s.result = reply.resultAPI();
    return s;
}

void HiveAuthV4::setup(SetupCallback callback)
{
    const bool sent = _bridge.callAsync("AuthV4", "setup", HiveParams(), [callback](const HiveResponse& reply) {
        if (callback) callback(parseSetupReply(reply));
    });
    if (!sent && callback) callback(SetupResult());
}

void HiveAuthV4::signIn(const std::string& providerType, SignInCallback callback)
{
    const bool sent = _bridge.callAsync("AuthV4", "signIn", { { "providerType", providerType } },
        [callback](const HiveResponse& reply) {
            if (callback) callback(parseSignInReply(reply));
        });
    if (!sent && callback) callback(SignInResult());
}

void HiveAuthV4::showSignIn(SignInCallback callback)
{
    const bool sent = _bridge.callAsync("AuthV4", "showSignIn", HiveParams(), [callback](const HiveResponse& reply) {
        if (callback) callback(parseSignInReply(reply));
    });
    if (!sent && callback) callback(SignInResult());
}

void HiveAuthV4::signOut(SignOutCallback callback)
{
    const bool sent = _bridge.callAsync("AuthV4", "signOut", HiveParams(), [callback](const HiveResponse& reply) {
        if (callback) callback(parseSignOutReply(reply));
    });
    if (!sent && callback) callback(SignOutResult());
}

void HiveAuthV4::playerDelete(SignOutCallback callback)
{
    const bool sent = _bridge.callAsync("AuthV4", "playerDelete", HiveParams(), [callback](const HiveResponse& reply) {
        if (callback) callback(parseSignOutReply(reply));
    });
    if (!sent && callback) callback(SignOutResult());
}

bool HiveAuthV4::isAutoSignIn()
{
    const HiveResponse reply = _bridge.callSync("AuthV4", "isAutoSignIn");
    return reply.ok && reply.getBool("isAutoSignIn");
}

bool HiveAuthV4::resetAgreement()
{
    if (!_bridge.isStarted()) return false;
    _bridge.callSync("AuthV4", "resetAgreement");     // the reply is empty; nothing to parse
    return true;
}

bool HiveAuthV4::terminateProcess()
{
    return _bridge.callSync("AuthV4", "terminateProcess").ok;
}

} // namespace hivecocos
