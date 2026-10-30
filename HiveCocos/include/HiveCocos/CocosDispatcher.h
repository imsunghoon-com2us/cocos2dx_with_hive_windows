#ifndef HIVECOCOS_COCOS_DISPATCHER_H
#define HIVECOCOS_COCOS_DISPATCHER_H

#include "HiveCocos/HiveBridge.h"

namespace hivecocos {

// Hands work from the SDK's threads to the cocos2d-x thread (runs on the next frame update).
// This is the only place in HiveCocos that depends on cocos2d-x.
HiveBridge::Dispatcher makeCocosDispatcher();

} // namespace hivecocos

#endif // HIVECOCOS_COCOS_DISPATCHER_H
