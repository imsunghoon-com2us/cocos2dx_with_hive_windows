#ifndef HIVECOCOS_HIVE_TRANSPORT_H
#define HIVECOCOS_HIVE_TRANSPORT_H

#include <functional>
#include <string>

namespace hivecocos {

// The three-function native surface of HIVE_PLUGIN_CPP.dll, behind an interface so the bridge can be
// tested without the SDK (FakeTransport) and without cocos2d-x.
class IHiveTransport
{
public:
    // Called by the SDK on its own thread for async replies and events (UTF-8 JSON).
    using MessageCallback = std::function<void(const std::string& json)>;

    virtual ~IHiveTransport() {}

    // Loads the native side and registers the message callback. Returns false and fills `error` on failure.
    virtual bool start(MessageCallback onMessage, std::string* error) = 0;

    // Synchronous call. Returns the JSON reply, or an empty string when the native side returned nothing.
    virtual std::string call(const std::string& requestJson) = 0;

    // Detaches the message callback. Safe to call more than once.
    virtual void stop() = 0;
};

} // namespace hivecocos

#endif // HIVECOCOS_HIVE_TRANSPORT_H
