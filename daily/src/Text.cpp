#include "Text.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <unordered_map>

using namespace geode::prelude;

namespace {
    using gdu::text::Font;

    struct Entry {
        std::string_view key;
        std::string_view en;
        std::string_view ko;
    };

    // 번역표. make_font.py 가 이 파일의 글자에서 한국어 글꼴에 넣을 글자를 고르므로,
    // 한국어 글을 바꾸거나 더하면 그 도구를 다시 돌려야 한다.
    constexpr Entry ENTRIES[] = {
        { "title", "Daily Challenge", "오늘의 도전" },
        { "view.ranking", "Ranking", "순위" },
        { "view.cards", "Cards", "카드" },
        { "level.by", "by {}", "{} 제작" },
        { "level.main", "Main level", "메인 레벨" },
        { "level.info", "{}  -  {}", "{}  -  {}" },
        { "status.offline", "OFFLINE", "연결 안 됨" },
        { "status.loading", "LOADING...", "불러오는 중..." },
        { "status.notReady", "NOT READY", "준비 중" },
        { "detail.notReady", "The server is still picking this level", "서버가 아직 레벨을 고르는 중이에요" },
        { "status.best", "BEST {} PTS", "최고 {}점" },
        { "detail.ranked", "Rank #{} of {}  -  {} attempts", "{1}명 중 {0}등, {2}번 시도" },
        { "detail.cleared", "Cleared in {} attempts", "{}번 시도해서 깼어요" },
        { "status.bestPercent", "BEST {}%", "최고 {}%" },
        { "status.inProgress", "IN PROGRESS", "도전 중" },
        { "detail.triesOne", "1 attempt today", "오늘 1번 시도" },
        { "detail.tries", "{} attempts today", "오늘 {}번 시도" },
        { "detail.noClear", "No clear yet", "아직 못 깼어요" },
        { "status.notPlayed", "NOT PLAYED YET", "아직 안 했어요" },
        { "detail.notPlayed", "Cards at 25, 50 and 75%", "25, 50, 75%에서 카드를 골라요" },
        { "board.noData", "No data yet.\nTap the refresh button.", "아직 정보가 없어요.\n새로 고침을 눌러 주세요." },
        { "board.empty", "No runs yet today.\nBe the first!", "오늘은 아직 기록이 없어요.\n첫 기록을 세워 보세요!" },
        { "board.you", "You", "나" },
        { "cards.empty", "No cards yet.\nTap the refresh button.", "아직 카드가 없어요.\n새로 고침을 눌러 주세요." },
        { "timer", "New challenge in {}", "다음 도전까지 {}" },
        { "error.http", "The server did not answer (HTTP {})", "서버가 답하지 않아요 (HTTP {})" },
        { "error.unreadable", "The server sent something unreadable", "서버의 답을 읽을 수 없어요" },
        { "error.empty", "The server sent an empty challenge", "서버가 빈 도전을 보냈어요" },

        { "picker.choose", "CHOOSE A CARD", "카드를 고르세요" },
        { "picker.hintDesktop", "Click a card, or press 1, 2 or 3", "카드를 누르거나 1, 2, 3 키를 누르세요" },
        { "picker.hintMobile", "Tap a card", "카드를 누르세요" },
        { "picker.rule", "Help cards cost points. Curses earn them. Dying loses every card.",
            "도움 카드는 점수가 깎이고 저주 카드는 점수가 올라요. 죽으면 카드를 모두 잃어요." },
        { "picker.points", "{} POINTS", "{}점" },
        { "picker.go", "GO!", "시작!" },

        { "card.help", "HELP", "도움" },
        { "card.curse", "CURSE", "저주" },

        { "hud.clear", "DAILY RUN CLEAR", "도전 성공!" },
        { "hud.points", "{} POINTS", "{}점" },
        { "shield", "SHIELD!", "방패!" },
        { "info.close", "Tap anywhere to close", "아무 곳이나 누르면 닫혀요" },

        { "toast.dayOver", "The daily challenge ended before your result arrived",
            "기록이 닿기 전에 오늘의 도전이 끝났어요" },
        { "toast.refused", "The server could not accept this result", "서버가 이 기록을 받지 않았어요" },
        { "toast.failed", "Could not send your result. It will retry when you open the Daily Challenge.",
            "기록을 보내지 못했어요. 오늘의 도전을 열면 다시 보내요." },
        { "toast.cleared", "{} cleared with {} points", "{} {}점으로 성공!" },
        { "toast.ranked", "{} cleared with {} points, rank #{} of {}", "{0} {1}점, {3}명 중 {2}등" },

        { "help.title", "How it works", "도움말" },
        { "help.headRun", "A roguelike run, every day", "날마다 새로운 로그라이크" },
        { "help.1", "Each tab has one level for today. Play it in normal mode.",
            "탭마다 오늘의 레벨이 하나씩 있어요.\n일반 모드로 하세요." },
        { "help.2", "At 25%, 50% and 75% the game stops and deals three cards.",
            "25%, 50%, 75%에서 게임이 멈추고\n카드 세 장이 나와요." },
        { "help.3", "Pick one and the run goes on after 3, 2, 1.", "한 장을 고르면 3, 2, 1 뒤에 다시 달려요." },
        { "help.4", "Help cards cost points. Curses earn points.", "도움 카드는 점수가 깎이고\n저주 카드는 점수가 올라요." },
        { "help.5", "Dying loses every card. The next attempt starts with none.",
            "죽으면 카드를 모두 잃고\n빈손으로 다시 시작해요." },
        { "help.6", "A clear scores 100 + your cards. The highest score wins; fewer attempts break ties.",
            "깨면 100점에 카드 점수를 더해요.\n점수가 높을수록, 같으면 시도가 적을수록 위예요." },
        { "help.7", "Everyone on the same tab gets the same cards that day.",
            "같은 탭을 하는 사람은 그날 모두\n같은 카드를 받아요." },
        { "help.8", "Practice mode and start positions do not count.", "연습 모드와 시작 위치로 깬 것은 치지 않아요." },
        { "help.headCards", "The cards", "카드" },
    };

