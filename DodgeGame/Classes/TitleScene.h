#ifndef __TITLE_SCENE_H__
#define __TITLE_SCENE_H__

#include "cocos2d.h"

#include <string>

// Title screen: initializes the Hive SDK and signs in, then waits for a key (or a click) to start the game.
//
//   createScene(true)   app start: starts the Hive SDK, which initializes and then signs in by itself
//   createScene(false)  after a sign-out / account deletion: shows "press any key to sign in" and signs in only when the
//                       player presses a key. (After "Reset terms" the Hive initialization is already running again,
//                       so the title first shows its progress, and the sign-in follows by itself like at the start.)
//
// Failures reported by HiveService (initialization, sign-in) are shown as popups here (see HiveService.h).
class TitleScene : public cocos2d::Layer
{
public:
    static cocos2d::Scene* createScene(bool autoSignIn);
    static TitleScene* create(bool autoSignIn);

    virtual void onEnter() override;
    virtual void update(float dt) override;

private:
    bool init(bool autoSignIn);
    void onAction(bool byKey);      // a key press or a click
    void startGame();
    void refreshTexts();
    void showNotice();

    bool _autoSignIn = true;
    bool _leaving = false;
    bool _noticeShown = false;
    bool _signInScheduled = false;

    cocos2d::Label* _hiveLabel = nullptr;
    cocos2d::Label* _loginLabel = nullptr;
    cocos2d::Label* _promptLabel = nullptr;
    std::string _hiveShown;
    std::string _loginShown;
    std::string _promptShown;
};

#endif // __TITLE_SCENE_H__
