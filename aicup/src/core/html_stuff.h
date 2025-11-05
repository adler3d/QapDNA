#include <unordered_set>
#include <regex>
#include <cctype>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <iomanip>

// -------------------- Конфигурация --------------------

static const std::unordered_set<std::string> allowedTags = {
    "img", "b", "small", "font", "span", "code", "s", "i", "a", "table", "th", "tr", "td", "br"
};

static const std::unordered_map<std::string, std::unordered_set<std::string>> allowedAttributes = {
    {"img", {"src", "alt", "title", "width", "height"}},
    {"font", {"color"}},
    {"a", {"href", "title", "target"}}
};

static const std::unordered_set<std::string> allowedSchemes = {
    "",     // relative urls
    "http",
    "https",
    "mailto",
    "tel"
};

// -------------------- Утилиты --------------------

static inline std::string toLowerCopy(const std::string &s) {
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c){ return std::tolower(c); });
    return t;
}

static inline std::string escapeHtml(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (unsigned char c : text) {
        switch (c) {
            case '&':  result += "&amp;";  break;
            case '<':  result += "&lt;";   break;
            case '>':  result += "&gt;";   break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&#39;";  break;
            default:   result += c;        break;
        }
    }
    return result;
}

static inline std::string escapeAttribute(const std::string& val) {
    // Для атрибутов дополнительно экранируем "
    return escapeHtml(val);
}

static inline std::string trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b-1])) --b;
    return s.substr(a, b-a);
}

// Декодирование сущностей: числовые и несколько базовых именованных
std::string decodeHtmlEntities(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '&') {
            // найдем ';'
            size_t semi = s.find(';', i+1);
            if (semi == std::string::npos) { out += s[i++]; continue; }
            std::string ent = s.substr(i+1, semi - (i+1)); // без & и ;
            if (!ent.empty() && ent[0] == '#') {
                // числовая
                try {
                    if (ent.size() >= 2 && (ent[1] == 'x' || ent[1] == 'X')) {
                        unsigned int code = std::stoul(ent.substr(2), nullptr, 16);
                        if (code > 0 && code < 0x10FFFF) {
                            // конвертируем в UTF-8 (упрощённо — для ASCII <128)
                            if (code < 128) out.push_back((char)code);
                            else {
                                // упрощённая поддержка: заменим на '?'
                                out.push_back('?');
                            }
                        }
                    } else {
                        unsigned int code = std::stoul(ent.substr(1), nullptr, 10);
                        if (code < 128) out.push_back((char)code);
                        else out.push_back('?');
                    }
                    i = semi + 1;
                    continue;
                } catch (...) {
                    // fallthrough -> treat literally
                }
            } else {
                std::string name = toLowerCopy(ent);
                if (name == "amp") { out += '&'; i = semi + 1; continue; }
                if (name == "lt")  { out += '<'; i = semi + 1; continue; }
                if (name == "gt")  { out += '>'; i = semi + 1; continue; }
                if (name == "quot"){ out += '"'; i = semi + 1; continue; }
                if (name == "apos"){ out += '\''; i = semi + 1; continue; }
                // неизвестная named entity -> leave as-is (or decode partially)
                // fall through to literal copy
            }
            // если сюда дошли — просто копируем как есть (&...;)
            out.append(s.substr(i, semi - i + 1));
            i = semi + 1;
        } else {
            out.push_back(s[i++]);
        }
    }
    return out;
}

// Убираем управляющие символы и нулевые байты из строки (защита от смешанных вхождений)
std::string stripControls(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (c == 0) continue;
        if (c == '\n' || c == '\r' || c == '\t' || (c >= 32)) out.push_back(c);
        // остальные управляющие символы опускаем
    }
    return out;
}

// Проверка схемы URL: получаем часть до ':' (если ':' встречается до первого '/' или '?' или '#')
std::string extractScheme(const std::string& val) {
    for (size_t i = 0; i < val.size(); ++i) {
        char c = val[i];
        if (c == ':' ) {
            // убедимся, что ':' не входит в часть типа 'http://'
            // схема считается валидной если до ':' только буквы/цифры/+-.
            bool ok = (i > 0);
            for (size_t j = 0; j < i && ok; ++j) {
                char cc = std::tolower((unsigned char)val[j]);
                if (!(std::isalnum((unsigned char)cc) || cc == '+' || cc == '-' || cc == '.')) ok = false;
            }
            if (ok) return toLowerCopy(val.substr(0, i));
            else return ""; // не распознано как схема
        }
        if (c == '/' || c == '?' || c == '#') break;
    }
    return ""; // относительный
}

bool isAllowedScheme(const std::string& scheme) {
    return allowedSchemes.find(scheme) != allowedSchemes.end();
}