    struct Named {
        std::string_view id;
        std::string_view en;
        std::string_view ko;
    };

    constexpr Named TABS[] = {
        { "main", "Main", "메인" },
        { "easy", "Easy", "이지" },
        { "normal", "Normal", "노멀" },
        { "hard", "Hard", "하드" },
        { "harder", "Harder", "하더" },
        { "insane", "Insane", "인세인" },
        { "easy-demon", "Demon", "데몬" },
    };

    constexpr Named TIERS[] = {
        { "main", "Main levels", "메인 레벨" },
        { "easy", "Easy", "이지" },
        { "normal", "Normal", "노멀" },
        { "hard", "Hard", "하드" },
        { "harder", "Harder", "하더" },
        { "insane", "Insane", "인세인" },
        { "easy-demon", "Easy Demon", "이지 데몬" },
    };

    constexpr Named CARD_NAMES[] = {
        { "shield", "Shield", "방패" },
        { "slow", "Slow", "느림" },
        { "mirror", "Mirror", "거울" },
        { "mini", "Mini", "미니" },
        { "fast", "Rush", "질주" },
        { "dark", "Dark", "어둠" },
        { "shake", "Quake", "지진" },
        { "invisible", "Ghost", "유령" },
        { "mute", "Silence", "침묵" },
        { "flash", "Blackout", "정전" },
    };

    // 카드 설명. 카드 칸이 좁아 한국어는 줄을 직접 나눠 둔다.
    constexpr Named CARD_TEXTS[] = {
        { "shield", "Survive one death, then 1s of safety", "한 번 죽어도 버티고\n1초 동안 무적" },
        { "slow", "The game runs at 0.85x speed", "게임이 0.85배\n속도로 흘러요" },
        { "mirror", "The screen is flipped", "화면이 좌우로\n뒤집혀요" },
        { "mini", "You turn mini until a portal says otherwise", "포털이 바꿀 때까지\n작은 아이콘이 돼요" },
        { "fast", "The game runs at 1.2x speed", "게임이 1.2배\n속도로 흘러요" },
        { "dark", "The screen goes dim", "화면이\n어두워져요" },
        { "shake", "The screen shakes", "화면이\n흔들려요" },
        { "invisible", "Your icon disappears", "내 아이콘이\n사라져요" },
        { "mute", "The music stops", "음악이\n꺼져요" },
        { "flash", "The screen blacks out now and then", "가끔 화면이\n깜깜해져요" },
    };

