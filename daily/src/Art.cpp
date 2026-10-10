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
            // 피처드는 금테, 에픽 이상은 불꽃 테. 불꽃 그림을 못 찾으면 금테로 둔다.
            CCSprite* ring = nullptr;
            if (feature == 2) {
                ring = frame("GJ_epicCoin_001.png");
            }
            else if (feature == 3) {
                ring = frame("GJ_epicCoin2_001.png");
            }
            else if (feature >= 4) {
                ring = frame("GJ_epicCoin3_001.png");
            }
            if (!ring) {
                ring = frame("GJ_featuredCoin_001.png");
            }
            if (ring) {
                ring->setPosition(face->getContentSize() / 2.f);
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
