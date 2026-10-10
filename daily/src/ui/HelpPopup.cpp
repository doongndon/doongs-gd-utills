#include "HelpPopup.hpp"

#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/ScrollLayer.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "../Art.hpp"
#include "../Daily.hpp"
#include "../Text.hpp"

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 380.f;
    constexpr float HEIGHT = 270.f;
    constexpr float LIST_W = 340.f;
    constexpr float LIST_H = 200.f;
    constexpr float PAD = 8.f;

    using gdu::text::Font;

    // 줄 하나. 위에서부터 차례로 쌓으므로 높이만 알면 된다.
    struct Row {
        CCNode* node;
        float height;
    };

    Row heading(std::string const& text) {
        auto* label = gdu::text::label(text, Font::Gold, 0.6f);
        gdu::text::fit(label, Font::Gold, LIST_W - 2.f * PAD, 0.6f, 0.2f);
        return { label, label->getScaledContentSize().height + 6.f };
    }

    Row paragraph(std::string const& text) {
        auto* label = gdu::text::boxed(text, Font::Chat, LIST_W - 2.f * PAD, 60.f, 0.55f);
        return { label, label->getScaledContentSize().height + 4.f };
    }

    // 카드 한 줄: 그림, 이름, 점수, 설명.
    Row cardRow(std::string const& id, gdu::daily::Card const& card) {
        constexpr float H = 30.f;
        auto* row = CCNode::create();
        row->setContentSize({ LIST_W - 2.f * PAD, H });
        row->setAnchorPoint({ 0.5f, 0.5f });

        auto* bg = NineSlice::create("square02b_001.png");
        bg->setContentSize({ LIST_W - 2.f * PAD, H - 2.f });
        bg->setAnchorPoint({ 0.f, 0.f });
        bg->setPosition({ 0.f, 1.f });
        bg->setColor(gdu::art::kindColor(card.help));
        bg->setOpacity(70);
        row->addChild(bg);

        if (auto* icon = gdu::art::cardIcon(id)) {
            gdu::art::fit(icon, H - 6.f);
            icon->setPosition({ 4.f + (H - 6.f) / 2.f, H / 2.f });
            row->addChild(icon);
        }

        auto* name = gdu::text::label(gdu::text::cardName(id, card.name), Font::Big, 0.42f);
        name->setAnchorPoint({ 0.f, 0.5f });
        name->setPosition({ H + 4.f, H / 2.f + 6.f });
        gdu::text::fit(name, Font::Big, 90.f, 0.42f, 0.15f);
        row->addChild(name);

        auto* points = CCLabelBMFont::create(gdu::art::pointsText(card.points).c_str(), "bigFont.fnt");
        points->setScale(0.36f);
        points->setColor(gdu::art::pointsColor(card.points));
        points->setAnchorPoint({ 0.f, 0.5f });
        points->setPosition({ H + 4.f, H / 2.f - 7.f });
        row->addChild(points);

        // 설명은 한 줄로. 한국어 설명은 카드에 맞게 줄을 나눠 두었으므로 이어 붙인다.
        std::string text = gdu::text::cardText(id, card.text);
        std::replace(text.begin(), text.end(), '\n', ' ');
        auto* about = gdu::text::label(text, Font::Chat, 0.5f);
        about->setAnchorPoint({ 0.f, 0.5f });
        about->setPosition({ H + 100.f, H / 2.f });
        gdu::text::fit(about, Font::Chat, LIST_W - 2.f * PAD - H - 104.f, 0.5f, 0.2f);
        row->addChild(about);
        return { row, H + 2.f };
    }
}

namespace gdu::ui {
    HelpPopup* HelpPopup::create() {
        auto* popup = new HelpPopup();
        if (popup->init()) {
            popup->autorelease();
            return popup;
        }
        delete popup;
        return nullptr;
    }

    bool HelpPopup::init() {
        if (!Popup::init(WIDTH, HEIGHT)) {
            return false;
        }
        this->setID("daily-help"_spr);

        auto* title = text::label(std::string(text::tr("help.title")), Font::Gold, 0.75f);
        title->setPosition({ WIDTH / 2.f, HEIGHT - 20.f });
        m_mainLayer->addChild(title);

        auto* frame = NineSlice::create("square02b_001.png");
        frame->setContentSize({ LIST_W, LIST_H });
        frame->setColor({ 0, 0, 0 });
        frame->setOpacity(80);
        frame->setAnchorPoint({ 0.f, 0.f });
        frame->setPosition({ (WIDTH - LIST_W) / 2.f, 22.f });
        m_mainLayer->addChild(frame);

        std::vector<Row> rows;
        rows.push_back(heading(std::string(text::tr("help.headRun"))));
        for (char const* key : { "help.1", "help.2", "help.3", "help.4", "help.5", "help.6", "help.7", "help.8" }) {
            rows.push_back(paragraph(std::string(text::tr(key))));
        }
        if (auto const& today = daily::today(); today && !today->roguelike.cards.empty()) {
            rows.push_back({ CCNode::create(), 6.f });
            rows.push_back(heading(std::string(text::tr("help.headCards"))));
            // 도움 카드를 먼저, 그다음 저주를 점수 순으로.
            std::vector<std::pair<std::string, daily::Card>> cards(
                today->roguelike.cards.begin(), today->roguelike.cards.end()
            );
            std::stable_sort(cards.begin(), cards.end(), [](auto const& a, auto const& b) {
                if (a.second.help != b.second.help) {
                    return a.second.help;
                }
                return a.second.points > b.second.points;
            });
            for (auto const& [id, card] : cards) {
                rows.push_back(cardRow(id, card));
            }
        }

        float total = PAD;
        for (auto const& row : rows) {
            total += row.height;
        }
        total += PAD;

        auto* scroll = ScrollLayer::create(CCSize{ LIST_W, LIST_H });
        scroll->setPosition({ (WIDTH - LIST_W) / 2.f, 22.f });
        m_mainLayer->addChild(scroll);
        auto* content = scroll->m_contentLayer;
        float const height = std::max(total, LIST_H);
        content->setContentSize({ LIST_W, height });

        float y = height - PAD;
        for (auto const& row : rows) {
            row.node->setPosition({ LIST_W / 2.f, y - row.height / 2.f });
            content->addChild(row.node);
            y -= row.height;
        }
        scroll->scrollToTop();
        handleTouchPriority(this);
        return true;
    }
}
