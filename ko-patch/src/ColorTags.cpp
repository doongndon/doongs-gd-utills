#include "ColorTags.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

using namespace geode::prelude;

namespace {
    // GD 가 쓰는 색표. <c-RRGGBB> 로 직접 적을 수도 있다.
    struct Named {
        char letter;
        unsigned int rgb;
    };

    constexpr Named COLORS[] = {
        { 'b', 0x4A52E1 }, { 'g', 0x40E348 }, { 'l', 0x60ABEF }, { 'j', 0x32C8FF },
        { 'y', 0xFFFF00 }, { 'o', 0xFF7043 }, { 'r', 0xFF5A5A }, { 'p', 0xFF00FF },
        { 'a', 0x9632FF }, { 'd', 0xFF96FF }, { 'c', 0xFFFF96 }, { 'f', 0x96FFFF },
        { 's', 0xFFDC41 },
    };

    cocos2d::ccColor3B toColor(unsigned int rgb) {
        return cocos2d::ccColor3B{
            static_cast<GLubyte>((rgb >> 16) & 0xFF),
            static_cast<GLubyte>((rgb >> 8) & 0xFF),
            static_cast<GLubyte>(rgb & 0xFF),
        };
    }

    // UTF-8 에서 이어지는 바이트(10xxxxxx)는 글자를 세지 않는다.
    std::size_t charCount(std::string_view text) {
        std::size_t count = 0;
        for (unsigned char byte : text) {
            if ((byte & 0xC0) != 0x80) ++count;
        }
        return count;
    }
}

namespace kopatch::colortags {
    bool hasTags(std::string_view text) {
        return text.find("</c>") != std::string_view::npos;
    }

    Parsed parse(std::string_view text) {
        Parsed out;
        out.plain.reserve(text.size());

        std::vector<cocos2d::ccColor3B> open;
        std::vector<std::size_t> openAt;
        std::size_t chars = 0;

        for (std::size_t i = 0; i < text.size();) {
            if (text[i] == '<') {
                auto const close = text.find('>', i);
                if (close != std::string_view::npos) {
                    auto const tag = text.substr(i + 1, close - i - 1);

                    if (tag == "/c") {
                        if (!open.empty()) {
                            out.spans.push_back(Span{
                                .start = openAt.back(),
                                .length = chars - openAt.back(),
                                .color = open.back(),
                            });
                            open.pop_back();
                            openAt.pop_back();
                        }
                        i = close + 1;
                        continue;
                    }

                    std::optional<cocos2d::ccColor3B> color;
                    if (tag.size() == 2 && tag[0] == 'c') {
                        for (auto const& named : COLORS) {
                            if (named.letter == tag[1]) {
                                color = toColor(named.rgb);
                                break;
                            }
                        }
                    }
                    else if (tag.size() == 8 && tag.starts_with("c-")) {
                        unsigned int rgb = 0;
                        bool ok = true;
                        for (std::size_t d = 2; d < tag.size(); ++d) {
                            auto const c = static_cast<unsigned char>(tag[d]);
                            if (!std::isxdigit(c)) { ok = false; break; }
                            rgb = rgb * 16
                                + static_cast<unsigned int>(
                                      std::isdigit(c) ? c - '0' : (std::tolower(c) - 'a' + 10));
                        }
                        if (ok) color = toColor(rgb);
                    }

                    if (color) {
                        open.push_back(*color);
                        openAt.push_back(chars);
                        i = close + 1;
                        continue;
                    }

                    // <d030> 처럼 색이 아닌 표시는 GD 가 알아서 다룬다. 그대로 넘긴다.
                }
            }

            unsigned char const byte = static_cast<unsigned char>(text[i]);
            std::size_t const width = byte < 0x80 ? 1
                : (byte >> 5) == 0x6 ? 2
                : (byte >> 4) == 0xE ? 3
                : (byte >> 3) == 0x1E ? 4 : 1;
            auto const take = std::min(width, text.size() - i);
            out.plain.append(text.substr(i, take));
            ++chars;
            i += take;
        }

        return out;
    }

    void apply(cocos2d::CCNode* node, std::string const& plain, std::vector<Span> const& spans) {
        if (!node || spans.empty()) return;

        auto const colorAt = [&spans](std::size_t index) -> cocos2d::ccColor3B const* {
            // 나중에 열린 구간이 이긴다. 겹쳐 쓴 경우 안쪽 색을 따른다.
            for (auto it = spans.rbegin(); it != spans.rend(); ++it) {
                if (index >= it->start && index < it->start + it->length) return &it->color;
            }
            return nullptr;
        };

        // 줄 라벨을 차례대로 찾아 평문에서 그 줄이 시작하는 글자 자리를 되짚는다.
        // 자리를 세어 나가지 않고 찾아서 맞추므로, GD 가 공백을 하나 지우거나
        // 줄을 더 나눠도 색이 밀리지 않는다.
        std::size_t cursor = 0;
        std::size_t charsBefore = 0;

        auto* children = node->getChildren();
        if (!children) return;

        for (auto* child : CCArrayExt<CCNode*>(children)) {
            auto* line = typeinfo_cast<CCLabelBMFont*>(child);
            if (!line) continue;

            std::string const text = line->getString();
            if (text.empty()) continue;

            auto const at = plain.find(text, cursor);
            if (at == std::string::npos) continue;

            charsBefore += charCount(std::string_view(plain).substr(cursor, at - cursor));
            cursor = at + text.size();

            auto* glyphs = line->getChildren();
            if (!glyphs) { charsBefore += charCount(text); continue; }

            for (auto* glyph : CCArrayExt<CCNode*>(glyphs)) {
                auto* sprite = typeinfo_cast<CCSprite*>(glyph);
                if (!sprite) continue;
                // cocos 는 글자 스프라이트를 그 줄에서의 글자 번호를 태그로 달아 붙인다.
                auto const index = charsBefore + static_cast<std::size_t>(sprite->getTag());
                if (auto const* color = colorAt(index)) {
                    sprite->setColor(*color);
                }
            }

            charsBefore += charCount(text);
        }
    }
}
