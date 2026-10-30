#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__

#include "cocos2d.h"

#include <algorithm>
#include <vector>

// 떨어지는 물체 피하기. 한 판이 곧 한 씬이며, 재시작은 새 씬으로 교체한다.
class GameScene : public cocos2d::Layer
{
public:
    static cocos2d::Scene* createScene();

    virtual bool init() override;
    virtual void update(float dt) override;

    CREATE_FUNC(GameScene);

private:
    enum class State { Playing, GameOver };

    struct Obstacle
    {
        cocos2d::DrawNode* node;
        cocos2d::Size size;
        float speed;
    };

    void setupPlayer();
    void setupHud();
    void setupInput();

    void updatePlayer(float dt);
    void updateSpawn(float dt);
    void updateObstacles(float dt);
    void spawnObstacle();

    float currentSpawnInterval() const;
    float currentFallSpeed() const;
    cocos2d::Rect playerHitRect() const;

    void addScore();
    void gameOver();
    bool canRestart() const;
    void restart();

    State _state = State::Playing;
    bool _restarting = false;
    float _elapsed = 0.0f;
    float _gameOverElapsed = 0.0f;
    float _spawnTimer = 0.0f;
    int _score = 0;
    int _bestScore = 0;

    cocos2d::DrawNode* _player = nullptr;
    cocos2d::Label* _scoreLabel = nullptr;
    cocos2d::Label* _bestLabel = nullptr;
    cocos2d::Label* _hiveLabel = nullptr;     // Hive SDK 상태 한 줄 (Windows 빌드에서만 사용)
    std::string _hiveStatusShown;
    cocos2d::Label* _loginLabel = nullptr;    // Hive 로그인 상태 한 줄 (Windows 빌드에서만 사용)
    std::string _loginStatusShown;
    cocos2d::Node* _gameOverPanel = nullptr;
    std::vector<Obstacle> _obstacles;

    bool _leftDown = false;
    bool _rightDown = false;
    bool _touching = false;
    float _touchX = 0.0f;
};

#endif // __GAME_SCENE_H__
