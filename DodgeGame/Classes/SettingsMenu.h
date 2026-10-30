#ifndef __SETTINGS_MENU_H__
#define __SETTINGS_MENU_H__

#include "cocos2d.h"

// The "Settings" button in the top-right corner of the game screen and its small drop-down menu:
//   Log out          only for a linked (non-guest) account: a guest cannot sign in again after signing out
//   Delete account   after a Delete / Cancel confirmation (irreversible)
// A successful sign-out or deletion returns to the title screen, where a key press starts a new sign-in.
// While the drop-down is open the game is paused (GameScene asks isPanelOpen()).
class SettingsMenu : public cocos2d::Node
{
public:
    CREATE_FUNC(SettingsMenu);

    virtual bool init() override;

    bool isPanelOpen() const { return _panel && _panel->isVisible(); }
    void closePanel();

private:
    void togglePanel();
    void openPanel();       // rebuilt every time: the buttons depend on the account type
    void onLogout();
    void onDeleteAccount();

    cocos2d::Node* _panel = nullptr;
    cocos2d::Vec2 _buttonCenter;
    cocos2d::Size _buttonSize;
};

#endif // __SETTINGS_MENU_H__
