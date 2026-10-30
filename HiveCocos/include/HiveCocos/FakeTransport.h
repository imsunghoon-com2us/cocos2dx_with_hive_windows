#ifndef HIVECOCOS_FAKE_TRANSPORT_H
#define HIVECOCOS_FAKE_TRANSPORT_H

#include "HiveCocos/HiveTransport.h"

#include <mutex>
#include <string>
#include <vector>

namespace hivecocos {

// Test double: records requests, answers synchronously through a responder, and lets a test push
// async messages as if the SDK had sent them.
class FakeTransport : public IHiveTransport
{
public:
    using Responder = std::function<std::string(const std::string& requestJson)>;

    bool start(MessageCallback onMessage, std::string* error) override;
    std::string call(const std::string& requestJson) override;
    void stop() override;

    void setStartFailure(const std::string& error) { _startError = error; }
    void setResponder(Responder responder);
    std::vector<std::string> requests() const;

    // Delivers a native message on the calling thread.
    void emit(const std::string& json);
    // Delivers a native message from a separate thread (like the real SDK) and waits for it to finish.
    void emitFromOtherThread(const std::string& json);

private:
    mutable std::mutex _mutex;
    MessageCallback _callback;
    Responder _responder;
    std::vector<std::string> _requests;
    std::string _startError;
};

} // namespace hivecocos

#endif // HIVECOCOS_FAKE_TRANSPORT_H