// Проверка имени тега: только буквы/цифры/-, проверка case-insensitive
bool validTagNameChar(char c) {
    return std::isalnum((unsigned char)c) || c == '-' || c == ':';
}

// -------------------- Парсер тега --------------------

struct Attribute {
    std::string name;
    std::string value;
};

struct ParsedTag {
    bool valid = false;
    bool closing = false;
    bool selfClosing = false;
    std::string name;
    std::vector<Attribute> attrs;
};

ParsedTag parseTag(const std::string& tagStr) {
    ParsedTag out;
    size_t n = tagStr.size();
    if (n < 2 || tagStr[0] != '<') return out;

    size_t pos = 1;
    // комментарии и директивы: <!-- ... -->, <!DOCTYPE ...>, <? ... ?>
    if (pos < n && tagStr[pos] == '!') {
        // treat as invalid for tags (we will escape the whole string)
        return out;
    }
    if (pos < n && tagStr[pos] == '?') return out;

    // closing tag?
    if (pos < n && tagStr[pos] == '/') {
        out.closing = true;
        ++pos;
    }

    // read tag name
    std::string name;
    while (pos < n) {
        char c = tagStr[pos];
        if (std::isspace((unsigned char)c) || c == '>' || c == '/') break;
        if (!validTagNameChar(c)) { return out; } // invalid char in tagname => treat as invalid tag
        name.push_back(std::tolower((unsigned char)c));
        ++pos;
    }
    if (name.empty()) return out;
    out.name = name;

    // parse attributes until '>' or '/>'
    while (pos < n) {
        // skip spaces
        while (pos < n && std::isspace((unsigned char)tagStr[pos])) ++pos;
        if (pos >= n) break;
        if (tagStr[pos] == '>') { ++pos; break; }
        if (tagStr[pos] == '/' && pos + 1 < n && tagStr[pos+1] == '>') {
            out.selfClosing = true;
            pos += 2;
            break;
        }
        if (tagStr[pos] == '/') { // stray slash
            ++pos;
            continue;
        }

        // read attr name
        size_t an_start = pos;
        while (pos < n && (std::isalnum((unsigned char)tagStr[pos]) || tagStr[pos]=='-' || tagStr[pos]==':')) ++pos;
        if (pos == an_start) {
            // unexpected char -> skip until next space or >
            while (pos < n && tagStr[pos] != '>' && tagStr[pos] != '/') ++pos;
            continue;
        }
        std::string attrName = tagStr.substr(an_start, pos - an_start);
        // skip spaces
        while (pos < n && std::isspace((unsigned char)tagStr[pos])) ++pos;

        std::string attrValue;
        if (pos < n && tagStr[pos] == '=') {
            ++pos;
            while (pos < n && std::isspace((unsigned char)tagStr[pos])) ++pos;
            if (pos < n && (tagStr[pos] == '"' || tagStr[pos] == '\'')) {
                char q = tagStr[pos++];
                size_t valStart = pos;
                while (pos < n && tagStr[pos] != q) ++pos;
                attrValue = tagStr.substr(valStart, (pos < n ? pos - valStart : pos - valStart));
                if (pos < n && tagStr[pos] == q) ++pos;
            } else {
                // unquoted value: read until whitespace or '>' or '/'
                size_t valStart = pos;
                while (pos < n && !std::isspace((unsigned char)tagStr[pos]) && tagStr[pos] != '>' && tagStr[pos] != '/') ++pos;
                attrValue = tagStr.substr(valStart, pos - valStart);
            }
        } else {
            // attribute without value (boolean). We'll set value = "".
            attrValue = "";
        }

        // store attribute (raw)
        Attribute a;
        a.name = toLowerCopy(attrName);
        a.value = attrValue;
        out.attrs.push_back(std::move(a));
    }

    out.valid = true;
    return out;
}

// -------------------- Санитизация значений атрибутов --------------------

std::string sanitizeAttributeValue(const std::string& tag, const std::string& attr, const std::string& rawValue) {
    // 1) декодируем entities
    std::string v = decodeHtmlEntities(rawValue);
    // 2) убираем управляющие символы
    v = stripControls(v);
    // 3) trim
    v = trim(v);

    // 4) для ссылок и src — проверка схемы
    if (attr == "href" || attr == "src") {
        // привести к lower для схемы
        std::string schemeCandidate = extractScheme(v);
        if (!isAllowedScheme(schemeCandidate)) {
            return "#";
        }
        // если схема разрешена — вернуть оригинал (нельзя возвращать необработанную строку с <>&")
        return v;
    }

    // для остальных атрибутов — запрещаем javascript-like -->
    std::string low = toLowerCopy(v);
    // удаляем начальные невидимые символы
    size_t p = 0;
    while (p < low.size() && std::isspace((unsigned char)low[p])) ++p;
    if (low.size() > p) {
        if (low.substr(p, 11) == "javascript:" ||
            low.substr(p, 8)  == "vbscript:" ||
            low.substr(p, 5)  == "data:") {
            return "";
        }
    }
    return v;
}

