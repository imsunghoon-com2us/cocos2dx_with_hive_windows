#ifndef __MODAL_DIALOG_H__
#define __MODAL_DIALOG_H__

#include "cocos2d.h"

#include <functional>
#include <string>
#include <vector>

// A popup with a message and one or two buttons, shown above everything else (add it to the scene with a high z-order).
// It swallows touches and, while it is open, key presses are meant for it only: scenes ask inputBlocked() first.
//
// Buttons are picked with the mouse, Enter/Space (first button) or Esc (last button). The callback runs once with the
// index of the chosen button (0.25 s after a key press, so that the key release reaches the game before anything the
// callback starts, e.g. the Hive UI, can swallow it); the dialog removes itself afterwards (never inside its own menu
// callback).
class ModalDialog : public cocos2d::LayerColor
{
public:
    using Choice = std::function<void(int index)>;

    static ModalDialog* create(const std::string& message, const std::vector<std::string>& buttons, Choice onChoice);
    ~ModalDialog() override;

    // True while a dialog is on screen.
    static bool isOpen();
    // True while a dialog is open and for a short time after it closed. Held keys repeat, and the key that closed a
    // dialog is still delivered to the scene underneath, so scenes ignore key presses (and Esc) during this time.
    static bool inputBlocked();

    // A text button for cocos2d::Menu (also used by the settings menu).
    static cocos2d::MenuItem* makeButton(const std::string& text, const cocos2d::Size& size,
                                         const std::function<void(cocos2d::Ref*)>& onClick, int fontSize = 22);

private:
    bool init(const std::string& message, const std::vector<std::string>& buttons, Choice onChoice);
    void choose(int index, bool byKey = false);

    Choice _onChoice;
    int _buttonCount = 0;
    bool _closed = false;
    double _createdAt = 0.0;
};

#endif // __MODAL_DIALOG_H__
