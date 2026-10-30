#include "HiveCocos/FakeTransport.h"

#include <thread>
#include <utility>

namespace hivecocos {

bool FakeTransport::start(MessageCallback onMessage, std::string* error)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_startError.empty())
    {
        if (error) *error = _startError;
        return false;
    }
    _callback = std::move(onMessage);
    return true;
}

std::string FakeTransport::call(const std::string& requestJson)
{
    Responder responder;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _requests.push_back(requestJson);
        responder = _responder;
    }
    return responder ? responder(requestJson) : std::string();
}

void FakeTransport::stop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _callback = nullptr;
}

void FakeTransport::setResponder(Responder responder)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _responder = std::move(responder);
}

std::vector<std::string> FakeTransport::requests() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _requests;
}

void FakeTransport::emit(const std::string& json)
{
    MessageCallback cb;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        cb = _callback;
    }
    if (cb) cb(json);
}

void FakeTransport::emitFromOtherThread(const std::string& json)
{
    std::thread t([this, json]() { emit(json); });
    t.join();
}

} // namespace hivecocos
