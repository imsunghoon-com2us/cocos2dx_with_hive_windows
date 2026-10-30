#include "TitleScene.h"

#include "GameScene.h"
#include "HiveService.h"
#include "ModalDialog.h"

USING_NS_CC;

namespace
{
const char* kFont = "fonts/arial.ttf";
}

Scene* TitleScene::createScene(bool autoSignIn)
{
    auto scene = Scene::create();
    scene->addChild(TitleScene::create(autoSignIn));
    return scene;
}

TitleScene* TitleScene::create(bool autoSignIn)
{
    auto scene = new (std::nothrow) TitleScene();
    if (scene && scene->init(autoSignIn))
    {
        scene->autorelease();
        return scene;
    }
    CC_SAFE_DELETE(scene);
    return nullptr;
}

bool TitleScene::init(bool autoSignIn)
{
    if (!Layer::init())
    {
        return false;
    }
    _autoSignIn = autoSignIn;

    const Size visible = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();
    const Vec2 center(origin.x + visible.width * 0.5f, origin.y + visible.height * 0.5f);

    addChild(LayerColor::create(Color4B(24, 28, 40, 255)), -1);

    auto title = Label::createWithTTF("DODGE", kFont, 64);
    title->setPosition(center + Vec2(0.0f, 110.0f));
    addChild(title);

    auto subtitle = Label::createWithTTF("Avoid the falling blocks", kFont, 20);
    subtitle->setTextColor(Color4B(160, 170, 190, 255));
    subtitle->setPosition(center + Vec2(0.0f, 60.0f));
    addChild(subtitle);

    _hiveLabel = Label::createWithTTF("", kFont, 14);
    _hiveLabel->setTextColor(Color4B(160, 170, 190, 255));
    _hiveLabel->setPosition(center + Vec2(0.0f, -20.0f));
    addChild(_hiveLabel);

    _loginLabel = Label::createWithTTF("", kFont, 14);
    _loginLabel->setTextColor(Color4B(120, 200, 150, 255));
    _loginLabel->setPosition(center + Vec2(0.0f, -42.0f));
    addChild(_loginLabel);

    _promptLabel = Label::createWithTTF("", kFont, 26);
    _promptLabel->setPosition(center + Vec2(0.0f, -110.0f));
    addChild(_promptLabel);
    _promptLabel->runAction(RepeatForever::create(Sequence::create(FadeTo::create(0.7f, 110), FadeTo::create(0.7f, 255), nullptr)));

    auto keyboard = EventListenerKeyboard::create();
    keyboard->onKeyPressed = [this](EventKeyboard::KeyCode code, Event*) {
        if (ModalDialog::inputBlocked()) return;            // a popup is open (or just closed): not for the title
        if (HiveService::instance().isBusy()) return;       // the Hive UI is up: its keys are not ours
        if (code == EventKeyboard::KeyCode::KEY_ESCAPE)
        {
            Director::getInstance()->end();
            return;
        }
        onAction(true);
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keyboard, this);

    auto touch = EventListenerTouchOneByOne::create();
    touch->onTouchBegan = [this](Touch*, Event*) {
        if (!ModalDialog::inputBlocked() && !HiveService::instance().isBusy())
        {
            onAction(false);
        }
        return true;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);

    scheduleUpdate();
    return true;
}

void TitleScene::onEnter()
{
    Layer::onEnter();
    if (_autoSignIn)
    {
        HiveService::instance().start();    // idempotent
    }
    refreshTexts();
}

void TitleScene::onAction(bool byKey)
{
    if (_leaving) return;

    HiveService& hive = HiveService::instance();
    if (hive.isSignedIn())
    {
        startGame();
    }
    else if (!hive.hasNotice())
    {
        // After a sign-out: the player asked for it. The Hive UI takes over the keyboard as soon as it shows and swallows
        // the key release; GLFW would then ignore the next press of this key. So a key press starts the sign-in a
        // moment later, after the release has reached the game.
        if (!byKey)
        {
            hive.requestSignIn();
        }
        else if (!_signInScheduled)
        {
            _signInScheduled = true;
            scheduleOnce([this](float) {
                _signInScheduled = false;
                HiveService::instance().requestSignIn();
            }, 0.25f, "title_sign_in");
        }
    }
}

void TitleScene::startGame()
{
    _leaving = true;
    Director::getInstance()->replaceScene(GameScene::createScene());
}

void TitleScene::update(float)
{
    refreshTexts();
    showNotice();
}

void TitleScene::refreshTexts()
{
    HiveService& hive = HiveService::instance();

    if (_hiveShown != hive.status())
    {
        _hiveShown = hive.status();
        _hiveLabel->setString(_hiveShown);
    }
    if (_loginShown != hive.loginStatus())
    {
        _loginShown = hive.loginStatus();
        _loginLabel->setString(_loginShown);
    }

    std::string prompt;
    if (hive.isSignedIn())
    {
        prompt = "Press any key to start";
    }
    else if (hive.isBusy())
    {
        prompt = "Please wait...";
    }
    else if (!hive.hasNotice())
    {
        prompt = "Press any key to sign in";    // signed out: waiting for the player
    }
    if (_promptShown != prompt)
    {
        _promptShown = prompt;
        _promptLabel->setString(prompt);
    }
}

void TitleScene::showNotice()
{
    HiveService& hive = HiveService::instance();
    if (!hive.hasNotice() || _noticeShown || _leaving)
    {
        return;
    }

    _noticeShown = true;
    auto dialog = ModalDialog::create(hive.notice().text, { "OK" }, [this](int) {
        _noticeShown = false;
        HiveService::instance().acknowledgeNotice();    // retries, or ends the game when the notice is fatal
    });
    if (dialog)
    {
        addChild(dialog, 1000);
    }
    else
    {
        _noticeShown = false;
    }
}
