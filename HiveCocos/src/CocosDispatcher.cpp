#include "HiveCocos/CocosDispatcher.h"

#include "cocos2d.h"

namespace hivecocos {

HiveBridge::Dispatcher makeCocosDispatcher()
{
    return [](std::function<void()> fn) {
        // performFunctionInCocosThread is thread-safe; the function runs in Scheduler::update on the cocos thread.
        cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread(std::move(fn));
    };
}

} // namespace hivecocos