    constexpr std::array<std::string_view, 5> LENGTH_EN = { "Tiny", "Short", "Medium", "Long", "XL" };
    constexpr std::array<std::string_view, 5> LENGTH_KO = { "아주 짧음", "짧음", "보통", "긺", "아주 긺" };

    std::string pick(Named const* table, std::size_t count, std::string const& id, std::string const& fallback) {
        for (std::size_t i = 0; i < count; ++i) {
            if (table[i].id == id) {
                return std::string(gdu::text::korean() ? table[i].ko : table[i].en);
            }
        }
        return fallback;
    }

    bool wide(std::string_view text) {
        return std::any_of(text.begin(), text.end(), [](char c) {
            return static_cast<unsigned char>(c) >= 0x80;
        });
    }

    char const* gameFont(Font font) {
        switch (font) {
            case Font::Big: return "bigFont.fnt";
            case Font::Gold: return "goldFont.fnt";
            default: return "chatFont.fnt";
        }
    }

    std::string const& ownFont(Font font) {
        static std::string const white = Mod::get()->expandSpriteName("daily-ko.fnt");
        static std::string const gold = Mod::get()->expandSpriteName("daily-ko-gold.fnt");
        return font == Font::Gold ? gold : white;
    }

    // 우리 글꼴이 묶음에 들어 있는지. 없는 .fnt 로 라벨을 만들면 cocos 가 터지므로
    // 한 번 확인하고, 없으면 GD 글꼴로 물러선다(한글은 빈칸이 된다).
    bool ownFontReady() {
        static bool const ready = [] {
            std::error_code error;
            auto const dir = Mod::get()->getResourcesDir();
            bool const found = std::filesystem::exists(dir / "daily-ko.fnt", error)
                && std::filesystem::exists(dir / "daily-ko-gold.fnt", error);
            if (!found) {
                log::warn("daily: the Korean font is missing from {}", dir.string());
            }
            return found;
        }();
        return ready;
    }

    int lineHeight(char const* file) {
        auto* config = FNTConfigLoadFile(file);
        return config ? static_cast<int>(config->m_nCommonHeight) : 0;
    }

    // GD 글꼴의 줄 높이. 화질을 바꾸면 글꼴을 다시 읽어 값이 달라지므로, 화질 배율이
    // 바뀌면 다시 잰다.
    int gameHeight(Font font) {
        static std::array<int, 3> heights{};
        static float scaleFactor = 0.f;
        float const now = CCDirector::sharedDirector()->getContentScaleFactor();
        if (now != scaleFactor) {
            heights = {};
            scaleFactor = now;
        }
        auto& height = heights[static_cast<std::size_t>(font)];
        if (height <= 0) {
            height = lineHeight(gameFont(font));
        }
        return height;
    }

    float ratio(int game, int own) {
        return game > 0 && own > 0 ? static_cast<float>(game) / static_cast<float>(own) : 1.f;
    }

    // GD 글꼴의 줄 높이 / 우리 글꼴의 줄 높이. 같은 배율이면 같은 크기로 보이게 곱한다.
    float ownFactor(Font font) {
        return ratio(gameHeight(font), lineHeight(ownFont(font).c_str()));
    }

    // 라벨이 지금 쓰는 글꼴 기준의 배율. 한국어 패치 모드가 깔려 있으면 한글 라벨의
    // 글꼴을 제 것으로 바꿔 끼우므로, 우리가 고른 글꼴이 아니라 실제 글꼴로 잰다.
    float currentFactor(CCLabelBMFont* label, Font font) {
        auto* config = label ? label->getConfiguration() : nullptr;
        return ratio(gameHeight(font), config ? static_cast<int>(config->m_nCommonHeight) : 0);
    }
}

namespace gdu::text {
    bool korean() {
        return Mod::get()->getSettingValue<std::string>("language") != "english";
    }

    std::string_view tr(std::string_view key) {
        static std::unordered_map<std::string_view, Entry const*> const table = [] {
            std::unordered_map<std::string_view, Entry const*> out;
            for (auto const& entry : ENTRIES) {
                out.emplace(entry.key, &entry);
            }
            return out;
        }();
        auto const found = table.find(key);
        if (found == table.end()) {
            return key;
        }
        return korean() ? found->second->ko : found->second->en;
    }

