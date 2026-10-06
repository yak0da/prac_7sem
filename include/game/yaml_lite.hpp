#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "config.hpp"
#include "types.hpp"

namespace mafia {

// Ошибка разбора YAML-конфига ролей.
class YamlError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Содержимое config/roles.yaml (нужное нам подмножество).
struct RolesFile {
    int mafiaDivisor = -1;          // -1 = не задан, оставить значение из CLI
    std::vector<Role> roles;        // особые роли (комиссар, доктор, ниндзя, ...)
};

// Разбирает простой YAML: скаляры и список roles с "- item".
RolesFile parseRolesYaml(const std::string& text);

// Читает файл с диска.
RolesFile loadRolesYamlFile(const std::string& path);

// Роли для раздачи: из YAML или defaultSpecialRoles(), плюс применяет mafia_divisor к Config.
std::vector<Role> specialRolesFor(Config& config);

}  // namespace mafia
