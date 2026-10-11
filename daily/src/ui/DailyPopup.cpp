#include "DailyPopup.hpp"

#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/General.hpp>
#include <Geode/ui/LoadingSpinner.hpp>
#include <Geode/ui/NineSlice.hpp>

#include <algorithm>
#include <array>
#include <chrono>

#include <fmt/format.h>

#include "../Art.hpp"
#include "../Daily.hpp"
#include "../Text.hpp"
#include "CardFace.hpp"
#include "CardInfo.hpp"
#include "HelpPopup.hpp"

using namespace geode::prelude;

namespace {
    using gdu::text::Font;

    constexpr float WIDTH = 420.f;
    constexpr float HEIGHT = 290.f;

    // 네 귀의 금장식(약 50 x 50)을 피해 안쪽에 놓는다.
    constexpr float TABS_LEFT = 52.f;
    constexpr float TABS_RIGHT = 368.f;
    constexpr float TABS_Y = 229.f;
    constexpr float TAB_W = 44.f;
    constexpr float TAB_H = 40.f;

    constexpr float PANEL_Y = 52.f;
    constexpr float PANEL_H = 150.f;
    constexpr float LEVEL_X = 16.f;
    constexpr float LEVEL_W = 160.f;
    constexpr float BOARD_X = 184.f;
    constexpr float BOARD_W = 220.f;

    constexpr std::size_t ROWS = 10;
    constexpr float ROW_TOP = 118.f;
    constexpr float ROW_STEP = 12.f;
    constexpr float HEADER_Y = 136.f;

    constexpr ccColor3B GOLD{ 255, 210, 90 };
    constexpr ccColor3B MEDALS[] = { { 255, 210, 60 }, { 215, 225, 240 }, { 235, 155, 90 } };

    // 서버의 난이도 목록(daily-server/server.js 의 TIERS)과 같은 차례다. 서버의 답이
    // 오기 전에도 탭을 그릴 수 있게 여기에도 적어 둔다. 이름은 Text.cpp 에 있다.
    constexpr std::array<char const*, 7> TABS = {
        "main", "easy", "normal", "hard", "harder", "insane", "easy-demon",
    };

    std::string tr(char const* key) {
        return std::string(gdu::text::tr(key));
    }

    // 어두운 반투명 판. Geode 의 모드 목록이 쓰는 것과 같은 그림이다.
    CCNode* panel(float width, float height) {
        auto* bg = NineSlice::create("square02b_001.png");
        bg->setContentSize({ width, height });
        bg->setColor({ 0, 0, 0 });
        bg->setOpacity(85);
        bg->setAnchorPoint({ 0.f, 0.f });
        return bg;
    }

    CCLabelBMFont* label(CCNode* parent, Font font, float scale, CCPoint position, CCPoint anchor = { 0.5f, 0.5f }) {
        auto* text = gdu::text::label("", font, scale);
        text->setAnchorPoint(anchor);
        text->setPosition(position);
        parent->addChild(text);
        return text;
    }

    std::string clock(int seconds) {
        seconds = std::max(0, seconds);
        return fmt::format("{:02}:{:02}:{:02}", seconds / 3600, seconds % 3600 / 60, seconds % 60);
    }
}

namespace gdu::ui {
    DailyPopup* DailyPopup::create() {
        auto* popup = new DailyPopup();
        if (popup->init()) {
            popup->autorelease();
            return popup;
        }
        delete popup;
        return nullptr;
    }

    bool DailyPopup::init() {
        if (!Popup::init(WIDTH, HEIGHT)) {
            return false;
        }
        this->setID("daily-popup"_spr);
        // GD 의 데일리 레벨 창처럼 네 귀에 금장식을 단다.
        addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupGold, false);

