#include "ModalDialog.h"

#include <algorithm>
#include <chrono>

USING_NS_CC;

namespace
{
const char* kFont = "fonts/arial.ttf";
const float kKeyGuardSeconds = 0.5f;        // inputBlocked() after a dialog closed
const float kOwnKeyDelay = 0.3f;            // a new dialog ignores Enter/Space/Esc for this long (keys held from before)
const float kKeyReleaseDelay = 0.25f;       // a choice made by key runs its callback this much later (see choose())

int g_openDialogs = 0;
double g_lastClosedAt = -1000.0;

double nowSeconds()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

Node* buttonVisual(const std::string& text, const Size& size, bool pressed, int fontSize)
{
    // Origin at the bottom-left corner: MenuItemSprite anchors its images there.
    auto node = Node::create();
    node->setContentSize(size);

    auto box = DrawNode::create();
    box->drawSolidRect(Vec2::ZERO, Vec2(size.width, size.height),
                       pressed ? Color4F(0.30f, 0.50f, 0.85f, 1.0f) : Color4F(0.20f, 0.32f, 0.55f, 1.0f));
    box->drawRect(Vec2::ZERO, Vec2(size.width, size.height), Color4F(0.65f, 0.75f, 0.95f, 1.0f));
    node->addChild(box);

    auto label = Label::createWithTTF(text, kFont, static_cast<float>(fontSize));
    label->setPosition(Vec2(size.width * 0.5f, size.height * 0.5f));
    node->addChild(label);
    return node;
}
}

ModalDialog* ModalDialog::create(const std::string& message, const std::vector<std::string>& buttons, Choice onChoice)
{
    auto dialog = new (std::nothrow) ModalDialog();
    if (dialog && dialog->init(message, buttons, std::move(onChoice)))
    {
        dialog->autorelease();
        return dialog;
    }
    CC_SAFE_DELETE(dialog);
    return nullptr;
}

ModalDialog::~ModalDialog()
{
    if (!_closed)
    {
        --g_openDialogs;    // removed without a choice (e.g. the scene was replaced)
    }
}

bool ModalDialog::isOpen()
{
    return g_openDialogs > 0;
}

bool ModalDialog::inputBlocked()
{
    return g_openDialogs > 0 || nowSeconds() - g_lastClosedAt < kKeyGuardSeconds;
}

MenuItem* ModalDialog::makeButton(const std::string& text, const Size& size,
                                  const std::function<void(Ref*)>& onClick, int fontSize)
{
    return MenuItemSprite::create(buttonVisual(text, size, false, fontSize), buttonVisual(text, size, true, fontSize), onClick);
}

bool ModalDialog::init(const std::string& message, const std::vector<std::string>& buttons, Choice onChoice)
{
    const Size visible = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();
    if (buttons.empty() || !LayerColor::initWithColor(Color4B(0, 0, 0, 170), visible.width, visible.height))
    {
        return false;
    }
    setPosition(origin);
    _onChoice = std::move(onChoice);
    _buttonCount = static_cast<int>(buttons.size());
    _createdAt = nowSeconds();
    ++g_openDialogs;

    // The panel is sized to the message.
    const float panelW = std::min(420.0f, visible.width - 40.0f);
    auto text = Label::createWithTTF(message, kFont, 22, Size(panelW - 40.0f, 0.0f), TextHAlignment::CENTER);
    const float panelH = text->getContentSize().height + 120.0f;
    const Vec2 center(visible.width * 0.5f, visible.height * 0.5f);

    auto panel = DrawNode::create();
    panel->drawSolidRect(center - Vec2(panelW * 0.5f, panelH * 0.5f), center + Vec2(panelW * 0.5f, panelH * 0.5f),
                         Color4F(0.11f, 0.13f, 0.20f, 1.0f));
    panel->drawRect(center - Vec2(panelW * 0.5f, panelH * 0.5f), center + Vec2(panelW * 0.5f, panelH * 0.5f),
                    Color4F(0.65f, 0.75f, 0.95f, 1.0f));
    addChild(panel);

    text->setPosition(center + Vec2(0.0f, 36.0f));
    addChild(text);

    Vector<MenuItem*> items;
    const Size buttonSize(buttons.size() > 1 ? 130.0f : 150.0f, 42.0f);
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const int index = static_cast<int>(i);
        items.pushBack(makeButton(buttons[i], buttonSize, [this, index](Ref*) { choose(index); }));
    }
    auto menu = Menu::createWithArray(items);
    menu->alignItemsHorizontallyWithPadding(24.0f);
    menu->setPosition(center + Vec2(0.0f, -panelH * 0.5f + 40.0f));
    addChild(menu);

    // Swallow every touch that is not on a button (the buttons' own menu listener has the higher priority).
    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [](Touch*, Event*) { return true; };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);

    auto keyboard = EventListenerKeyboard::create();
    keyboard->onKeyPressed = [this](EventKeyboard::KeyCode code, Event*) {
        if (_closed || nowSeconds() - _createdAt < kOwnKeyDelay) return;
        if (code == EventKeyboard::KeyCode::KEY_ENTER || code == EventKeyboard::KeyCode::KEY_KP_ENTER ||
            code == EventKeyboard::KeyCode::KEY_SPACE)
        {
            choose(0, true);
        }
        else if (code == EventKeyboard::KeyCode::KEY_ESCAPE)
        {
            choose(_buttonCount - 1, true);
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keyboard, this);
    return true;
}

void ModalDialog::choose(int index, bool byKey)
{
    if (_closed) return;
    _closed = true;
    --g_openDialogs;
    g_lastClosedAt = nowSeconds();

    auto callback = std::move(_onChoice);
    _onChoice = nullptr;
    setVisible(false);

    // Removing the dialog right now would destroy the menu whose callback is running: do it a frame later.
    if (!callback)
    {
        runAction(Sequence::create(DelayTime::create(0.0f), RemoveSelf::create(), nullptr));
    }
    else if (byKey)
    {
        // The callback may bring up the Hive UI, which takes over the keyboard at once and swallows the key release.
        // GLFW would then keep the key "pressed" and the next press of the same key would arrive as a repeat, which
        // cocos2d-x ignores. Let the release reach the game first.
        runAction(Sequence::create(DelayTime::create(kKeyReleaseDelay),
                                   CallFunc::create([callback, index]() { callback(index); }),
                                   RemoveSelf::create(), nullptr));
    }
    else
    {
        runAction(Sequence::create(DelayTime::create(0.0f), RemoveSelf::create(), nullptr));
        callback(index);
    }
}
