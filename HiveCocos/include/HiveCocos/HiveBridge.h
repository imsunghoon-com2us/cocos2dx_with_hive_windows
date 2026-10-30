#ifndef HIVECOCOS_HIVE_BRIDGE_H
#define HIVECOCOS_HIVE_BRIDGE_H

#include "HiveCocos/HiveTransport.h"
#include "HiveCocos/HiveTypes.h"

#include "json/document.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace hivecocos {

// A parsed native reply.
class HiveResponse
{
public:
    bool ok = false;            // false: not started, empty reply, or invalid JSON
    std::string error;          // reason when !ok
    std::string raw;            // reply as received
    std::shared_ptr<rapidjson::Document> doc;

    bool has(const char* key) const;
    std::string getString(const char* key, const std::string& def = std::string()) const;
    bool getBool(const char* key, bool def = false) const;
    int getInt(const char* key, int def = 0) const;
    HiveResultAPI resultAPI() const;    // parses the "resultAPI" object (defaults to UNKNOWN when absent)
};

// Request/reply plumbing for the Hive native bridge.
//  - callSync: one blocking call, the reply is the return value.
//  - callAsync: assigns a handlerId (>= 1); the SDK answers on its own thread through the transport callback.
//    The reply is handed to `Dispatcher`, which must run it on the thread the game wants (the cocos thread).
class HiveBridge
{
public:
    using Dispatcher = std::function<void(std::function<void()>)>;
    using Callback = std::function<void(const HiveResponse&)>;

    struct Options
    {
        std::string engine;                 // omitted from requests when empty
        std::string platform = "cpp";       // the legacy hive:: C++ adapter sends "cpp"; Unreal sends "unreal"
    };

    HiveBridge(std::shared_ptr<IHiveTransport> transport, Dispatcher dispatcher, Options options = Options());
    ~HiveBridge();

    bool start(std::string* error = nullptr);
    void shutdown();
    bool isStarted() const { return _started; }

    HiveResponse callSync(const std::string& cls, const std::string& method, const HiveParams& params = HiveParams());
    bool callAsync(const std::string& cls, const std::string& method, const HiveParams& params, Callback callback);

    // Events are native messages without a handlerId. Key is "class:method" or "class:method:uniqueKey".
    void setEventListener(const std::string& key, Callback callback);
    void removeEventListener(const std::string& key);

    // Exposed for tests.
    static std::string buildRequest(const std::string& cls, const std::string& method, int handlerId,
                                    const Options& options, const HiveParams& params);

private:
    void onNativeMessage(const std::string& json);
    static HiveResponse parse(const std::string& json);
    void dispatch(std::function<void()> fn);

    std::shared_ptr<IHiveTransport> _transport;
    Dispatcher _dispatcher;
    Options _options;
    bool _started = false;

    std::atomic<int> _nextHandlerId;
    std::mutex _mutex;
    std::map<int, Callback> _handlers;
    std::map<std::string, Callback> _listeners;
};

} // namespace hivecocos

#endif // HIVECOCOS_HIVE_BRIDGE_H
