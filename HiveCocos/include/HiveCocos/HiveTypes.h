#ifndef HIVECOCOS_HIVE_TYPES_H
#define HIVECOCOS_HIVE_TYPES_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace hivecocos {

// A flat request parameter value (the native bridge takes a flat JSON object).
class HiveValue
{
public:
    enum class Type { String, Number, Bool };

    HiveValue(const char* v) : _type(Type::String), _string(v) {}
    HiveValue(std::string v) : _type(Type::String), _string(std::move(v)) {}
    HiveValue(double v) : _type(Type::Number), _number(v) {}
    HiveValue(int v) : _type(Type::Number), _number(v) {}
    HiveValue(bool v) : _type(Type::Bool), _bool(v) {}

    Type type() const { return _type; }
    const std::string& asString() const { return _string; }
    double asNumber() const { return _number; }
    bool asBool() const { return _bool; }

private:
    Type _type;
    std::string _string;
    double _number = 0.0;
    bool _bool = false;
};

using HiveParams = std::vector<std::pair<std::string, HiveValue>>;

// Mirrors the "resultAPI" object of native replies.
struct HiveResultAPI
{
    int errorCode = -99;        // EErrorCode::UNKNOWN until the reply says otherwise
    int code = -999;            // ECode::CommonUnknown
    std::string errorMessage;
    std::string message;
    int64_t latencyMs = 0;

    // Same rule as the Unreal wrapper (FHiveResultAPI::IsSuccess).
    bool isSuccess() const { return errorCode >= 0 || code >= 0; }
    std::string toString() const;
};

enum class HiveZone { Real, Sandbox };

} // namespace hivecocos

#endif // HIVECOCOS_HIVE_TYPES_H
