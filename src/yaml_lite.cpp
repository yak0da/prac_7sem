#include <game/yaml_lite.hpp>

#include <game/state.hpp>

#include <cctype>
#include <fstream>
#include <sstream>

namespace mafia {

namespace {

std::string trim(std::string_view s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

std::string stripComment(std::string_view line) {
    const auto hash = line.find('#');
    if (hash == std::string_view::npos) return std::string(line);
    return std::string(line.substr(0, hash));
}

std::string unquote(std::string s) {
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
                          (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

enum class ListKind { None, Roles, Characters };

}  // namespace

RolesFile parseRolesYaml(const std::string& text) {
    RolesFile result;
    ListKind list = ListKind::None;
    std::istringstream in(text);
    std::string raw;
    int lineNo = 0;
    while (std::getline(in, raw)) {
        ++lineNo;
        std::string line = trim(stripComment(raw));
        if (line.empty()) continue;

        if (line == "roles:") {
            list = ListKind::Roles;
            continue;
        }
        if (line == "characters:") {
            list = ListKind::Characters;
            continue;
        }

        if (list != ListKind::None && line.starts_with("- ")) {
            const std::string item = unquote(trim(line.substr(2)));
            if (list == ListKind::Roles) {
                auto role = roleFromKey(item);
                if (!role) {
                    throw YamlError("строка " + std::to_string(lineNo) +
                                    ": неизвестная роль \"" + item + "\"");
                }
                if (*role != Role::Civilian && *role != Role::Mafia) {
                    result.roles.push_back(*role);
                }
            } else {
                if (!item.empty()) result.characters.push_back(item);
            }
            continue;
        }

        if (list != ListKind::None && !line.starts_with("- ")) {
            list = ListKind::None;
        }

        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            throw YamlError("строка " + std::to_string(lineNo) + ": ожидался ключ: значение");
        }
        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if (key == "mafia_divisor") {
            try {
                result.mafiaDivisor = std::stoi(value);
            } catch (...) {
                throw YamlError("mafia_divisor должен быть целым числом");
            }
            if (result.mafiaDivisor < 3) {
                throw YamlError("mafia_divisor должен быть >= 3");
            }
        } else if (key == "roles") {
            list = ListKind::Roles;
        } else if (key == "characters") {
            list = ListKind::Characters;
        }
        // прочие ключи игнорируем
    }
    return result;
}

RolesFile loadRolesYamlFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw YamlError("не удалось открыть файл ролей: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return parseRolesYaml(ss.str());
}

std::vector<Role> specialRolesFor(Config& config) {
    if (config.rolesConfig.empty()) return defaultSpecialRoles();
    RolesFile file = loadRolesYamlFile(config.rolesConfig);
    if (file.mafiaDivisor >= 3) config.mafiaDivisor = file.mafiaDivisor;
    if (file.roles.empty()) return defaultSpecialRoles();
    return file.roles;
}

std::vector<std::string> charactersFor(const Config& config) {
    if (config.rolesConfig.empty()) return {};
    try {
        return loadRolesYamlFile(config.rolesConfig).characters;
    } catch (const YamlError&) {
        return {};
    }
}

}  // namespace mafia
