#include "Art.hpp"

#include <algorithm>
#include <cctype>

#include <fmt/format.h>

using namespace geode::prelude;

namespace {
    // 서버가 주는 카드 id 를 파일 이름에 넣기 전에 거른다. 영문 소문자, 숫자, - 만.
    bool safeID(std::string const& id) {
        return !id.empty() && id.size() <= 32 && std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return std::islower(c) || std::isdigit(c) || c == '-';
        });
    }

    CCSprite* frame(std::string const& name) {
        return CCSprite::createWithSpriteFrameName(name.c_str());
    }
}

namespace gdu::art {
    ccColor3B kindColor(bool help) {
        return help ? ccColor3B{ 40, 205, 150 } : ccColor3B{ 235, 60, 150 };
    }

    ccColor3B kindLight(bool help) {
        return help ? ccColor3B{ 150, 255, 215 } : ccColor3B{ 255, 170, 220 };
    }

    ccColor3B pointsColor(int points) {
        return points < 0 ? ccColor3B{ 255, 125, 125 } : ccColor3B{ 255, 222, 90 };
    }

    std::string pointsText(int points) {
        return fmt::format("{}{}", points > 0 ? "+" : "", points);
    }

    CCSprite* sprite(std::string_view name) {
        auto const path = Mod::get()->expandSpriteName(name);
        return CCSprite::create(path.c_str());
    }

    CCSprite* cardIcon(std::string const& id) {
        if (safeID(id)) {
            if (auto* icon = sprite(fmt::format("icon-{}.png", id))) {
                return icon;
            }
        }
        return frame("GJ_bigStar_001.png");
    }

    CCSprite* difficultyFace(int number, int feature) {
        // 글자 없는 얼굴(diffIcon_XX_btn_001.png). 이름은 따로 GD 글꼴로 쓴다.
        // 그림 이름의 꼴은 GD 가 목록의 난이도에 쓰는 것과 같다(GJLevelList::frameForListDifficulty).
        number = std::clamp(number, 0, 10);
        auto* face = frame(fmt::format("diffIcon_{:02}_btn_001.png", number));
        if (!face) {
            face = frame(fmt::format("difficulty_{:02}_btn_001.png", number));
        }
        if (!face) {
            face = frame("diffIcon_00_btn_001.png");
        }
        if (!face) {
            return nullptr;
        }

        if (feature > 0) {
            // 피처드는 금테, 에픽 이상은 불꽃 테. GD 의 테 그림(GJ_featuredCoin 등)은 얼굴
            // 가운데에서 위로 치우쳐 있어 얼굴 정중앙에 붙이면 위쪽만 삐져나온다. 그래서
            // 얼굴 가운데에 맞춰 그린 우리 테를 쓴다(make_art.py 의 ring_featured, ring_flames).
            bool const flames = feature >= 2;
            if (auto* ring = sprite(flames ? "ring-flames.png" : "ring-featured.png")) {
                auto const content = face->getContentSize();
                fit(ring, std::max(content.width, content.height) * (flames ? 1.6f : 1.32f));
                ring->setPosition(content / 2.f);
                if (feature == 2) {
                    ring->setColor({ 255, 140, 40 });   // 에픽: 주황
                }
                else if (feature == 3) {
                    ring->setColor({ 90, 200, 255 });   // 레전더리: 하늘
                }
                else if (feature >= 4) {
                    ring->setColor({ 220, 90, 255 });   // 미식: 보라
                }
                face->addChild(ring, -1);
            }
        }
        return face;
    }

    void fit(CCNode* node, float size) {
        if (!node) {
            return;
        }
        auto const content = node->getContentSize();
        float const longest = std::max(content.width, content.height);
        node->setScale(longest > 0.f ? size / longest : 1.f);
    }

    void additive(CCSprite* sprite) {
        if (sprite) {
            sprite->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
        }
    }

    void play(std::string_view sound, float volume) {
        if (!Mod::get()->getSettingValue<bool>("sound-effects")) {
            return;
        }
        auto* engine = FMODAudioEngine::sharedEngine();
        // 사용자가 효과음을 꺼 두었으면 우리 소리도 내지 않는다.
        if (!engine || engine->m_sfxVolume <= 0.f) {
            return;
        }
        auto const path = Mod::get()->expandSpriteName(sound);
        engine->playEffect(path.c_str(), 1.f, 0.f, volume);
    }
}
