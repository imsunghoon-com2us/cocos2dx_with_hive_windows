#include "HiveCocos/HiveBridge.h"

#include "json/stringbuffer.h"
#include "json/writer.h"

#include <cmath>
#include <sstream>
#include <type_traits>
#include <utility>

namespace hivecocos {

std::string HiveResultAPI::toString() const
{
    std::ostringstream os;
    os << "ResultAPI { errorCode=" << errorCode << ", code=" << code << ", errorMessage=" << errorMessage
       << ", message=" << message << ", latencyMs=" << latencyMs << " }";
    return os.str();
}

// ---- HiveResponse ----

static const rapidjson::Value* findMember(const HiveResponse& r, const char* key)
{
    if (!r.doc || !r.doc->IsObject()) return nullptr;
    auto it = r.doc->FindMember(key);
    return it == r.doc->MemberEnd() ? nullptr : &it->value;
}

bool HiveResponse::has(const char* key) const { return findMember(*this, key) != nullptr; }

std::string HiveResponse::getString(const char* key, const std::string& def) const
{
    const rapidjson::Value* v = findMember(*this, key);
    return (v && v->IsString()) ? std::string(v->GetString(), v->GetStringLength()) : def;
}

bool HiveResponse::getBool(const char* key, bool def) const
{
    const rapidjson::Value* v = findMember(*this, key);
    return (v && v->IsBool()) ? v->GetBool() : def;
}

int HiveResponse::getInt(const char* key, int def) const
{
    const rapidjson::Value* v = findMember(*this, key);
    return (v && v->IsNumber()) ? v->GetInt() : def;
}

HiveResultAPI HiveResponse::resultAPI() const
{
    HiveResultAPI r;
    const rapidjson::Value* v = findMember(*this, "resultAPI");
    if (!v || !v->IsObject()) return r;

    auto num = [&](const char* k, auto& out) {
        auto it = v->FindMember(k);
        if (it != v->MemberEnd() && it->value.IsNumber()) out = static_cast<std::remove_reference_t<decltype(out)>>(it->value.GetInt64());
    };
    auto str = [&](const char* k, std::string& out) {
        auto it = v->FindMember(k);
        if (it != v->MemberEnd() && it->value.IsString()) out.assign(it->value.GetString(), it->value.GetStringLength());
    };
    num("errorCode", r.errorCode);
    num("code", r.code);
    num("latencyMs", r.latencyMs);
    str("errorMessage", r.errorMessage);
    str("message", r.message);
    return r;
}

// ---- HiveBridge ----

HiveBridge::HiveBridge(std::shared_ptr<IHiveTransport> transport, Dispatcher dispatcher, Options options)
    : _transport(std::move(transport))
    , _dispatcher(std::move(dispatcher))
    , _options(std::move(options))
    , _nextHandlerId(1)
{
}

HiveBridge::~HiveBridge()
{
    shutdown();
}

bool HiveBridge::start(std::string* error)
{
    if (_started) return true;
    if (!_transport)
    {
        if (error) *error = "no transport";
        return false;
    }
    _started = _transport->start([this](const std::string& json) { onNativeMessage(json); }, error);
    return _started;
}

void HiveBridge::shutdown()
{
    if (!_started) return;
    _started = false;
    _transport->stop();
    std::lock_guard<std::mutex> lock(_mutex);
    _handlers.clear();
    _listeners.clear();
}

std::string HiveBridge::buildRequest(const std::string& cls, const std::string& method, int handlerId,
                                     const Options& options, const HiveParams& params)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> w(sb);
    w.StartObject();
    w.Key("class");     w.String(cls.c_str(), static_cast<rapidjson::SizeType>(cls.size()));
    w.Key("method");    w.String(method.c_str(), static_cast<rapidjson::SizeType>(method.size()));
    w.Key("handlerId"); w.Int(handlerId);
    if (!options.engine.empty())
    {
        w.Key("engine");
        w.String(options.engine.c_str(), static_cast<rapidjson::SizeType>(options.engine.size()));
    }
    w.Key("platform");  w.String(options.platform.c_str(), static_cast<rapidjson::SizeType>(options.platform.size()));
    for (const auto& p : params)
    {
        w.Key(p.first.c_str(), static_cast<rapidjson::SizeType>(p.first.size()));
        switch (p.second.type())
        {
        case HiveValue::Type::String: w.String(p.second.asString().c_str(), static_cast<rapidjson::SizeType>(p.second.asString().size())); break;
        case HiveValue::Type::Number:
        {
            // Integral values are written as integers ("5", not "5.0"), as the Unreal wrapper does.
            const double d = p.second.asNumber();
            if (d == std::floor(d) && std::fabs(d) < 1e15) w.Int64(static_cast<int64_t>(d));
            else w.Double(d);
            break;
        }
        case HiveValue::Type::Bool:   w.Bool(p.second.asBool()); break;
        }
    }
    w.EndObject();
    return sb.GetString();
}

HiveResponse HiveBridge::parse(const std::string& json)
{
    HiveResponse r;
    r.raw = json;
    if (json.empty())
    {
        r.error = "empty reply";
        return r;
    }
    auto doc = std::make_shared<rapidjson::Document>();
    doc->Parse(json.c_str());
    if (doc->HasParseError() || !doc->IsObject())
    {
        r.error = "invalid JSON reply";
        return r;
    }
    r.doc = doc;
    r.ok = true;
    return r;
}

HiveResponse HiveBridge::callSync(const std::string& cls, const std::string& method, const HiveParams& params)
{
    if (!_started)
    {
        HiveResponse r;
        r.error = "bridge not started";
        return r;
    }
    // handlerId -1: a synchronous call has no async handler.
    return parse(_transport->call(buildRequest(cls, method, -1, _options, params)));
}

bool HiveBridge::callAsync(const std::string& cls, const std::string& method, const HiveParams& params, Callback callback)
{
    if (!_started) return false;

    int id = -1;
    if (callback)
    {
        id = _nextHandlerId++;
        std::lock_guard<std::mutex> lock(_mutex);
        _handlers[id] = std::move(callback);
    }
    // The reply of an async call arrives through the message callback; the return value is not used.
    _transport->call(buildRequest(cls, method, id, _options, params));
    return true;
}

void HiveBridge::setEventListener(const std::string& key, Callback callback)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _listeners[key] = std::move(callback);
}

void HiveBridge::removeEventListener(const std::string& key)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _listeners.erase(key);
}

void HiveBridge::dispatch(std::function<void()> fn)
{
    if (_dispatcher) _dispatcher(std::move(fn));
    else fn();
}

// Runs on the SDK's thread.
void HiveBridge::onNativeMessage(const std::string& json)
{
    if (json.empty()) return;
    HiveResponse resp = parse(json);
    if (!resp.ok) return;

    Callback handler;
    const int handlerId = resp.getInt("handlerId", -1);
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (handlerId >= 1)
        {
            auto it = _handlers.find(handlerId);
            if (it == _handlers.end()) return;              // unknown or already answered
            handler = std::move(it->second);
            _handlers.erase(it);
        }
        else if (resp.has("class") && resp.has("method"))
        {
            std::string key = resp.getString("class") + ":" + resp.getString("method");
            const std::string unique = resp.getString("uniqueKey");
            if (!unique.empty()) key += ":" + unique;
            auto it = _listeners.find(key);
            if (it == _listeners.end()) return;
            handler = it->second;
        }
        else
        {
            return;
        }
    }
    dispatch([handler, resp]() { handler(resp); });
}

} // namespace hivecocos
