#include <regex>
#include <unordered_set>
// Список разрешённых тегов (без угловых скобок)
const std::unordered_set<std::string> allowedTags = {
    "img", "b", "small", "font", "span", "code", "s", "i", "a", "table", "th", "tr", "td"
};

// Разрешённые атрибуты для каждого тега
const std::unordered_map<std::string, std::unordered_set<std::string>> allowedAttributes = {
    {"img", {"src"}},
    {"font", {"color"}},
    {"a", {"href"}},
    // Для остальных тегов атрибуты не разрешены
};

// Экранирование символов &, <, > для текста вне разрешённых тегов
std::string escapeHtml(const std::string& text) {
    std::string result;
    for (char c : text) {
        switch (c) {
            case '&':  result += "&amp;";   break;
            case '<':  result += "&lt;";    break;
            case '>':  result += "&gt;";    break;
            default:   result += c;          break;
        }
    }
    return result;
}

// Проверка, является ли строка корректным именем тега (без пробелов, спецсимволов)
bool isValidTagName(const std::string& tag) {
    if (tag.empty()) return false;
    for (char c : tag) {
        if (!isalpha(c)) return false; // Только буквы
    }
    return true;
}

// Извлекает имя тега из строки вида "<tag ...>" или "</tag>"
std::string extractTagName(const std::string& tagStr) {
    std::regex tagRegex("<\\s*/?\\s*([a-zA-Z]+)");
    std::smatch match;
    if (std::regex_search(tagStr, match, tagRegex) && match.size() > 1) {
        return match[1].str();
    }
    return "";
}

// Проверяет, разрешён ли атрибут для данного тега
bool isAllowedAttribute(const std::string& tag, const std::string& attr,bool img_not_allowed) {
    auto it = allowedAttributes.find(tag);
    if (it == allowedAttributes.end()) {
        return false; // Нет разрешённых атрибутов для этого тега
    }
    if(img_not_allowed&&it->first=="img")return false;
    return it->second.find(attr) != it->second.end();
}

// Обрабатывает атрибут (очищает значение, например, убирает JS-протоколы)
std::string sanitizeAttributeValue(const std::string& attr, const std::string& value) {
    if (attr == "href") {
        // Запрещаем javascript:, vbscript: и т.п.
        std::string lowerVal = value;
        std::transform(lowerVal.begin(), lowerVal.end(), lowerVal.begin(), ::tolower);
        if (lowerVal.find("javascript:") == 0 ||
            lowerVal.find("vbscript:") == 0 ||
            lowerVal.find("data:") == 0) {
            return "#"; // Заменяем на безопасную ссылку
        }
    }
    // Для других атрибутов просто возвращаем значение (можно добавить очистку)
    return value;
}

// Основная функция санитизации
std::string sanitizeHtml(const std::string& input,bool img_not_allowed=true) {
    std::string result;
    size_t pos = 0;
    size_t len = input.length();

    while (pos < len) {
        if (input[pos] == '<') {
            // Находим конец тега
            size_t end = input.find('>', pos);
            if (end == std::string::npos) {
                // Нет закрывающего '>', считаем это текстом
                result += escapeHtml(input.substr(pos));
                break;
            }

            std::string tagStr = input.substr(pos, end - pos + 1);
            std::string tagName = extractTagName(tagStr);

            if (tagName.empty() || !isValidTagName(tagName)) {
                // Некорректный тег — экранируем
                result += escapeHtml(tagStr);
            } else if (allowedTags.find(tagName) != allowedTags.end()) {
                // Разрешённый тег — копируем с очисткой атрибутов
                std::regex attrRegex("\\s+([a-zA-Z]+)\\s*=\\s*\"([^\"]*)\"");
                std::sregex_iterator it(tagStr.begin(), tagStr.end(), attrRegex);
                std::sregex_iterator end;

                std::string cleanedTag = "<" + tagName;
                for (; it != end; ++it) {
                    std::string attrName = it->str(1);
                    std::string attrValue = it->str(2);

                    if (isAllowedAttribute(tagName, attrName,img_not_allowed)) {
                        std::string safeValue = sanitizeAttributeValue(attrName, attrValue);
                        cleanedTag += " " + attrName + "=\"" + safeValue + "\"";
                    }
                }

                // Проверяем, закрывающий ли тег
                if (tagStr.find("/") != std::string::npos && tagStr.find("/") < tagStr.find(">")) {
                    cleanedTag += "</" + tagName + ">";
                } else {
                    cleanedTag += ">";
                }

                result += cleanedTag;
            } else {
                // Запрещённый тег — экранируем
                result += escapeHtml(tagStr);
            }

            pos = end + 1;
        } else {
            // Обычный текст — экранируем
            result += escapeHtml(std::string(1, input[pos]));
            pos++;
        }
    }

    return result;
}
