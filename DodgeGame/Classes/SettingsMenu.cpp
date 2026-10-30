#include "SettingsMenu.h"

#include "HiveService.h"
#include "ModalDialog.h"
#include "TitleScene.h"

USING_NS_CC;

namespace
{
const Size kPanelButtonSize(160.0f, 34.0f);
const float kPanelPadding = 8.0f;

void showMessage(const std::string& text)
{
    auto scene = Director::getInstance()->getRunningScene();
    auto dialog = ModalDialog::create(text, { "OK" }, nullptr);
    if (scene && dialog)
    {
        scene->addChild(dialog, 1000);
    }
}

void goToTitle()
{
    // After a sign-out the player decides when to sign in again: the title waits for a key press.
    Director::getInstance()->replaceScene(TitleScene::createScene(false));
}
}

bool SettingsMenu::init()
{
    if (!Node::init())
    {
        return false;
    }

    const Size visible = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();

    _buttonSize = Size(96.0f, 30.0f);
    _buttonCenter = Vec2(origin.x + visible.width - 12.0f - _buttonSize.width * 0.5f,
                         origin.y + visible.height - 10.0f - _buttonSize.height * 0.5f);

    auto button = ModalDialog::makeButton("Settings", _buttonSize, [this](Ref*) { togglePanel(); }, 16);
    button->setPosition(_buttonCenter);
    auto menu = Menu::create(button, nullptr);
    menu->setPosition(Vec2::ZERO);
    addChild(menu);

    _panel = Node::create();
    _panel->setVisible(false);
    addChild(_panel);
    return true;
}

void SettingsMenu::closePanel()
{
    // Only hide: this is called from the panel's own menu callbacks, which must not be destroyed while they run.
    // The old buttons are removed when the panel is opened again.
    if (_panel)
    {
        _panel->setVisible(false);
    }
}

void SettingsMenu::togglePanel()
{
    // Ignore the click while a popup is open (the button's menu may still see touches that are meant for the popup)
    // or while the Hive SDK is working on a request.
    if (ModalDialog::isOpen() || HiveService::instance().isBusy())
    {
        return;
    }
    if (isPanelOpen())
    {
        closePanel();
    }
    else
    {
        openPanel();
    }
}

void SettingsMenu::openPanel()
{
    _panel->removeAllChildren();
    HiveService& hive = HiveService::instance();

    Vector<MenuItem*> items;
    if (hive.canSignOut())
    {
        items.pushBack(ModalDialog::makeButton("Log out", kPanelButtonSize, [this](Ref*) { onLogout(); }, 18));
    }
    items.pushBack(ModalDialog::makeButton("Delete account", kPanelButtonSize, [this](Ref*) { onDeleteAccount(); }, 18));
    items.pushBack(ModalDialog::makeButton("Reset terms", kPanelButtonSize, [this](Ref*) { onResetTerms(); }, 18));

    const float width = kPanelButtonSize.width + kPanelPadding * 2.0f;
    const float height = items.size() * kPanelButtonSize.height + (items.size() + 1) * kPanelPadding;
    const float right = _buttonCenter.x + _buttonSize.width * 0.5f;
    const float top = _buttonCenter.y - _buttonSize.height * 0.5f - 6.0f;
    const Vec2 center(right - width * 0.5f, top - height * 0.5f);

    auto box = DrawNode::create();
    box->drawSolidRect(center - Vec2(width * 0.5f, height * 0.5f), center + Vec2(width * 0.5f, height * 0.5f),
                       Color4F(0.11f, 0.13f, 0.20f, 0.97f));
    box->drawRect(center - Vec2(width * 0.5f, height * 0.5f), center + Vec2(width * 0.5f, height * 0.5f),
                  Color4F(0.65f, 0.75f, 0.95f, 1.0f));
    _panel->addChild(box);

    auto menu = Menu::createWithArray(items);
    menu->alignItemsVerticallyWithPadding(kPanelPadding);
    menu->setPosition(center);
    _panel->addChild(menu);
    _panel->setVisible(true);
}

void SettingsMenu::onLogout()
{
    closePanel();
    if (ModalDialog::isOpen()) return;

    // The callbacks capture nothing: the scene may already be gone when the SDK answers.
    HiveService::instance().requestSignOut([](bool ok, const std::string& reason) {
        if (ok)
        {
            goToTitle();
        }
        else
        {
            showMessage("Log out failed (" + reason + ").");
        }
    });
}

void SettingsMenu::onResetTerms()
{
    closePanel();
    if (ModalDialog::isOpen()) return;

    // Test aid: AuthV4.resetAgreement clears the SDK's stored terms agreement data.
    if (HiveService::instance().resetAgreement())
    {
        showMessage("Terms agreement data was reset.\nRestart the game to see the terms.");
    }
    else
    {
        showMessage("Reset failed (Hive SDK not available).");
    }
}

void SettingsMenu::onDeleteAccount()
{
    closePanel();
    if (ModalDialog::isOpen()) return;

    auto scene = Director::getInstance()->getRunningScene();
    auto dialog = ModalDialog::create("Delete your account?\nThis cannot be undone.", { "Delete", "Cancel" }, [](int index) {
        if (index != 0) return;     // Cancel / Esc

        HiveService::instance().requestDeleteAccount([](bool ok, const std::string& reason) {
            if (ok)
            {
                goToTitle();
            }
            else
            {
                showMessage("Account deletion failed (" + reason + ").");
            }
        });
    });
    if (scene && dialog)
    {
        scene->addChild(dialog, 1000);
    }
}