    std::string cardName(std::string const& id, std::string const& fallback) {
        return pick(CARD_NAMES, std::size(CARD_NAMES), id, fallback);
    }

    std::string cardText(std::string const& id, std::string const& fallback) {
        // 영어 설명은 서버가 준 것을 먼저 쓴다. 서버가 카드 설명을 고치면 바로 보이게.
        if (!korean() && !fallback.empty()) {
            return fallback;
        }
        return pick(CARD_TEXTS, std::size(CARD_TEXTS), id, fallback);
    }

    std::string tierName(std::string const& id, std::string const& fallback) {
        return pick(TIERS, std::size(TIERS), id, fallback);
    }

    std::string tabName(std::string const& id) {
        return pick(TABS, std::size(TABS), id, id);
    }

    std::string lengthName(int length) {
        if (length < 0 || length >= static_cast<int>(LENGTH_EN.size())) {
            return "";
        }
        return std::string(korean() ? LENGTH_KO[length] : LENGTH_EN[length]);
    }

    std::string upper(std::string text) {
        for (auto& c : text) {
            if (c >= 'a' && c <= 'z') {
                c = static_cast<char>(c - 'a' + 'A');
            }
        }
        return text;
    }

    char const* fontFor(std::string_view text, Font font) {
        return wide(text) && ownFontReady() ? ownFont(font).c_str() : gameFont(font);
    }

    float factorFor(std::string_view text, Font font) {
        return wide(text) && ownFontReady() ? ownFactor(font) : 1.f;
    }

    CCLabelBMFont* label(std::string const& text, Font font, float scale) {
        auto* node = CCLabelBMFont::create(text.c_str(), fontFor(text, font));
        node->setScale(scale * currentFactor(node, font));
        return node;
    }

    CCLabelBMFont* lines(std::string const& text, Font font, float scale) {
        // 너비를 주지 않으면(-1) 글꼴이 줄을 스스로 나누지 않는다. 한글은 글자마다 바이트
        // 수가 달라 GD 가 줄을 엉뚱하게 나누는 일이 있어, 줄은 글 안의 \n 으로만 나눈다.
        auto* node = CCLabelBMFont::create(text.c_str(), fontFor(text, font), -1.f, kCCTextAlignmentCenter);
        node->setScale(scale * currentFactor(node, font));
        return node;
    }

    void set(CCLabelBMFont* label, std::string const& text, Font font, float scale) {
        if (!label) {
            return;
        }
        char const* file = fontFor(text, font);
        auto const* current = label->getFntFile();
        if (!current || std::string_view(current) != file) {
            label->setFntFile(file);
        }
        label->setString(text.c_str());
        label->setScale(scale * currentFactor(label, font));
    }

    void fit(CCLabelBMFont* label, Font font, float width, float scale, float minScale) {
        if (!label) {
            return;
        }
        float const factor = currentFactor(label, font);
        label->limitLabelWidth(width, scale * factor, minScale * factor);
    }

    void setFit(CCLabelBMFont* label, std::string const& text, Font font, float width, float scale, float minScale) {
        set(label, text, font, scale);
        fit(label, font, width, scale, minScale);
    }

    CCLabelBMFont* boxed(std::string const& text, Font font, float width, float height, float scale) {
        if (wide(text) && ownFontReady()) {
            auto* node = lines(text, font, scale);
            auto const size = node->getContentSize();
            float best = node->getScale();
            if (size.width > 0.f) {
                best = std::min(best, width / size.width);
            }
            if (size.height > 0.f) {
                best = std::min(best, height / size.height);
            }
            node->setScale(best);
            return node;
        }
        // 영어는 GD 글꼴이 너비에 맞춰 줄을 나눈다. 칸에 들어갈 때까지 글자를 줄인다.
        CCLabelBMFont* node = nullptr;
        for (float s = scale; s > 0.25f; s -= 0.04f) {
            node = CCLabelBMFont::create(text.c_str(), gameFont(font), width / s, kCCTextAlignmentCenter);
            node->setScale(s);
            if (node->getContentSize().height * s <= height) {
                break;
            }
        }
        return node;
    }
}
