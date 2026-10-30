#include "GameScene.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
#include "HiveService.h"
#endif

USING_NS_CC;

namespace
{
const char* kFont = "fonts/arial.ttf";
const char* kBestScoreKey = "best_score";

const Size kPlayerSize(44.0f, 44.0f);
const float kPlayerMargin = 24.0f;      // 화면 아래쪽 여백(플레이어 아랫변까지의 거리)
const float kPlayerSpeed = 340.0f;      // px/s
const float kHitShrink = 6.0f;          // 플레이어 판정 영역을 한 변당 줄이는 양

const float kMinObstacle = 24.0f;
const float kMaxObstacle = 56.0f;
const int kScorePerDodge = 10;

// 창 이동·크기 조절로 루프가 멈췄다 돌아오면 dt가 매우 커진다. 장애물이 한 프레임에
// 플레이어를 통과(터널링)하고 스폰이 몰리지 않도록 한 프레임에 처리할 시간의 상한을 둔다.
const float kMaxFrameDt = 1.0f / 30.0f;
// 죽는 순간 진행 중이던 입력이 곧바로 재시작으로 이어지지 않도록 하는 지연.
const float kRestartDelay = 0.5f;
}

Scene* GameScene::createScene()
{
    auto scene = Scene::create();
    scene->addChild(GameScene::create());
    return scene;
}

bool GameScene::init()
{
    if (!Layer::init())
    {
        return false;
    }

    _bestScore = UserDefault::getInstance()->getIntegerForKey(kBestScoreKey, 0);

    // 배경
    addChild(LayerColor::create(Color4B(24, 28, 40, 255)), -1);

    setupPlayer();
    setupHud();
    setupInput();

    scheduleUpdate();
    return true;
}

void GameScene::setupPlayer()
{
    const Size visible = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();

    _player = DrawNode::create();
    const Vec2 half(kPlayerSize.width * 0.5f, kPlayerSize.height * 0.5f);
    _player->drawSolidRect(-half, half, Color4F(0.25f, 0.65f, 1.0f, 1.0f));
    _player->setPosition(Vec2(origin.x + visible.width * 0.5f,
                              origin.y + kPlayerMargin + half.y));
    addChild(_player, 1);
}

void GameScene::setupHud()
{
    const Size visible = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();

    _scoreLabel = Label::createWithTTF("Score: 0", kFont, 24);
    _scoreLabel->setAnchorPoint(Vec2(0.0f, 1.0f));
    _scoreLabel->setPosition(Vec2(origin.x + 12.0f, origin.y + visible.height - 10.0f));
    addChild(_scoreLabel, 10);

    _bestLabel = Label::createWithTTF(StringUtils::format("Best: %d", _bestScore), kFont, 24);
    _bestLabel->setAnchorPoint(Vec2(1.0f, 1.0f));
    _bestLabel->setPosition(Vec2(origin.x + visible.width - 12.0f, origin.y + visible.height - 10.0f));
    addChild(_bestLabel, 10);

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
    _hiveLabel = Label::createWithTTF("", kFont, 14);
    _hiveLabel->setAnchorPoint(Vec2(0.0f, 1.0f));
    _hiveLabel->setPosition(Vec2(origin.x + 12.0f, origin.y + visible.height - 40.0f));
    _hiveLabel->setTextColor(Color4B(160, 170, 190, 255));
    addChild(_hiveLabel, 10);
#endif
}

void GameScene::setupInput()
{
    auto keyboard = EventListenerKeyboard::create();
    keyboard->onKeyPressed = [this](EventKeyboard::KeyCode code, Event*) {
        switch (code)
        {
        case EventKeyboard::KeyCode::KEY_LEFT_ARROW:
        case EventKeyboard::KeyCode::KEY_A:
            _leftDown = true;
            break;
        case EventKeyboard::KeyCode::KEY_RIGHT_ARROW:
        case EventKeyboard::KeyCode::KEY_D:
            _rightDown = true;
            break;
        case EventKeyboard::KeyCode::KEY_R:
        case EventKeyboard::KeyCode::KEY_SPACE:
            if (canRestart())
            {
                restart();
            }
            break;
        case EventKeyboard::KeyCode::KEY_ESCAPE:
            Director::getInstance()->end();
            break;
        default:
            break;
        }
    };
    keyboard->onKeyReleased = [this](EventKeyboard::KeyCode code, Event*) {
        switch (code)
        {
        case EventKeyboard::KeyCode::KEY_LEFT_ARROW:
        case EventKeyboard::KeyCode::KEY_A:
            _leftDown = false;
            break;
        case EventKeyboard::KeyCode::KEY_RIGHT_ARROW:
        case EventKeyboard::KeyCode::KEY_D:
            _rightDown = false;
            break;
        default:
            break;
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keyboard, this);

    // 마우스(Windows에서는 터치로 변환된다): 누르고 있는 x 위치로 따라간다. 게임오버 후에는 재시작.
    auto touch = EventListenerTouchOneByOne::create();
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (_state == State::GameOver)
        {
            if (canRestart())
            {
                restart();
            }
            return true;
        }
        _touching = true;
        _touchX = t->getLocation().x;
        return true;
    };
    touch->onTouchMoved = [this](Touch* t, Event*) { _touchX = t->getLocation().x; };
    touch->onTouchEnded = [this](Touch*, Event*) { _touching = false; };
    touch->onTouchCancelled = [this](Touch*, Event*) { _touching = false; };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
}

void GameScene::update(float dt)
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
    // Hive 상태는 비동기로 바뀌므로 달라졌을 때만 라벨을 갱신한다(게임오버 중에도 표시).
    if (_hiveLabel && _hiveStatusShown != HiveService::instance().status())
    {
        _hiveStatusShown = HiveService::instance().status();
        _hiveLabel->setString(_hiveStatusShown);
    }
