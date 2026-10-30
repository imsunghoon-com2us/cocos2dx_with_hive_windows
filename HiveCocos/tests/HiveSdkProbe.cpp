// Integration probe against the REAL Hive SDK DLLs (no cocos2d-x, no UI). Not part of the unit tests.
//   HiveSdkProbe.exe <directory with HIVE_PLUGIN_CPP.dll>
// Checks, in order: DLL load, a synchronous call, and that an asynchronous reply really arrives through the
// SDK's message callback on another thread and is handed to the dispatcher. Exit code 0 = all passed.
#include "HiveCocos/DllTransport.h"
#include "HiveCocos/HiveBridge.h"
#include "HiveCocos/HiveConfiguration.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace hivecocos;

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2)
    {
        std::printf("usage: HiveSdkProbe <directory containing HIVE_PLUGIN_CPP.dll>\n");
        return 2;
    }

    std::mutex m;
    std::condition_variable cv;
    std::deque<std::function<void()>> queue;
    const std::thread::id mainThread = std::this_thread::get_id();

    HiveBridge::Dispatcher dispatcher = [&](std::function<void()> fn) {
        { std::lock_guard<std::mutex> l(m); queue.push_back(std::move(fn)); }
        cv.notify_all();
    };

    HiveBridge bridge(std::make_shared<DllTransport>(argv[1]), dispatcher);
    std::string error;
    if (!bridge.start(&error))
    {
        std::printf("FAIL start: %s\n", error.c_str());
        return 1;
    }
    std::printf("PASS dll loaded and callback registered\n");

    HiveConfiguration config(bridge);
    const std::string version = config.getHiveSDKVersion();
    std::printf("%s sync getHiveSDKVersion = '%s'\n", version.empty() ? "FAIL" : "PASS", version.c_str());
    if (version.empty()) return 1;

    // Asynchronous round trip: the reply is produced by the SDK on its own thread.
    bool got = false;
    bool onMainThread = false;
    HiveResultAPI result;
    const bool sent = bridge.callAsync("Configuration", "getMetaData", { { "key", "hivecocos_probe" }, { "forceReload", false } },
        [&](const HiveResponse& r) {
            got = true;
            onMainThread = std::this_thread::get_id() == mainThread;
            result = r.resultAPI();
        });
    std::printf("%s async request sent\n", sent ? "PASS" : "FAIL");
    if (!sent) return 1;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!got && std::chrono::steady_clock::now() < deadline)
    {
        std::function<void()> fn;
        {
            std::unique_lock<std::mutex> l(m);
            cv.wait_for(l, std::chrono::milliseconds(200), [&] { return !queue.empty(); });
            if (!queue.empty()) { fn = std::move(queue.front()); queue.pop_front(); }
        }
        if (fn) fn();
    }

    std::printf("%s async reply received through the SDK callback\n", got ? "PASS" : "FAIL");
    if (!got) return 1;
    std::printf("%s handler ran on the dispatcher (caller) thread\n", onMainThread ? "PASS" : "FAIL");
    std::printf("     %s\n", result.toString().c_str());     // success or a server/network error: either proves the path

    bridge.shutdown();
    return onMainThread ? 0 : 1;
}
