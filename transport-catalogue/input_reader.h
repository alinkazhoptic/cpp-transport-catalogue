#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "geo.h"
#include "transport_catalogue.h"

namespace loading {

struct CommandDescription {
    // Определяет, задана ли команда (поле command непустое)
    explicit operator bool() const {
        return !command.empty();
    }

    bool operator!() const {
        return !operator bool();
    }

    std::string command;      // Название команды
    std::string id;           // id маршрута или остановки
    std::string description;  // Параметры команды
};

/* Закомментировано, потому что оказалось, что удалять лишние пробелы внутри названий не требуется, 
что странно, т.к. в реальной жизни может привести к некорректному поведению
// Удаляет лишние пробелы (между словами, если они есть)
// !! Изменяет входную строку
void DeleteExcessIntraSpaces(std::string& name);
*/


class InputReader {
public:
    /**
     * Парсит строку в структуру CommandDescription и сохраняет результат в commands_
     */
    void ParseLine(std::string_view line);

    /**
     * Наполняет данными транспортный справочник, используя команды из commands_
     */
    void ApplyCommands(transport::TransportCatalogue& catalogue) const;

private:
    std::vector<CommandDescription> commands_;

    void AddStopsToCatalogue([[maybe_unused]] transport::TransportCatalogue& catalogue) const;
    void AddBusesToCatalogue([[maybe_unused]] transport::TransportCatalogue& catalogue) const;
    
};

}  // namespace loading