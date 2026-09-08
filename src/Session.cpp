#include "Session.hpp"

using namespace geode::prelude;

namespace {
    constexpr const char* KEY_ID = "resume-level-id";
    constexpr const char* KEY_NAME = "resume-level-name";
    constexpr const char* KEY_PRACTICE = "resume-practice";
    constexpr const char* KEY_ACTIVE = "resume-active";

    // 앱이 OS 에 의해 강제 종료되면 Geode 가 알아서 저장해 줄 틈이 없다.
    // 그래서 값을 바꾼 즉시 디스크에 내려쓴다. 작은 JSON 한 번이라 값싸다.
    void flush() {
        if (Mod::get()->saveData().isErr()) {
            log::warn("could not persist the resume record");
        }
    }
}

namespace gdu::session {
    void begin(GJGameLevel* level) {
        if (!level) {
            return;
        }
        // 온라인에서 받은 레벨만 다룬다. 메인 레벨과 에디터 레벨은 다시 여는
        // 경로가 달라서, 어설프게 지원하느니 버튼을 띄우지 않는 편이 낫다.
        auto const type = level->m_levelType;
        if (type != GJLevelType::Saved && type != GJLevelType::SearchResult) {
            end();
            return;
        }

        int const id = level->m_levelID.value();
        if (id <= 0) {
            end();
            return;
        }

        auto* mod = Mod::get();
        mod->setSavedValue<int64_t>(KEY_ID, id);
        mod->setSavedValue<std::string>(KEY_NAME, std::string(level->m_levelName));
        mod->setSavedValue<bool>(KEY_PRACTICE, false);
        mod->setSavedValue<bool>(KEY_ACTIVE, true);
        flush();
    }

    void setPractice(bool practice) {
        auto* mod = Mod::get();
        if (!mod->getSavedValue<bool>(KEY_ACTIVE, false)) {
            return;
        }
        if (mod->getSavedValue<bool>(KEY_PRACTICE, false) == practice) {
            return;
        }
        mod->setSavedValue<bool>(KEY_PRACTICE, practice);
        flush();
    }

    void end() {
        auto* mod = Mod::get();
        if (!mod->getSavedValue<bool>(KEY_ACTIVE, false)) {
            return;
        }
        mod->setSavedValue<bool>(KEY_ACTIVE, false);
        flush();
    }

    Record current() {
        auto* mod = Mod::get();
        return Record{
            .levelID = static_cast<int>(mod->getSavedValue<int64_t>(KEY_ID, 0)),
            .levelName = mod->getSavedValue<std::string>(KEY_NAME, ""),
            .practice = mod->getSavedValue<bool>(KEY_PRACTICE, false),
            .active = mod->getSavedValue<bool>(KEY_ACTIVE, false),
        };
    }
}