#endif

    if (_state == State::GameOver)
    {
        _gameOverElapsed += dt;
        return;
    }

    dt = std::min(dt, kMaxFrameDt);
    _elapsed += dt;
    updatePlayer(dt);
    updateSpawn(dt);
    updateObstacles(dt);
}

void GameScene::updatePlayer(float dt)
{
    const Size visible = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();
    const float halfW = kPlayerSize.width * 0.5f;

    float x = _player->getPositionX();
    const float step = kPlayerSpeed * dt;

    if (_touching)
    {
        const float dx = _touchX - x;
        x += clampf(dx, -step, step);
    }
    else
    {
        if (_leftDown)  { x -= step; }
        if (_rightDown) { x += step; }
    }

    x = clampf(x, origin.x + halfW, origin.x + visible.width - halfW);
    _player->setPositionX(x);
}

void GameScene::updateSpawn(float dt)
{
    _spawnTimer += dt;
    const float interval = currentSpawnInterval();
    while (_spawnTimer >= interval)
    {
        _spawnTimer -= interval;
        spawnObstacle();
    }
}

void GameScene::spawnObstacle()
{
    const Size visible = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();

    const float w = RandomHelper::random_real(kMinObstacle, kMaxObstacle);
    const float h = RandomHelper::random_real(kMinObstacle, kMaxObstacle);
    const float x = RandomHelper::random_real(origin.x + w * 0.5f, origin.x + visible.width - w * 0.5f);

    auto node = DrawNode::create();
    const Vec2 half(w * 0.5f, h * 0.5f);
    node->drawSolidRect(-half, half, Color4F(1.0f, 0.4f, 0.35f, 1.0f));
    node->setPosition(Vec2(x, origin.y + visible.height + h * 0.5f));
    addChild(node, 0);

    // 속도에 약간의 편차를 줘서 패턴이 단조롭지 않게 한다.
    const float speed = currentFallSpeed() * RandomHelper::random_real(0.85f, 1.2f);
    _obstacles.push_back({ node, Size(w, h), speed });
}

void GameScene::updateObstacles(float dt)
{
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();
    const Rect playerRect = playerHitRect();

    for (size_t i = 0; i < _obstacles.size();)
    {
        Obstacle& ob = _obstacles[i];
        ob.node->setPositionY(ob.node->getPositionY() - ob.speed * dt);

        const Vec2 p = ob.node->getPosition();
        const Rect rect(p.x - ob.size.width * 0.5f, p.y - ob.size.height * 0.5f, ob.size.width, ob.size.height);

        if (rect.intersectsRect(playerRect))
        {
            gameOver();
            return;
        }

        if (p.y + ob.size.height * 0.5f < origin.y)
        {
            // 화면 아래로 빠져나갔다 = 피했다.
            ob.node->removeFromParent();
            _obstacles.erase(_obstacles.begin() + i);
            addScore();
            continue;
        }
        ++i;
    }
}

float GameScene::currentSpawnInterval() const
{
    return std::max(0.25f, 0.9f - _elapsed * 0.015f);
}

float GameScene::currentFallSpeed() const
{
    return std::min(520.0f, 180.0f + _elapsed * 6.0f);
}

Rect GameScene::playerHitRect() const
{
    const Vec2 p = _player->getPosition();
    const float w = kPlayerSize.width - kHitShrink * 2.0f;
    const float h = kPlayerSize.height - kHitShrink * 2.0f;
    return Rect(p.x - w * 0.5f, p.y - h * 0.5f, w, h);
}

void GameScene::addScore()
{
    _score += kScorePerDodge;
    _scoreLabel->setString(StringUtils::format("Score: %d", _score));
}

void GameScene::gameOver()
{
    _state = State::GameOver;
    _touching = false;

    const bool newBest = _score > _bestScore;
    if (newBest)
    {
        _bestScore = _score;
        UserDefault::getInstance()->setIntegerForKey(kBestScoreKey, _bestScore);
        UserDefault::getInstance()->flush();
        _bestLabel->setString(StringUtils::format("Best: %d", _bestScore));
    }

    const Size visible = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();
    const Vec2 center(origin.x + visible.width * 0.5f, origin.y + visible.height * 0.5f);

    _gameOverPanel = Node::create();
    _gameOverPanel->addChild(LayerColor::create(Color4B(0, 0, 0, 160), visible.width, visible.height));
    _gameOverPanel->setPosition(origin);
    addChild(_gameOverPanel, 100);

    auto title = Label::createWithTTF("GAME OVER", kFont, 48);
    title->setPosition(center - origin + Vec2(0.0f, 60.0f));
    _gameOverPanel->addChild(title);

    auto result = Label::createWithTTF(
        StringUtils::format("Score: %d%s", _score, newBest ? "  (NEW BEST!)" : ""), kFont, 28);
    result->setPosition(center - origin + Vec2(0.0f, 0.0f));
    _gameOverPanel->addChild(result);

    auto hint = Label::createWithTTF("Press R / Space or click to restart", kFont, 20);
    hint->setPosition(center - origin + Vec2(0.0f, -50.0f));
    _gameOverPanel->addChild(hint);
}

bool GameScene::canRestart() const
{
    return _state == State::GameOver && _gameOverElapsed >= kRestartDelay;
}

void GameScene::restart()
{
    // replaceScene은 다음 프레임에 적용되므로, 그 전에 재시작 입력이 연타되어도 한 번만 처리한다.
    if (_restarting)
    {
        return;
    }
    _restarting = true;
    Director::getInstance()->replaceScene(GameScene::createScene());
}
