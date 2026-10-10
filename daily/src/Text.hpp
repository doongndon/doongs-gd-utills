#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <string_view>

#include <fmt/format.h>

// 화면에 쓰는 글. 한국어와 영어를 고를 수 있고(모드 설정 language), 기본은 한국어다.
//
// GD 의 글꼴(bigFont, goldFont, chatFont)에는 한글이 없다. 한글이 섞인 글은 모드에
// 묶은 주아체 비트맵 글꼴(daily-ko.fnt, daily-ko-gold.fnt)로 그린다. 이 글꼴은
// daily/tools/make_font.py 가 Text.cpp 의 한국어 글에 쓰인 글자만 골라 굽는다.
// 숫자와 영어만 있는 글(점수, 이름)은 GD 글꼴 그대로 둔다.
//
// 글꼴마다 한 줄의 높이가 달라 같은 배율로 그리면 크기가 다르다. 그래서 배율은
// 언제나 "GD 글꼴로 그릴 때의 배율"로 받고, 우리 글꼴로 그릴 때는 줄 높이의 비만큼
// 고쳐서 건다.
namespace gdu::text {
    enum class Font {
        Big,   // bigFont.fnt: 흰 글자, 검은 테두리
        Gold,  // goldFont.fnt: 금색 제목 글자
        Chat,  // chatFont.fnt: 작은 설명 글
    };

    bool korean();

    // 번역. 표에 없는 열쇠는 열쇠를 그대로 돌려준다.
    std::string_view tr(std::string_view key);

    template <class... Args>
    std::string format(std::string_view key, Args&&... args) {
        return fmt::format(fmt::runtime(tr(key)), std::forward<Args>(args)...);
    }

    // 카드, 난이도, 길이의 이름. 모르는 것은 서버가 준 영어(fallback)를 쓴다.
    std::string cardName(std::string const& id, std::string const& fallback);
    std::string cardText(std::string const& id, std::string const& fallback);
    std::string tierName(std::string const& id, std::string const& fallback);
    std::string tabName(std::string const& id);
    std::string lengthName(int length);

    // 영어만 대문자로 바꾼다(한글은 그대로).
    std::string upper(std::string text);

    // 글에 맞는 글꼴로 라벨을 만든다. scale 은 GD 글꼴 기준이다.
    cocos2d::CCLabelBMFont* label(std::string const& text, Font font, float scale);
    // 줄바꿈(\n)으로 나뉜 여러 줄 글. 글꼴이 줄을 스스로 나누지 않게 한다.
    cocos2d::CCLabelBMFont* lines(std::string const& text, Font font, float scale);
    // 글을 바꾼다. 한글이 섞이거나 빠지면 글꼴도 바꾼다.
    void set(cocos2d::CCLabelBMFont* label, std::string const& text, Font font, float scale);
    // 너비를 넘으면 줄인다. scale 과 minScale 은 GD 글꼴 기준이다.
    void fit(cocos2d::CCLabelBMFont* label, Font font, float width, float scale, float minScale = 0.1f);
    // 글을 바꾸고 너비에 맞춘다.
    void setFit(cocos2d::CCLabelBMFont* label, std::string const& text, Font font, float width, float scale, float minScale = 0.1f);
    // 정해진 칸(width x height)에 들어가도록 줄인 여러 줄 글.
    cocos2d::CCLabelBMFont* boxed(std::string const& text, Font font, float width, float height, float scale);

    // 그 글을 그릴 글꼴 파일. ButtonSprite 처럼 글꼴 이름을 받는 곳에 쓴다.
    char const* fontFor(std::string_view text, Font font);
    // 그 글꼴로 그릴 때 곱할 배율.
    float factorFor(std::string_view text, Font font);
}
