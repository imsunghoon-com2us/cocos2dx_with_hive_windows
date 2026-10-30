#include "HiveCocos/HiveAuthV4.h"

namespace hivecocos {

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

void HiveAuthV4::setup(SetupCallback callback)
{
    const bool sent = _bridge.callAsync("AuthV4", "setup", HiveParams(), [callback](const HiveResponse& reply) {
        if (callback) callback(parseSetupReply(reply));
    });
    if (!sent && callback) callback(SetupResult());
}

bool HiveAuthV4::terminateProcess()
{
    return _bridge.callSync("AuthV4", "terminateProcess").ok;
}

} // namespace hivecocos
