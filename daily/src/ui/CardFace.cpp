#include "CardFace.hpp"

#include <Geode/ui/NineSlice.hpp>

#include "../Art.hpp"
#include "../Daily.hpp"
#include "../Text.hpp"

using namespace geode::prelude;

namespace {
    using gdu::ui::CardFace;
    using gdu::text::Font;

    // make_art.py 의 CARD_* 와 같은 자리(카드 안 좌표, 아래가 0).
    constexpr float RIBBON_Y = 153.f;
    constexpr float MEDAL_Y = 111.f;
    constexpr float MEDAL_ICON = 44.f;
    constexpr float NAME_Y = 70.f;
    constexpr float NAME_W = 96.f;
    constexpr float TEXT_TOP = 59.f;
    constexpr float TEXT_BOTTOM = 25.f;
    constexpr float TEXT_W = 84.f;
    constexpr float POINTS_Y = 13.f;

    // 카드 그림을 카드 크기에 맞춰 놓는다. 화질 설정과 상관없이 같은 크기가 된다.
    CCNode* cardArt(char const* name, ccColor3B fallbackColor) {
        if (auto* art = gdu::art::sprite(name)) {
            art->setScaleX(CardFace::WIDTH / art->getContentSize().width);
            art->setScaleY(CardFace::HEIGHT / art->getContentSize().height);
            art->setPosition({ CardFace::WIDTH / 2.f, CardFace::HEIGHT / 2.f });
            return art;
        }
        // 그림을 못 읽으면 GD 의 판을 카드 색으로 물들여 대신한다.
        auto* plate = CCNode::create();
        plate->setContentSize({ CardFace::WIDTH, CardFace::HEIGHT });
        auto* frame = NineSlice::create("GJ_square02.png");
        frame->setContentSize({ CardFace::WIDTH, CardFace::HEIGHT });
        frame->setAnchorPoint({ 0.f, 0.f });
        plate->addChild(frame);
        auto* tint = NineSlice::create("square02b_001.png");
        tint->setContentSize({ CardFace::WIDTH - 12.f, CardFace::HEIGHT - 12.f });
        tint->setAnchorPoint({ 0.f, 0.f });
        tint->setPosition({ 6.f, 6.f });
        tint->setColor(fallbackColor);
        tint->setOpacity(120);
        plate->addChild(tint);
        return plate;
    }
}

namespace gdu::ui {
    CardFace* CardFace::create(std::string const& id) {
        auto* face = new CardFace();
        if (face->init(id)) {
            face->autorelease();
            return face;
        }
        delete face;
        return nullptr;
    }

    bool CardFace::init(std::string const& id) {
        if (!CCNode::init()) {
            return false;
        }
        m_id = id;
        auto const* card = daily::card(id);
        m_help = card && card->help;

        this->setContentSize({ WIDTH, HEIGHT });
        this->setAnchorPoint({ 0.5f, 0.5f });

        m_front = CCNode::create();
        m_front->setContentSize({ WIDTH, HEIGHT });
        this->addChild(m_front);
        m_front->addChild(cardArt(m_help ? "card-help.png" : "card-curse.png", art::kindColor(m_help)));

        auto* kind = text::label(std::string(text::tr(m_help ? "card.help" : "card.curse")), Font::Big, 0.38f);
        text::fit(kind, Font::Big, 70.f, 0.38f, 0.2f);
        kind->setPosition({ WIDTH / 2.f, RIBBON_Y });
        m_front->addChild(kind);

        if (auto* icon = art::cardIcon(id)) {
            art::fit(icon, MEDAL_ICON);
            icon->setPosition({ WIDTH / 2.f, MEDAL_Y });
            m_front->addChild(icon);
        }

        auto* name = text::label(text::cardName(id, card ? card->name : id), Font::Big, 0.55f);
        text::fit(name, Font::Big, NAME_W, 0.55f, 0.2f);
        name->setPosition({ WIDTH / 2.f, NAME_Y });
        m_front->addChild(name);

        auto const about = text::cardText(id, card ? card->text : "");
        if (!about.empty()) {
            auto* label = text::boxed(about, Font::Chat, TEXT_W, TEXT_TOP - TEXT_BOTTOM - 4.f, 0.55f);
            label->setPosition({ WIDTH / 2.f, (TEXT_TOP + TEXT_BOTTOM) / 2.f });
            m_front->addChild(label);
        }

        int const points = card ? card->points : 0;
        auto* score = CCLabelBMFont::create(art::pointsText(points).c_str(), "bigFont.fnt");
        score->setScale(0.42f);
        score->setColor(art::pointsColor(points));
        score->setPosition({ WIDTH / 2.f, POINTS_Y });
        m_front->addChild(score);

        m_back = CCNode::create();
        m_back->setContentSize({ WIDTH, HEIGHT });
        m_back->addChild(cardArt("card-back.png", { 60, 70, 170 }));
        m_back->setVisible(false);
        this->addChild(m_back);
        return true;
    }

    void CardFace::showFront(bool front) {
        m_front->setVisible(front);
        m_back->setVisible(!front);
    }

    bool CardFace::isHelp() const {
        return m_help;
    }

    std::string const& CardFace::cardID() const {
        return m_id;
    }

    CCNode* cardChip(std::string const& id, float width, float height) {
        auto const* card = daily::card(id);
        bool const help = card && card->help;

        auto* chip = CCNode::create();
        chip->setContentSize({ width, height });
        chip->setAnchorPoint({ 0.5f, 0.5f });

        auto* bg = NineSlice::create("square02b_001.png");
        bg->setContentSize({ width, height });
        bg->setAnchorPoint({ 0.f, 0.f });
        bg->setColor(art::kindColor(help));
        bg->setOpacity(140);
        chip->addChild(bg);

        auto const light = art::kindLight(help);
        auto* edge = CCLayerColor::create({ light.r, light.g, light.b, 220 }, 3.f, height - 10.f);
        edge->setPosition({ 3.f, 5.f });
        chip->addChild(edge);

        float const iconSize = height - 8.f;
        if (auto* icon = art::cardIcon(id)) {
            art::fit(icon, iconSize);
            icon->setPosition({ 9.f + iconSize / 2.f, height / 2.f });
            chip->addChild(icon);
        }

        float const textX = 12.f + iconSize;
        auto* name = text::label(text::cardName(id, card ? card->name : id), Font::Big, 0.34f);
        name->setAnchorPoint({ 0.f, 0.5f });
        text::fit(name, Font::Big, width - textX - 4.f, 0.34f, 0.15f);
        name->setPosition({ textX, height * 0.66f });
        chip->addChild(name);

        int const points = card ? card->points : 0;
        auto* score = CCLabelBMFont::create(art::pointsText(points).c_str(), "bigFont.fnt");
        score->setAnchorPoint({ 0.f, 0.5f });
        score->setScale(0.3f);
        score->setColor(art::pointsColor(points));
        score->setPosition({ textX, height * 0.3f });
        chip->addChild(score);
        return chip;
    }
}
