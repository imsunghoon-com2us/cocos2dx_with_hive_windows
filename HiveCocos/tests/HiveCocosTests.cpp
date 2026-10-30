// Console tests for the cocos-free core of HiveCocos. Exit code 0 = all passed.
#include "HiveCocos/DllTransport.h"
#include "HiveCocos/FakeTransport.h"
#include "HiveCocos/HiveAuthV4.h"
#include "HiveCocos/HiveBridge.h"
#include "HiveCocos/HiveConfiguration.h"

#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace hivecocos;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                                     \
    do {                                                                                \
        ++g_checks;                                                                     \
        if (!(cond)) { ++g_failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

#define CHECK_EQ(a, b)                                                                  \
    do {                                                                                \
        ++g_checks;                                                                     \
        const auto va = (a); const auto vb = (b);                                       \
        if (!(va == vb)) { ++g_failures; std::printf("  FAIL %s:%d  %s == %s\n", __FILE__, __LINE__, #a, #b); } \
    } while (0)

// A dispatcher that queues work; the test thread plays the part of the cocos thread.
struct MainQueue
{
    std::mutex m;
    std::deque<std::function<void()>> q;
    HiveBridge::Dispatcher dispatcher()
    {
        return [this](std::function<void()> fn) { std::lock_guard<std::mutex> l(m); q.push_back(std::move(fn)); };
    }
    size_t pending() { std::lock_guard<std::mutex> l(m); return q.size(); }
    void drain()
    {
        for (;;)
        {
            std::function<void()> fn;
            { std::lock_guard<std::mutex> l(m); if (q.empty()) return; fn = std::move(q.front()); q.pop_front(); }
            fn();
        }
    }
};

static rapidjson::Document parseJson(const std::string& s)
{
    rapidjson::Document d;
    d.Parse(s.c_str());
    return d;
}

struct Fixture
{
    std::shared_ptr<FakeTransport> fake = std::make_shared<FakeTransport>();
    MainQueue queue;
    HiveBridge bridge;
    explicit Fixture(HiveBridge::Options o = HiveBridge::Options()) : bridge(fake, queue.dispatcher(), o)
    {
        std::string err;
        const bool started = bridge.start(&err);
        CHECK(started);
    }
};

static void testRequestFormat()
{
    std::printf("request format\n");
    HiveBridge::Options o;
    std::string s = HiveBridge::buildRequest("Configuration", "setAppId", 7, o, { { "setAppId", "com.example.app" } });
    rapidjson::Document d = parseJson(s);
    CHECK(d.IsObject());
    CHECK_EQ(std::string(d["class"].GetString()), std::string("Configuration"));
    CHECK_EQ(std::string(d["method"].GetString()), std::string("setAppId"));
    CHECK_EQ(d["handlerId"].GetInt(), 7);
    CHECK_EQ(std::string(d["platform"].GetString()), std::string("cpp"));
    CHECK(!d.HasMember("engine"));                       // omitted unless configured
    CHECK_EQ(std::string(d["setAppId"].GetString()), std::string("com.example.app"));   // flat params

    o.engine = "Cocos2dx";
    o.platform = "windows";
    d = parseJson(HiveBridge::buildRequest("AuthV4", "setup", 1, o, {}));
    CHECK_EQ(std::string(d["engine"].GetString()), std::string("Cocos2dx"));
    CHECK_EQ(std::string(d["platform"].GetString()), std::string("windows"));

    s = HiveBridge::buildRequest("Configuration", "setHttpConnectTimeout", -1, HiveBridge::Options(),
                                 { { "setHttpConnectTimeout", 5 }, { "setUseLog", true }, { "ratio", 0.5 } });
    CHECK(s.find("\"setHttpConnectTimeout\":5,") != std::string::npos);   // "5", not "5.0"
    CHECK(s.find("5.0") == std::string::npos);
    CHECK(s.find("\"setUseLog\":true") != std::string::npos);
    CHECK(s.find("\"ratio\":0.5") != std::string::npos);

    // UTF-8 and escaping survive a round trip.
    const std::string tricky = u8"\uD55C\uAE00 \"quote\" \\ back\nline";
    d = parseJson(HiveBridge::buildRequest("X", "y", 1, HiveBridge::Options(), { { "v", tricky } }));
    CHECK(d.IsObject());
    CHECK_EQ(std::string(d["v"].GetString()), tricky);
}

static void testConfigurationSync()
{
    std::printf("configuration (sync)\n");
    Fixture f;
    f.fake->setResponder([](const std::string& req) -> std::string {
        rapidjson::Document d = parseJson(req);
        const std::string m = d["method"].GetString();
        if (m == "getHiveSDKVersion") return "{\"getHiveSDKVersion\":\"26.7.0\"}";
        if (m == "getZone") return "{\"getZone\":\"SANDBOX\"}";
        if (m == "getAppId") return "{\"getAppId\":\"com.example.app\"}";
        return "{}";
    });
    HiveConfiguration cfg(f.bridge);
    CHECK_EQ(cfg.getHiveSDKVersion(), std::string("26.7.0"));
    CHECK(cfg.getZone() == HiveZone::Sandbox);
    CHECK_EQ(cfg.getAppId(), std::string("com.example.app"));

    CHECK(cfg.setAppId("com.example.app"));
    CHECK(cfg.setZone(HiveZone::Real));
    CHECK(cfg.setServerId("KR"));
    CHECK(cfg.setGameLanguage("ko"));
    CHECK(cfg.setUseLog(true));
    CHECK(cfg.setHiveCertificationKey("KEY"));

    auto reqs = f.fake->requests();
    CHECK_EQ(reqs.size(), size_t(9));
    rapidjson::Document last = parseJson(reqs.back());
    CHECK_EQ(std::string(last["method"].GetString()), std::string("setHiveCertificationKey"));
    CHECK_EQ(std::string(last["appKey"].GetString()), std::string("KEY"));    // key differs from the method name
    CHECK_EQ(last["handlerId"].GetInt(), -1);                                  // sync calls carry no handler
    rapidjson::Document zone = parseJson(reqs[4]);
    CHECK_EQ(std::string(zone["setZone"].GetString()), std::string("REAL"));
}

static void testSyncFailures()
{
    std::printf("sync failures\n");
    Fixture f;
    HiveConfiguration cfg(f.bridge);

    f.fake->setResponder([](const std::string&) { return std::string(); });          // native returned nothing
    HiveResponse r = f.bridge.callSync("Configuration", "getAppId");
    CHECK(!r.ok);
    CHECK_EQ(cfg.getAppId(), std::string());

    f.fake->setResponder([](const std::string&) { return std::string("not json{"); });
    r = f.bridge.callSync("Configuration", "getAppId");
    CHECK(!r.ok);
    CHECK(!cfg.setAppId("x"));

    f.fake->setResponder([](const std::string&) { return std::string("[1,2]"); });    // valid JSON, wrong shape
    CHECK(!f.bridge.callSync("a", "b").ok);
}

static void testAsyncMatchingAndThreading()
{
    std::printf("async matching and threading\n");
    Fixture f;
    HiveAuthV4 auth(f.bridge);

    int calls = 0;
    std::thread::id handlerThread;
    HiveAuthV4::SetupResult got;
    auth.setup([&](const HiveAuthV4::SetupResult& r) { ++calls; handlerThread = std::this_thread::get_id(); got = r; });

    auto reqs = f.fake->requests();
    CHECK_EQ(reqs.size(), size_t(1));
    rapidjson::Document req = parseJson(reqs[0]);
    CHECK_EQ(std::string(req["class"].GetString()), std::string("AuthV4"));
    CHECK_EQ(std::string(req["method"].GetString()), std::string("setup"));
    const int id = req["handlerId"].GetInt();
    CHECK_EQ(id, 1);                                                                   // ids start at 1

    // The SDK answers from its own thread.
    f.fake->emitFromOtherThread("{\"handlerId\":" + std::to_string(id) +
        ",\"resultAPI\":{\"errorCode\":0,\"code\":0,\"errorMessage\":\"\",\"message\":\"success\",\"latencyMs\":42},"
        "\"isAutoSignIn\":true,\"did\":\"123456789\",\"providerTypeList\":[\"GOOGLE\",\"GUEST\"]}");
    CHECK_EQ(calls, 0);                                         // not run on the SDK thread
    CHECK_EQ(f.queue.pending(), size_t(1));
    f.queue.drain();
    CHECK_EQ(calls, 1);
    CHECK(handlerThread == std::this_thread::get_id());         // ran where drain() ran ("cocos thread")
    CHECK(got.delivered);
    CHECK(got.result.isSuccess());
    CHECK_EQ(got.result.latencyMs, int64_t(42));
    CHECK(got.isAutoSignIn);
    CHECK_EQ(got.did, std::string("123456789"));
    CHECK_EQ(got.providerTypeList.size(), size_t(2));
    CHECK_EQ(got.providerTypeList[1], std::string("GUEST"));

    // A reply is delivered once: a duplicate and an unknown handlerId are ignored.
    f.fake->emitFromOtherThread("{\"handlerId\":" + std::to_string(id) + ",\"did\":\"dup\"}");
    f.fake->emitFromOtherThread("{\"handlerId\":999,\"did\":\"unknown\"}");
    f.queue.drain();
    CHECK_EQ(calls, 1);
}

static void testOutOfOrderReplies()
{
    std::printf("out-of-order replies\n");
    Fixture f;
    std::string a, b;
    f.bridge.callAsync("X", "first", {}, [&](const HiveResponse& r) { a = r.getString("v"); });
    f.bridge.callAsync("X", "second", {}, [&](const HiveResponse& r) { b = r.getString("v"); });
    auto reqs = f.fake->requests();
    const int idA = parseJson(reqs[0])["handlerId"].GetInt();
    const int idB = parseJson(reqs[1])["handlerId"].GetInt();
    CHECK(idA != idB);
    CHECK(idA >= 1 && idB >= 1);

    f.fake->emitFromOtherThread("{\"handlerId\":" + std::to_string(idB) + ",\"v\":\"B\"}");
    f.fake->emitFromOtherThread("{\"handlerId\":" + std::to_string(idA) + ",\"v\":\"A\"}");
    f.queue.drain();
    CHECK_EQ(a, std::string("A"));
    CHECK_EQ(b, std::string("B"));
}

static void testEvents()
{
    std::printf("events\n");
    Fixture f;
    int plain = 0, keyed = 0;
    f.bridge.setEventListener("AuthV4:onSignIn", [&](const HiveResponse&) { ++plain; });
    f.bridge.setEventListener("AuthV4:onSignIn:abc", [&](const HiveResponse&) { ++keyed; });

    f.fake->emitFromOtherThread("{\"class\":\"AuthV4\",\"method\":\"onSignIn\"}");
    f.fake->emitFromOtherThread("{\"class\":\"AuthV4\",\"method\":\"onSignIn\",\"uniqueKey\":\"abc\"}");
    f.fake->emitFromOtherThread("{\"class\":\"AuthV4\",\"method\":\"unknown\"}");       // no listener: ignored
    f.fake->emitFromOtherThread("{\"nothing\":1}");                                      // neither handler nor event
    f.queue.drain();
    CHECK_EQ(plain, 1);
    CHECK_EQ(keyed, 1);

    f.bridge.removeEventListener("AuthV4:onSignIn");
    f.fake->emitFromOtherThread("{\"class\":\"AuthV4\",\"method\":\"onSignIn\"}");
    f.queue.drain();
    CHECK_EQ(plain, 1);
}

static void testNotStartedAndFailures()
{
    std::printf("not started / start failure / shutdown\n");
    {
        auto fake = std::make_shared<FakeTransport>();
        MainQueue q;
        HiveBridge bridge(fake, q.dispatcher());                 // never started
        CHECK(!bridge.isStarted());
        CHECK(!bridge.callAsync("X", "y", {}, [](const HiveResponse&) {}));
        HiveResponse r = bridge.callSync("X", "y");
        CHECK(!r.ok);
        CHECK_EQ(r.error, std::string("bridge not started"));
        CHECK_EQ(fake->requests().size(), size_t(0));            // nothing reached the transport

        HiveAuthV4 auth(bridge);
        bool called = false, delivered = true;
        auth.setup([&](const HiveAuthV4::SetupResult& s) { called = true; delivered = s.delivered; });
        CHECK(called);                                           // reported immediately, not lost
        CHECK(!delivered);
    }
    {
        auto fake = std::make_shared<FakeTransport>();
        fake->setStartFailure("boom");
        HiveBridge bridge(fake, nullptr);
        std::string err;
        CHECK(!bridge.start(&err));
        CHECK_EQ(err, std::string("boom"));
    }
    {
        Fixture f;
        int calls = 0;
        f.bridge.callAsync("X", "y", {}, [&](const HiveResponse&) { ++calls; });
        const int id = parseJson(f.fake->requests()[0])["handlerId"].GetInt();
        f.bridge.shutdown();
        f.fake->emitFromOtherThread("{\"handlerId\":" + std::to_string(id) + "}");   // callback detached: no crash
        f.queue.drain();
        CHECK_EQ(calls, 0);
    }
    {
        // Without a dispatcher the handler runs inline on the delivering thread.
        auto fake = std::make_shared<FakeTransport>();
        HiveBridge bridge(fake, nullptr);
        bridge.start();
        int calls = 0;
        bridge.callAsync("X", "y", {}, [&](const HiveResponse&) { ++calls; });
        fake->emit("{\"handlerId\":1}");
        CHECK_EQ(calls, 1);
    }
}

static void testResultApi()
{
    std::printf("resultAPI\n");
    HiveResponse none;
    CHECK(!none.resultAPI().isSuccess());                                   // absent -> UNKNOWN

    auto reply = [](const char* json) { HiveResponse r; r.ok = true; r.doc = std::make_shared<rapidjson::Document>(); r.doc->Parse(json); return r; };
    CHECK(reply("{\"resultAPI\":{\"errorCode\":0}}").resultAPI().isSuccess());
    CHECK(reply("{\"resultAPI\":{\"errorCode\":-1,\"code\":-1100001}}").resultAPI().isSuccess() == false);
    CHECK(reply("{\"resultAPI\":{\"errorCode\":-1,\"code\":0}}").resultAPI().isSuccess());   // either field >= 0 (as in the Unreal wrapper)
    HiveResultAPI r = reply("{\"resultAPI\":{\"errorCode\":-99,\"code\":-1200055,\"errorMessage\":\"net\",\"latencyMs\":5000000000}}").resultAPI();
    CHECK_EQ(r.code, -1200055);
    CHECK_EQ(r.errorMessage, std::string("net"));
    CHECK_EQ(r.latencyMs, int64_t(5000000000LL));                            // 64-bit latency
}

static void testSignInSignOut()
{
    std::printf("sign in / show sign in / sign out\n");
    Fixture f;
    HiveAuthV4 auth(f.bridge);

    HiveAuthV4::SignInResult in1, in2;
    HiveAuthV4::SignOutResult out;
    int calls = 0;
    auth.signIn("GUEST", [&](const HiveAuthV4::SignInResult& r) { ++calls; in1 = r; });
    auth.showSignIn([&](const HiveAuthV4::SignInResult& r) { ++calls; in2 = r; });
    auth.signOut([&](const HiveAuthV4::SignOutResult& r) { ++calls; out = r; });

    auto reqs = f.fake->requests();
    CHECK_EQ(reqs.size(), size_t(3));
    rapidjson::Document r0 = parseJson(reqs[0]), r1 = parseJson(reqs[1]), r2 = parseJson(reqs[2]);
    CHECK_EQ(std::string(r0["method"].GetString()), std::string("signIn"));
    CHECK_EQ(std::string(r0["providerType"].GetString()), std::string("GUEST"));     // parameter key per the SDK source
    CHECK_EQ(std::string(r1["method"].GetString()), std::string("showSignIn"));
    CHECK(!r1.HasMember("providerType"));
    CHECK_EQ(std::string(r2["method"].GetString()), std::string("signOut"));
    CHECK_EQ(std::string(r2["class"].GetString()), std::string("AuthV4"));
    CHECK(r0["handlerId"].GetInt() >= 1 && r1["handlerId"].GetInt() >= 1 && r2["handlerId"].GetInt() >= 1);

    // Successful sign-in (playerId does not fit in 32 bits), answered out of order and from another thread.
    f.fake->emitFromOtherThread("{\"handlerId\":" + std::to_string(r2["handlerId"].GetInt()) +
        ",\"resultAPI\":{\"errorCode\":0,\"code\":0}}");
    f.fake->emitFromOtherThread("{\"handlerId\":" + std::to_string(r1["handlerId"].GetInt()) +
        ",\"resultAPI\":{\"errorCode\":-1,\"code\":-1200101,\"errorMessage\":\"user cancelled\"}}");
    f.fake->emitFromOtherThread("{\"handlerId\":" + std::to_string(r0["handlerId"].GetInt()) +
        ",\"resultAPI\":{\"errorCode\":0,\"code\":0},\"playerInfo\":{\"playerId\":5000000123,\"playerName\":\"tester\","
        "\"playerImageUrl\":\"http://x/y.png\",\"playerToken\":\"SECRET-PLAYER-TOKEN\",\"accessToken\":\"SECRET-ACCESS-TOKEN\","
        "\"did\":\"987654321\",\"isNewUser\":true}}");
    CHECK_EQ(calls, 0);
    f.queue.drain();
    CHECK_EQ(calls, 3);

    CHECK(in1.delivered);
    CHECK(in1.result.isSuccess());
    CHECK_EQ(in1.playerInfo.playerId, int64_t(5000000123LL));
    CHECK_EQ(in1.playerInfo.playerName, std::string("tester"));
    CHECK_EQ(in1.playerInfo.did, std::string("987654321"));
    CHECK(in1.playerInfo.isNewUser);
    CHECK_EQ(in1.playerInfo.playerToken, std::string("SECRET-PLAYER-TOKEN"));      // available to the game ...
    CHECK_EQ(in1.playerInfo.accessToken, std::string("SECRET-ACCESS-TOKEN"));
    const std::string logged = in1.playerInfo.toString();
    CHECK(logged.find("SECRET") == std::string::npos);                              // ... but never in log output
    CHECK(logged.find("5000000123") != std::string::npos);

    CHECK(in2.delivered);                                                            // failure keeps the error details
    CHECK(!in2.result.isSuccess());
    CHECK_EQ(in2.result.code, -1200101);
    CHECK_EQ(in2.result.errorMessage, std::string("user cancelled"));
    CHECK_EQ(in2.playerInfo.playerId, int64_t(0));
    CHECK(in2.playerInfo.playerToken.empty());

    CHECK(out.delivered);
    CHECK(out.result.isSuccess());

    // Without a started bridge the callbacks still fire (delivered == false) instead of being lost.
    auto fake2 = std::make_shared<FakeTransport>();
    HiveBridge notStarted(fake2, nullptr);
    HiveAuthV4 auth2(notStarted);
    int fired = 0;
    bool anyDelivered = false;
    auth2.signIn("GUEST", [&](const HiveAuthV4::SignInResult& r) { ++fired; anyDelivered |= r.delivered; });
    auth2.showSignIn([&](const HiveAuthV4::SignInResult& r) { ++fired; anyDelivered |= r.delivered; });
    auth2.signOut([&](const HiveAuthV4::SignOutResult& r) { ++fired; anyDelivered |= r.delivered; });
    CHECK_EQ(fired, 3);
    CHECK(!anyDelivered);
    CHECK_EQ(fake2->requests().size(), size_t(0));
}

static void testPlayerDeleteAndAutoSignIn()
{
    std::printf("player delete / isAutoSignIn / guest detection\n");
    Fixture f;
    HiveAuthV4 auth(f.bridge);

    // playerDelete: same shape as signOut (class AuthV4, no parameters, handlerId >= 1).
    HiveAuthV4::SignOutResult del;
    int calls = 0;
    auth.playerDelete([&](const HiveAuthV4::SignOutResult& r) { ++calls; del = r; });
    auto reqs = f.fake->requests();
    CHECK_EQ(reqs.size(), size_t(1));
    rapidjson::Document r0 = parseJson(reqs[0]);
    CHECK_EQ(std::string(r0["class"].GetString()), std::string("AuthV4"));
    CHECK_EQ(std::string(r0["method"].GetString()), std::string("playerDelete"));
    CHECK(r0["handlerId"].GetInt() >= 1);
    f.fake->emitFromOtherThread("{\"handlerId\":" + std::to_string(r0["handlerId"].GetInt()) +
        ",\"resultAPI\":{\"errorCode\":-1,\"code\":-1200301,\"errorMessage\":\"denied\"}}");
    f.queue.drain();
    CHECK_EQ(calls, 1);
    CHECK(del.delivered);
    CHECK(!del.result.isSuccess());                  // a failed deletion keeps its code
    CHECK_EQ(del.result.code, -1200301);

    // isAutoSignIn: synchronous, answered as {"isAutoSignIn":bool}.
    bool answer = true;
    f.fake->setResponder([&answer](const std::string& req) -> std::string {
        rapidjson::Document d = parseJson(req);
        if (std::string(d["method"].GetString()) == "isAutoSignIn")
            return std::string("{\"isAutoSignIn\":") + (answer ? "true" : "false") + "}";
        return "{}";
    });
    CHECK(auth.isAutoSignIn());
    answer = false;
    CHECK(!auth.isAutoSignIn());
    rapidjson::Document last = parseJson(f.fake->requests().back());
    CHECK_EQ(std::string(last["class"].GetString()), std::string("AuthV4"));
    CHECK_EQ(last["handlerId"].GetInt(), -1);        // sync calls carry no handler

    auto fake2 = std::make_shared<FakeTransport>();
    HiveBridge notStarted(fake2, nullptr);
    CHECK(!HiveAuthV4(notStarted).isAutoSignIn());   // not started -> false, nothing sent
    CHECK_EQ(fake2->requests().size(), size_t(0));

    // resetAgreement: a synchronous request without parameters; an empty reply is fine.
    const size_t before = f.fake->requests().size();
    f.fake->setResponder([](const std::string&) -> std::string { return ""; });
    CHECK(auth.resetAgreement());
    auto afterReqs = f.fake->requests();
    CHECK_EQ(afterReqs.size(), before + 1);
    rapidjson::Document rr = parseJson(afterReqs.back());
    CHECK_EQ(std::string(rr["class"].GetString()), std::string("AuthV4"));
    CHECK_EQ(std::string(rr["method"].GetString()), std::string("resetAgreement"));
    CHECK_EQ(rr["handlerId"].GetInt(), -1);
    CHECK(!HiveAuthV4(notStarted).resetAgreement());  // not started -> false, nothing sent
    CHECK_EQ(fake2->requests().size(), size_t(0));

    // Guest detection from the sign-in reply. Only provider NAMES are read.
    auto parse = [](const std::string& playerInfoJson) {
        HiveResponse reply;
        reply.raw = "{\"resultAPI\":{\"errorCode\":0,\"code\":0},\"playerInfo\":" + playerInfoJson + "}";
        reply.doc = std::make_shared<rapidjson::Document>();
        reply.doc->Parse(reply.raw.c_str());
        reply.ok = true;
        return HiveAuthV4::parseSignInReply(reply).playerInfo;
    };
    const HivePlayerInfo linked = parse("{\"playerId\":1,\"providerInfoData\":{\"GOOGLE\":{\"providerType\":3,"
        "\"providerUserId\":\"SECRET-USER-ID\",\"providerEmail\":\"secret@example.com\"}}}");
    CHECK(linked.providerInfoKnown);
    CHECK_EQ(linked.connectedProviders.size(), size_t(1));
    CHECK_EQ(linked.connectedProviders[0], std::string("GOOGLE"));
    CHECK(!linked.isGuest());
    const std::string logged = linked.toString();
    CHECK(logged.find("GOOGLE") != std::string::npos);
    CHECK(logged.find("SECRET-USER-ID") == std::string::npos);     // user ids / e-mails never reach the log
    CHECK(logged.find("secret@example.com") == std::string::npos);

    const HivePlayerInfo guest = parse("{\"playerId\":2,\"providerInfoData\":{}}");
    CHECK(guest.providerInfoKnown);
    CHECK(guest.isGuest());

    const HivePlayerInfo customOnly = parse("{\"playerId\":3,\"providerInfoData\":{},"
        "\"customProviderInfoData\":{\"MYIDP\":{\"providerUserId\":\"x\"}}}");
    CHECK(!customOnly.isGuest());                                   // a custom provider counts as connected

    const HivePlayerInfo unknown = parse("{\"playerId\":4}");        // neither object: cannot tell -> not a guest
    CHECK(!unknown.providerInfoKnown);
    CHECK(!unknown.isGuest());
}

static void testDllTransportMissing()
{
    std::printf("DllTransport (missing DLL)\n");
    DllTransport t(L"C:\\this\\directory\\does\\not\\exist");
    std::string err;
    CHECK(!t.start([](const std::string&) {}, &err));
#ifdef _WIN32
    CHECK(err.find("HIVE_PLUGIN_CPP.dll not found") != std::string::npos);
#endif
    CHECK_EQ(t.call("{}"), std::string());                                   // safe when not loaded
    t.stop();
    t.stop();                                                                // idempotent

    auto transport = std::make_shared<DllTransport>(L"C:\\this\\directory\\does\\not\\exist");
    HiveBridge bridge(transport, nullptr);
    CHECK(!bridge.start(&err));
    CHECK(!err.empty());
    CHECK(!bridge.callSync("X", "y").ok);
}

int main()
{
    testRequestFormat();
    testConfigurationSync();
    testSyncFailures();
    testAsyncMatchingAndThreading();
    testOutOfOrderReplies();
    testEvents();
    testNotStartedAndFailures();
    testResultApi();
    testSignInSignOut();
    testPlayerDeleteAndAutoSignIn();
    testDllTransportMissing();

    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