// -------------------- Основная функция --------------------

std::string sanitizeHtml(const std::string& in, bool img_not_allowed = false) {
    auto input=join(split(in,"\n"),"<br>");
    std::string out;
    out.reserve(input.size());

    size_t pos = 0;
    while (pos < input.size()) {
        if (input[pos] == '<') {
            // попытка найти ближайший '>'
            size_t end = pos;
            bool found = false;
            while (end < input.size()) {
                if (input[end] == '>') { found = true; break; }
                ++end;
            }
            if (!found) {
                // нет закрывающего '>' — экранируем остаток
                out += escapeHtml(input.substr(pos));
                break;
            }

            std::string tagStr = input.substr(pos, end - pos + 1);
            ParsedTag p = parseTag(tagStr);
            if (!p.valid) {
                // Экранируем весь кусок как текст (включая <...>)
                out += escapeHtml(tagStr);
            } else {
                // Проверим разрешён ли тег (case-insensitive)
                std::string tagName = p.name; // уже lower
                if (allowedTags.find(tagName) == allowedTags.end() || (img_not_allowed && tagName == "img")) {
                    // запрещённый тег — экранируем полностью
                    out += escapeHtml(tagStr);
                } else {
                    // Формируем очищенный тег
                    if (p.closing) {
                        out += "</";
                        out += tagName;
                        out += ">";
                    } else {
                        out += "<";
                        out += tagName;
                        // перебираем атрибуты, оставляем только разрешённые
                        auto itAttrs = allowedAttributes.find(tagName);
                        std::unordered_set<std::string> allowedForTag;
                        if (itAttrs != allowedAttributes.end()) allowedForTag = itAttrs->second;

                        for (const Attribute &a : p.attrs) {
                            std::string an = a.name;
                            // убираем все on* события и style
                            if (an.size() >= 2 && an[0]=='o' && an[1]=='n') continue;
                            if (an == "style") continue;

                            // атрибут разрешён для тега?
                            bool allowed = (allowedForTag.empty()) ? false : (allowedForTag.find(an) != allowedForTag.end());
                            if (!allowed) continue;

                            std::string safeVal = sanitizeAttributeValue(tagName, an, a.value);
                            if (safeVal.empty() && (an=="href" || an=="src" || an=="title" || an=="alt")) {
                                // если важное значение стало пустым, заменяем на безопасный stub
                                if (an == "href" || an == "src") safeVal = "#";
                            }
                            // экранируем значение при выводе
                            out += " ";
                            out += an;
                            out += "=\"";
                            out += escapeAttribute(safeVal);
                            out += "\"";
                        }

                        // самозакрывающийся?
                        if (p.selfClosing) out += " /";
                        out += ">";
                    }
                }
            }

            pos = end + 1;
        } else {
            // обычный текст — просто экранируем отдельные опасные символы
            // Собираем кусок до следующего '<' для эффективности
            size_t next = input.find('<', pos);
            if (next == std::string::npos) next = input.size();
            out += escapeHtml(input.substr(pos, next - pos));
            pos = next;
        }
    }

    return out;
}

// -------------------- Простейшие тесты --------------------

#ifdef SANITIZE_HTML_TEST_MAIN
int main() {
    std::vector<std::string> tests = {
        "<a href=\"javascript:alert(1)\">x</a>",
        "<a href=\" JavaScript:alert(1)\">x</a>",
        "<a href=\"javascript&#x3A;alert(1)\">x</a>",
        "<a href='javascript:alert(1)'>x</a>",
        "<a href=javascript:alert(1)>x</a>",
        "<img src=x onerror=alert(1)>",
        "<img src=\"x\" onerror=\"alert(1)\">",
        "<span style=\"background:url(javascript:alert(1))\">x</span>",
        "<svg/onload=alert(1)>",
        "<img src=\"x\" onerror=\"alert(1)\"><script>alert(2)</script>",
        "<a href=\"data:text/html,<script>alert(1)</script>\">test</a>",
        "<font color=\"red\" onclick=\"alert(1)\">text</font>",
        "<a href=\"https://example.com\" target=\"_blank\" onclick=\"stealCookies()\">link</a>",
        "<b>bold</b> and <unknown attr=x>bad</unknown>"
    };

    for (auto &t : tests) {
        std::cout << "IN : " << t << "\n";
        std::cout << "OUT: " << sanitizeHtml(t) << "\n\n";
    }
    return 0;
}
#endif