        // Popup::setTitle 은 goldFont 로만 그려 한글이 빈칸이 되므로 직접 단다.
        auto* title = text::label(tr("title"), Font::Gold, 0.75f);
        text::fit(title, Font::Gold, WIDTH - 120.f, 0.75f, 0.2f);
        title->setPosition({ WIDTH / 2.f, HEIGHT - 20.f });
        m_mainLayer->addChild(title, 2);

        m_tier = daily::selectedTier();
        if (std::none_of(TABS.begin(), TABS.end(), [&](char const* id) { return m_tier == id; })) {
            m_tier = TABS[0];
        }

        if (auto* info = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png")) {
            info->setScale(0.8f);
            auto* button = CCMenuItemSpriteExtra::create(info, this, menu_selector(DailyPopup::onInfo));
            button->setPosition({ WIDTH - 22.f, HEIGHT - 22.f });
            m_buttonMenu->addChild(button);
        }

        this->buildTabs();
        this->buildLevelPanel();
        this->buildBoardPanel();

        if (auto* icon = CCSprite::createWithSpriteFrameName("GJ_timeIcon_001.png")) {
            art::fit(icon, 12.f);
            icon->setPosition({ WIDTH / 2.f - 78.f, 31.f });
            m_mainLayer->addChild(icon);
        }
        m_timer = label(m_mainLayer, Font::Chat, 0.55f, { WIDTH / 2.f - 70.f, 31.f }, { 0.f, 0.5f });

