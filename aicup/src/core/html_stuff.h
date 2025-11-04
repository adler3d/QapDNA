#include <unordered_set>
#include <regex>
#include <cctype>

// Разрешённые теги (без угловых скобок)
const std::unordered_set<std::string> allowedTags = {
    "img", "b", "small", "font", "span", "code", "s", "i", "a", "table", "th", "tr", "td"
};

// Разрешённые атрибуты для каждого тега
const std::unordered_map<std::string, std::unordered_set<std::string>> allowedAttributes = {
    {"img", {"src"}},
    {"font", {"color"}},
    {"a", {"href"}}
};

// Экранирование &, <, >
std::string escapeHtml(const std::string& text) {
    std::string result;
    for (char c : text) {
        switch (c) {
            case '&':  result += "&amp;";  break;
            case '<':  result += "&lt;";   break;
            case '>':  result += "&gt;";   break;
            default:   result += c;         break;
        }
    }
    return result;
}

// Проверяет, является ли символ буквой (a-z, A-Z)
bool isLetter(char c) {
    return std::isalpha(static_cast<unsigned char>(c));
}

// Извлекает имя тега из строки вида "<tag ...>" или "</tag>"
std::string extractTagName(const std::string& tagStr) {
    if (tagStr.empty() || tagStr[0] != '<') return "";

    size_t start = 1;
    if (tagStr[1] == '/') start = 2;  // Пропускаем '/' в закрывающем теге

    std::string name;
    for (size_t i = start; i < tagStr.size(); ++i) {
        char c = tagStr[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' || c == '/') {
            break;
        }
        if (!isLetter(c)) return "";  // Небуква — некорректный тег
        name += c;
    }
    return name;
}

// Проверяет, разрешён ли атрибут для данного тега
bool isAllowedAttribute(const std::string& tag, const std::string& attr,bool img_not_allowed) {
    auto it = allowedAttributes.find(tag);
    if (it == allowedAttributes.end()) return false;
    if(img_not_allowed&&it->first=="img")return false;
    return it->second.find(attr) != it->second.end();
}

// Очищает значение атрибута (например, блокирует javascript:)
std::string sanitizeAttributeValue(const std::string& attr, const std::string& value) {
    if (attr == "href") {
        std::string lowerVal = value;
        std::transform(lowerVal.begin(), lowerVal.end(), lowerVal.begin(), ::tolower);
        if (lowerVal.find("javascript:") == 0 ||
            lowerVal.find("vbscript:") == 0 ||
            lowerVal.find("data:") == 0) {
            return "#";
        }
    }
    return value;
}

// Основная функция санитизации
std::string sanitizeHtml(const std::string& input,bool img_not_allowed=false) {
    std::string result;
    size_t pos = 0;
    size_t len = input.length();

    while (pos < len) {
        if (input[pos] == '<') {
            // Находим конец тега
            size_t end = input.find('>', pos);
            if (end == std::string::npos) {
                // Нет '>' — экранируем всё до конца
                result += escapeHtml(input.substr(pos));
                break;
            }

            std::string tagStr = input.substr(pos, end - pos + 1);
            std::string tagName = extractTagName(tagStr);

            if (tagName.empty() || allowedTags.find(tagName) == allowedTags.end()) {
                // Тег не разрешён — экранируем
                result += escapeHtml(tagStr);
            } else {
                // Разрешённый тег — собираем заново
                std::string cleanedTag;

                // Определяем, открывающий или закрывающий тег
                if (tagStr[1] == '/') {
                    cleanedTag = "</" + tagName + ">";
                } else {
                    cleanedTag = "<" + tagName;

                    // Обрабатываем атрибуты
                    std::regex attrRegex("\\s+([a-zA-Z]+)=\\s*\"([^\"]*)\"");
                    auto words_begin = std::sregex_iterator(tagStr.begin(), tagStr.end(), attrRegex);
                    auto words_end = std::sregex_iterator();

                    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                        std::string attrName = i->str(1);
                        std::string attrValue = i->str(2);

                        if (isAllowedAttribute(tagName, attrName,img_not_allowed)) {
                            std::string safeValue = sanitizeAttributeValue(attrName, attrValue);
                            cleanedTag += " " + attrName + "=\"" + safeValue + "\"";
                        }
                    }

                    cleanedTag += ">";
                }

                result += cleanedTag;
            }

            pos = end + 1;
        } else {
            // Обычный символ — добавляем в результат
            result += input[pos];
            pos++;
        }
    }

    return result;
}