        // 받아 둔 것이 있으면 먼저 보여 주고, 뒤에서 새로 받아 바꾼다.
        this->render(std::nullopt);
        this->refresh();
        this->schedule(schedule_selector(DailyPopup::tick), 1.f);
        return true;
    }

    // 난이도 탭. 처음 한 번만 짓고, 고를 때는 안의 그림만 바꾼다. 누른 단추를 그 단추의
    // 콜백 안에서 지웠다 다시 지으면 아직 돌고 있는 단추를 지우는 셈이 된다.
    void DailyPopup::buildTabs() {
        float const slot = (TABS_RIGHT - TABS_LEFT) / static_cast<float>(TABS.size());
        for (std::size_t i = 0; i < TABS.size(); ++i) {
            Tab tab;
            tab.id = TABS[i];
            tab.holder = CCNode::create();
            tab.holder->setContentSize({ TAB_W, TAB_H });

            tab.glow = art::sprite("glow.png");
            if (tab.glow) {
                art::fit(tab.glow, 50.f);
                tab.glow->setColor(GOLD);
                art::additive(tab.glow);
                tab.glow->setPosition({ TAB_W / 2.f, 24.f });
                tab.glow->setVisible(false);
                tab.glow->runAction(CCRepeatForever::create(CCSequence::create(
                    CCFadeTo::create(0.9f, 120),
                    CCFadeTo::create(0.9f, 220),
                    nullptr
                )));
                tab.holder->addChild(tab.glow, 0);
            }

            tab.label = text::label(text::tabName(tab.id), Font::Big, 0.28f);
            text::fit(tab.label, Font::Big, TAB_W - 2.f, 0.28f, 0.1f);
            tab.label->setPosition({ TAB_W / 2.f, 6.f });
            tab.holder->addChild(tab.label, 2);

            if ((tab.check = CCSprite::createWithSpriteFrameName("GJ_completesIcon_001.png"))) {
                art::fit(tab.check, 11.f);
                tab.check->setPosition({ TAB_W - 6.f, TAB_H - 6.f });
                tab.check->setVisible(false);
                tab.holder->addChild(tab.check, 3);
            }

            tab.button = CCMenuItemSpriteExtra::create(tab.holder, this, menu_selector(DailyPopup::onTab));
            tab.button->setTag(static_cast<int>(i));
            tab.button->m_scaleMultiplier = 1.1f;
            tab.button->setPosition({ TABS_LEFT + slot * (static_cast<float>(i) + 0.5f), TABS_Y });
            m_buttonMenu->addChild(tab.button);
            m_tabs.push_back(tab);
        }
        this->paintTabs();
    }

    void DailyPopup::paintTabs() {
        for (auto& tab : m_tabs) {
            bool const selected = tab.id == m_tier;
            if (tab.face) {
                tab.face->removeFromParent();
                tab.face = nullptr;
            }
            if (auto* face = art::difficultyFace(daily::tierFace(tab.id))) {
                art::fit(face, selected ? 30.f : 25.f);
                face->setPosition({ TAB_W / 2.f, 24.f });
                face->setColor(selected ? ccColor3B{ 255, 255, 255 } : ccColor3B{ 175, 175, 175 });
                tab.holder->addChild(face, 1);
                tab.face = face;
            }
            if (tab.glow) {
                tab.glow->setVisible(selected);
            }
            tab.label->setColor(selected ? GOLD : ccColor3B{ 255, 255, 255 });
            tab.label->setOpacity(selected ? 255 : 170);
            if (tab.check) {
                // 받아 둔 오늘의 도전이 없으면 저장된 기록이 어제 것일 수 있다.
                tab.check->setVisible(daily::today() && daily::progress(tab.id).clearedWith > 0);
            }
        }
    }

    void DailyPopup::buildLevelPanel() {
        m_levelPanel = CCNode::create();
        m_levelPanel->setContentSize({ LEVEL_W, PANEL_H });
        m_levelPanel->setPosition({ LEVEL_X, PANEL_Y });
        m_mainLayer->addChild(m_levelPanel);
        m_levelPanel->addChild(panel(LEVEL_W, PANEL_H));

        m_levelArt = CCNode::create();
        m_levelPanel->addChild(m_levelArt);

        float const middle = LEVEL_W / 2.f;
        m_levelName = label(m_levelPanel, Font::Big, 0.5f, { middle, 66.f });
        m_creator = label(m_levelPanel, Font::Gold, 0.45f, { middle, 52.f });
        m_info = label(m_levelPanel, Font::Chat, 0.42f, { middle, 41.f });
        m_info->setOpacity(175);

        auto* divider = CCLayerColor::create({ 255, 255, 255, 35 }, LEVEL_W - 24.f, 1.f);
        divider->setPosition({ 12.f, 32.f });
        m_levelPanel->addChild(divider);

        m_status = label(m_levelPanel, Font::Gold, 0.5f, { middle, 21.f });
        m_detail = label(m_levelPanel, Font::Chat, 0.42f, { middle, 8.f });
        m_detail->setOpacity(190);

        // GD 의 레벨 화면에 있는 그 큰 초록 재생 단추.
        CCNode* play = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
        if (!play) {
            play = ButtonSprite::create("Play", "goldFont.fnt", "GJ_button_01.png", 0.9f);
        }
        m_playButton = CCMenuItemSpriteExtra::create(play, this, menu_selector(DailyPopup::onPlay));
        auto const size = m_playButton->getContentSize();
        float const scale = 46.f / std::max(1.f, std::max(size.width, size.height));
        m_playButton->setScale(scale);
        m_playButton->m_baseScale = scale;
        m_playButton->setPosition({ LEVEL_X + 116.f, PANEL_Y + 108.f });
        m_buttonMenu->addChild(m_playButton);
    }

    void DailyPopup::buildBoardPanel() {
        m_boardPanel = CCNode::create();
        m_boardPanel->setContentSize({ BOARD_W, PANEL_H });
        m_boardPanel->setPosition({ BOARD_X, PANEL_Y });
        m_mainLayer->addChild(m_boardPanel);
        m_boardPanel->addChild(panel(BOARD_W, PANEL_H));

        // 순위표와 오늘의 카드를 오가는 단추 두 개.
        float x = BOARD_X + 6.f;
        for (auto [key, tag] : { std::pair{ "view.ranking", 0 }, std::pair{ "view.cards", 1 } }) {
            auto const caption = tr(key);
            auto* sprite = ButtonSprite::create(
                caption.c_str(), text::fontFor(caption, Font::Big), "GJ_button_04.png",
                0.6f * text::factorFor(caption, Font::Big)
            );
            auto* button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(DailyPopup::onView));
            button->setTag(tag);
            float const scale = 15.f / std::max(1.f, button->getContentSize().height);
            button->setScale(scale);
            button->m_baseScale = scale;
            float const width = button->getScaledContentSize().width;
            button->setPosition({ x + width / 2.f, PANEL_Y + HEADER_Y });
            x += width + 4.f;
            m_buttonMenu->addChild(button);
            (tag == 0 ? m_rankingTab : m_cardsTab) = sprite;
        }

        CCNode* refresh = nullptr;
        if (auto* top = art::sprite("refresh-icon.png")) {
            refresh = CircleButtonSprite::create(top, CircleBaseColor::Green, CircleBaseSize::Small);
        }
        if (!refresh) {
            refresh = ButtonSprite::create("Refresh", "goldFont.fnt", "GJ_button_04.png", 0.8f);
        }
        auto* refreshButton = CCMenuItemSpriteExtra::create(refresh, this, menu_selector(DailyPopup::onRefresh));
        float const refreshScale = 18.f / std::max(1.f, refreshButton->getContentSize().height);
        refreshButton->setScale(refreshScale);
        refreshButton->m_baseScale = refreshScale;
        refreshButton->setPosition({ BOARD_X + BOARD_W - 14.f, PANEL_Y + HEADER_Y });
        m_buttonMenu->addChild(refreshButton);

        m_rows = CCNode::create();
        m_rows->setContentSize({ BOARD_W, PANEL_H });
        m_boardPanel->addChild(m_rows);

        m_empty = text::lines("", Font::Chat, 0.6f);
        m_empty->setOpacity(175);
        m_empty->setPosition({ BOARD_W / 2.f, 64.f });
        m_boardPanel->addChild(m_empty);

        auto* spinner = LoadingSpinner::create(26.f);
        spinner->setPosition({ BOARD_W / 2.f, 64.f });
        spinner->setVisible(false);
        m_boardPanel->addChild(spinner);
        m_spinner = spinner;
    }

    void DailyPopup::setLoading(bool loading) {
        m_loading = loading;
        m_spinner->setVisible(loading);
        if (loading) {
            text::set(m_empty, "", Font::Chat, 0.6f);
        }
    }

    void DailyPopup::refresh() {
        this->setLoading(true);
        auto url = daily::todayUrl(m_tier);
        m_task.spawn(
            "daily popup",
            [url = std::move(url), agent = std::string(Mod::get()->getID())] {
                return web::WebRequest()
                    .userAgent(agent)
                    .timeout(std::chrono::seconds(30))
                    .get(url);
            },
            [this](web::WebResponse response) {
                auto const error = daily::acceptToday(response);
                if (!error) {
                    daily::resubmitPending();
                }
                this->setLoading(false);
                this->render(error);
            }
        );
    }

    void DailyPopup::render(std::optional<std::string> const& error) {
        m_error = error;
        if (daily::today()) {
            m_hadToday = true;
        }
        // 서버가 이 난이도의 레벨을 못 골랐으면 이 기기에서 GD 서버를 검색해 대신 고른다.
        if (auto const* tier = daily::tier(m_tier); tier && !tier->level && tier->pick && !error) {
            daily::pickLevel(m_tier);
        }
        this->paintTabs();
        this->renderLevel(error);
        if (m_view == View::Ranking) {
            this->renderRanking();
        }
        else {
            this->renderCards();
        }
        m_rankingTab->updateBGImage(m_view == View::Ranking ? "GJ_button_01.png" : "GJ_button_04.png");
        m_cardsTab->updateBGImage(m_view == View::Cards ? "GJ_button_01.png" : "GJ_button_04.png");
        this->renderTimer();
        m_seenRevision = daily::revision();
    }

    void DailyPopup::renderLevel(std::optional<std::string> const& error) {
        m_levelArt->removeAllChildren();
        auto const* tier = daily::tier(m_tier);
        bool const hasLevel = tier && tier->level;
        m_playButton->setVisible(hasLevel);
        m_playButton->setEnabled(hasLevel);

        auto const look = tier ? daily::lookOf(*tier) : daily::Look{ .face = daily::tierFace(m_tier) };
        if (auto* face = art::difficultyFace(look.face, look.feature)) {
            art::fit(face, 40.f);
            face->setPosition({ 42.f, 116.f });
            m_levelArt->addChild(face);
        }
        if (look.stars > 0) {
            auto* stars = CCLabelBMFont::create(std::to_string(look.stars).c_str(), "bigFont.fnt");
            stars->setScale(0.36f);
            stars->setAnchorPoint({ 1.f, 0.5f });
            stars->setPosition({ 41.f, 79.f });
            m_levelArt->addChild(stars);
            if (auto* star = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png")) {
                art::fit(star, 11.f);
                star->setPosition({ 49.f, 79.f });
                m_levelArt->addChild(star);
            }
        }

        float const width = LEVEL_W - 14.f;
        auto status = [&](std::string const& text, ccColor3B color) {
            m_status->setColor(color);
            text::setFit(m_status, text, Font::Gold, width, 0.5f, 0.15f);
        };
        auto detail = [&](std::string const& text) {
            text::setFit(m_detail, text, Font::Chat, width, 0.42f, 0.15f);
        };
        constexpr ccColor3B PLAIN{ 255, 255, 255 };
        constexpr ccColor3B BAD{ 255, 120, 120 };

        if (!hasLevel) {
            text::setFit(m_levelName, "", Font::Big, width, 0.5f, 0.15f);
            text::setFit(m_creator, "", Font::Gold, width, 0.45f, 0.15f);
            text::setFit(m_info, "", Font::Chat, width, 0.42f, 0.15f);
            if (error) {
                status(tr("status.offline"), BAD);
                detail(*error);
            }
            else if (daily::pickingLevel(m_tier)) {
                status(tr("status.picking"), PLAIN);
                detail(tr("detail.picking"));
            }
            else if (auto const failed = daily::pickError(m_tier)) {
                status(tr("status.pickFailed"), BAD);
                detail(*failed);
            }
            else {
                bool const waiting = m_loading || !daily::today();
                status(tr(waiting ? "status.loading" : "status.notReady"), PLAIN);
                detail(waiting ? "" : tr("detail.notReady"));
            }
            return;
        }

        auto const& level = *tier->level;
        text::setFit(m_levelName, level.name, Font::Big, width, 0.5f, 0.15f);
        text::setFit(
            m_creator, look.creator.empty() ? "" : text::format("level.by", look.creator), Font::Gold, width, 0.45f, 0.15f
        );
        std::string info = level.mainLevel ? tr("level.main") : text::tierName(tier->id, tier->name);
        if (auto const length = text::lengthName(look.length); !length.empty()) {
            info = text::format("level.info", length, info);
        }
        text::setFit(m_info, info, Font::Chat, width, 0.42f, 0.15f);

        auto const& today = daily::today();
        auto const progress = daily::progress(m_tier);
        auto const attempts = daily::attemptsToday(m_tier);

        if (error) {
            status(tr("status.offline"), BAD);
            detail(*error);
        }
        else if (progress.clearedWith > 0) {
            status(text::format("status.best", progress.score), PLAIN);
            bool const ranked = today && today->boardTier == m_tier && today->myRank;
            detail(ranked
                ? text::format("detail.ranked", *today->myRank, today->total, progress.clearedWith)
                : text::format("detail.cleared", progress.clearedWith));
        }
        else if (progress.baseline >= 0) {
            status(progress.bestPercent > 0
                ? text::format("status.bestPercent", progress.bestPercent)
                : tr("status.inProgress"), PLAIN);
            if (!attempts) {
                detail(tr("detail.noClear"));
            }
            else if (*attempts == 1) {
                detail(tr("detail.triesOne"));
            }
            else {
                detail(text::format("detail.tries", *attempts));
            }
        }
        else {
            status(tr("status.notPlayed"), PLAIN);
            detail(tr("detail.notPlayed"));
        }
    }

    void DailyPopup::clearChips() {
        for (auto* chip : m_chips) {
            chip->removeFromParent();
        }
        m_chips.clear();
    }

    // 순위표. 순위, 이름, 고른 카드, 점수를 한 줄씩. 위 세 자리는 금, 은, 동. 내 줄은 밝게.
    void DailyPopup::renderRanking() {
        m_rows->removeAllChildren();
        this->clearChips();

        auto const& today = daily::today();
        if (m_loading) {
            return;
        }
        if (!today || today->boardTier != m_tier) {
            text::set(m_empty, today ? "" : tr("board.noData"), Font::Chat, 0.6f);
            return;
        }
        if (today->top.empty()) {
            text::set(m_empty, tr("board.empty"), Font::Chat, 0.6f);
            return;
        }
        text::set(m_empty, "", Font::Chat, 0.6f);

        // 내가 10위 밖이면 마지막 줄을 내 줄로 바꾼다.
        std::vector<std::pair<int, daily::Row>> rows;
        for (std::size_t i = 0; i < today->top.size() && i < ROWS; ++i) {
            rows.emplace_back(static_cast<int>(i) + 1, today->top[i]);
        }
        int const mine = today->myRank.value_or(0);
        if (mine > static_cast<int>(rows.size()) && !rows.empty()) {
            daily::Row me;
            me.name = tr("board.you");
            me.score = today->myScore;
            me.cards = daily::progress(m_tier).cards;
            if (rows.size() >= ROWS) {
                rows.back() = { mine, me };
            }
            else {
                rows.emplace_back(mine, me);
            }
        }

        for (std::size_t i = 0; i < rows.size(); ++i) {
            auto const& [rank, row] = rows[i];
            float const y = ROW_TOP - static_cast<float>(i) * ROW_STEP;
            bool const isMe = rank == mine;

            if (isMe) {
                auto* bar = CCLayerColor::create({ 255, 215, 90, 45 }, BOARD_W - 8.f, ROW_STEP - 1.f);
                bar->setPosition({ 4.f, y - (ROW_STEP - 1.f) / 2.f });
                m_rows->addChild(bar);
            }

            auto* number = CCLabelBMFont::create(std::to_string(rank).c_str(), "goldFont.fnt");
            number->setPosition({ 15.f, y });
            number->limitLabelWidth(20.f, 0.42f, 0.2f);
            if (rank <= 3) {
                number->setColor(MEDALS[rank - 1]);
            }
            m_rows->addChild(number);

            auto* name = text::label(row.name, Font::Chat, 0.52f);
            name->setAnchorPoint({ 0.f, 0.5f });
            name->setPosition({ 28.f, y });
            text::fit(name, Font::Chat, 92.f, 0.52f, 0.25f);
            if (isMe) {
                name->setColor(GOLD);
            }
            m_rows->addChild(name);

            for (std::size_t c = 0; c < row.cards.size() && c < 3; ++c) {
                if (auto* icon = art::cardIcon(row.cards[c])) {
                    art::fit(icon, 10.f);
                    icon->setPosition({ 134.f + 12.f * static_cast<float>(c), y });
                    m_rows->addChild(icon);
                }
            }

            auto* value = CCLabelBMFont::create(std::to_string(row.score.value_or(0)).c_str(), "bigFont.fnt");
            value->setScale(0.36f);
            value->setAnchorPoint({ 1.f, 0.5f });
            value->setPosition({ BOARD_W - 8.f, y });
            value->setColor(rank <= 3 ? MEDALS[rank - 1] : ccColor3B{ 255, 255, 255 });
            m_rows->addChild(value);
        }
    }

    // 오늘의 카드. 25, 50, 75% 에서 나오는 카드 세 장씩. 누르면 크게 보인다.
    void DailyPopup::renderCards() {
        m_rows->removeAllChildren();
        this->clearChips();
        text::set(m_empty, "", Font::Chat, 0.6f);

        auto const* tier = daily::tier(m_tier);
        auto const& today = daily::today();
        if (!tier || !today || tier->offers.empty()) {
            if (!m_loading) {
                text::set(m_empty, tr("cards.empty"), Font::Chat, 0.6f);
            }
            return;
        }

        constexpr float CHIP_W = 56.f;
        constexpr float CHIP_H = 32.f;
        auto const& milestones = today->roguelike.milestones;
        for (std::size_t stop = 0; stop < tier->offers.size() && stop < 3; ++stop) {
            float const y = 106.f - 40.f * static_cast<float>(stop);
            int const percent = stop < milestones.size() ? milestones[stop] : 0;
            auto* mark = CCLabelBMFont::create(fmt::format("{}%", percent).c_str(), "goldFont.fnt");
            mark->setScale(0.48f);
            mark->setPosition({ 20.f, y });
            m_rows->addChild(mark);

            auto const& offer = tier->offers[stop];
            for (std::size_t i = 0; i < offer.size() && i < 3; ++i) {
                auto* chip = cardChip(offer[i], CHIP_W, CHIP_H);
                auto* button = CCMenuItemExt::createSpriteExtra(chip, [id = offer[i]](CCMenuItemSpriteExtra*) {
                    if (auto* popup = CardInfo::create(id)) {
                        popup->show();
                    }
                });
                button->m_scaleMultiplier = 1.08f;
                button->setPosition({ BOARD_X + 68.f + 58.f * static_cast<float>(i), PANEL_Y + y });
                m_buttonMenu->addChild(button);
                m_chips.push_back(button);
            }
        }
    }

    void DailyPopup::renderTimer() {
        auto const left = daily::secondsLeft();
        text::set(m_timer, left ? text::format("timer", clock(*left)) : "", Font::Chat, 0.55f);
    }

    void DailyPopup::tick(float) {
        this->renderTimer();
        // 뒤에서 오늘의 도전이 바뀌었으면(대신 고른 레벨이 정해지는 등) 다시 그린다.
        if (!m_loading && daily::revision() != m_seenRevision) {
            this->render(m_error);
        }
        // 자정이 지나 오늘의 도전이 끝났으면 새것을 한 번 받는다. 서버가 안 받을 때
        // 매초 두드리지 않도록 받아 둔 것이 있다가 사라졌을 때만.
        if (m_hadToday && !daily::today() && !m_loading) {
            m_hadToday = false;
            this->refresh();
        }
    }

    void DailyPopup::onTab(CCObject* sender) {
        auto const index = static_cast<std::size_t>(static_cast<CCNode*>(sender)->getTag());
        if (index >= TABS.size() || m_tier == TABS[index]) {
            return;
        }
        m_tier = TABS[index];
        daily::selectTier(m_tier);
        this->render(std::nullopt);
        this->refresh();
    }

    void DailyPopup::onView(CCObject* sender) {
        auto const view = static_cast<CCNode*>(sender)->getTag() == 1 ? View::Cards : View::Ranking;
        if (view == m_view) {
            return;
        }
        m_view = view;
        this->render(std::nullopt);
    }

    void DailyPopup::onPlay(CCObject*) {
        daily::open(m_tier);
    }

    void DailyPopup::onRefresh(CCObject*) {
        if (!m_loading) {
            // 대신 고르기에 실패했던 것도 새로 고침에서 다시 해 본다.
            daily::forgetPickError(m_tier);
            this->refresh();
        }
    }

    void DailyPopup::onInfo(CCObject*) {
        if (auto* popup = HelpPopup::create()) {
            popup->show();
        }
    }
}
