#include "input_reader.h"

#include <algorithm>
#include <cassert>
#include <iterator>

using namespace std::literals;
using namespace loading;

namespace detail {
/**
 * Парсит строку вида "10.123,  -30.1837" и возвращает пару координат (широта, долгота)
 */
geo::Coordinates ParseCoordinates(std::string_view str) {
    static const double nan = std::nan("");

    auto not_space = str.find_first_not_of(' ');
    auto comma = str.find(',');

    if (comma == str.npos) {
        return {nan, nan};
    }

    auto not_space2 = str.find_first_not_of(' ', comma + 1);

    double lat = std::stod(std::string(str.substr(not_space, comma - not_space)));
    double lng = std::stod(std::string(str.substr(not_space2)));

    return {lat, lng};
}


/* Закомментировано, потому что оказалось, что удалять лишние пробелы внутри названий не требуется, 
что странно, т.к. в реальной жизни может привести к некорректному поведению
// Удаляет лишние пробелы (между словами, если они есть)
// !! Изменяет входную строку
*/
/*
void DeleteExcessIntraSpaces(std::string& name) {
    const auto start = name.find_first_not_of(' ');
    if (start == name.npos) {
        return;
    }
    auto space_pos_1 = name.find(' ', start);
    auto space_pos_2 = name.find(' ', space_pos_1 + 1);

    while (space_pos_2 != name.npos) {
        if (space_pos_2 == space_pos_1 + 1) {
            name.erase(space_pos_2, 1);
            space_pos_2 = name.find(' ', space_pos_1 + 1);
        }
        else {
            // смещаем точку отсчета двойных пробелов
            space_pos_1 = space_pos_2;
            space_pos_2 = name.find(' ', space_pos_1 + 1);
        }        
    }
    return;
}
*/


/**
 * Удаляет пробелы в начале и конце строки
 */
std::string_view Trim(std::string_view string) {
    const auto start = string.find_first_not_of(' ');
    if (start == string.npos) {
        return {};
    }
    return string.substr(start, string.find_last_not_of(' ') + 1 - start);
}


/**
 * Разбивает строку string на n строк, с помощью указанного символа-разделителя delim
 */
std::vector<std::string_view> Split(std::string_view string, char delim) {
    std::vector<std::string_view> result;

    size_t pos = 0;
    while ((pos = string.find_first_not_of(' ', pos)) < string.length()) {
        auto delim_pos = string.find(delim, pos);
        if (delim_pos == string.npos) {
            delim_pos = string.size();
        }
        if (auto substr = Trim(string.substr(pos, delim_pos - pos)); !substr.empty()) {
            result.push_back(substr);
        }
        pos = delim_pos + 1;
    }

    return result;
}


/**
 * Парсит маршрут.
 * Для кольцевого маршрута (A>B>C>A) возвращает массив названий остановок [A,B,C,A]
 * Для некольцевого маршрута (A-B-C-D) возвращает массив названий остановок [A,B,C,D,C,B,A]
 */
std::vector<std::string_view> ParseRoute(std::string_view route) {
    if (route.find('>') != route.npos) {
        return Split(route, '>');
    }

    auto stops = Split(route, '-');
    std::vector<std::string_view> results(stops.begin(), stops.end());
    results.insert(results.end(), std::next(stops.rbegin()), stops.rend());

    return results;
}


CommandDescription ParseCommandDescription(std::string_view line) {
    auto colon_pos = line.find(':');
    if (colon_pos == line.npos) {
        return {};
    }

    auto space_pos = line.find(' ');
    if (space_pos >= colon_pos) {
        return {};
    }

    auto not_space = line.find_first_not_of(' ', space_pos);
    if (not_space >= colon_pos) {
        return {};
    }

    return {std::string(line.substr(0, space_pos)),
            std::string(line.substr(not_space, colon_pos - not_space)),
            std::string(line.substr(colon_pos + 1))};
}


}  // namespace detail 


void InputReader::ParseLine(std::string_view line) {
    auto command_description = detail::ParseCommandDescription(line);
    if (command_description) {
        commands_.push_back(std::move(command_description));
    }
}


// Обработка запросов на добавление остановок
// Идем по списку команд и обрабатывааем только запросы на добавление остановок
void InputReader::AddStopsToCatalogue([[maybe_unused]] transport::TransportCatalogue& catalogue) const {
    for(const CommandDescription& command_cur : commands_) {
        // если запрос не про остановку, то переходим к следующему запросу
        if (detail::Trim(command_cur.command) != "Stop"s) {
            continue;
        }
        // вытаскиваем имя остановки  без пробелов в начале и в конце
        std::string stop_name = std::string(detail::Trim(command_cur.id));
        // DeleteExcessIntraSpaces(stop_name);  // удаляем лишние пробелы в середине
        // stop_name = DeleteExcessIntraSpaces(stop_name)


        // вытаскиваем координаты:
        std::string coordinates_str = std::string(detail::Trim(command_cur.description));
        // DeleteExcessIntraSpaces(coordinates_str);  // удалили лишние пробелы
        geo::Coordinates coordinates = detail::ParseCoordinates(coordinates_str);

        transport::Stop stop_cur;
        stop_cur.name = stop_name;
        stop_cur.coordinates = coordinates; 

        catalogue.AddStop(std::move(stop_cur));
    }
    return;
}

// Обработка запросов на добавление маршрутов автобусов
// идем по списку команд и обрабатываем только добавление автобусов
void InputReader::AddBusesToCatalogue([[maybe_unused]] transport::TransportCatalogue& catalogue) const {
        // 1. Обработка запросов на добавление маршрутов автобусов
    for(const CommandDescription& command_cur : commands_) {
        // если запрос не про остановку, то переходим к следующему запросу
        if (detail::Trim(command_cur.command) != "Bus"s) {
            continue;
        }

        // вытаскиваем имя маршрута  без пробелов в начале и в конце
        std::string bus_name = std::string(detail::Trim(command_cur.id));
        // DeleteExcessIntraSpaces(bus_name);  // удаляем лишние пробелы в середине

        // вытаскиваем маршрут:
        std::string route_str = std::string(detail::Trim(command_cur.description));
        // DeleteExcessIntraSpaces(route_str);  // удалили лишние пробелы

        // вытаскиваем все остановки
        std::vector<std::string_view> stops_names_on_route = detail::ParseRoute(route_str);
        
        // сюда будем записывать набор уникальных остановок
        std::unordered_set<std::string_view> unique_stops;

        std::vector<transport::Stop*> stops_on_route;

        for (std::string_view stop_name_cur : stops_names_on_route) {
            // ищем остановку в справочнике
            transport::Stop* stop_cur = catalogue.FindStop(stop_name_cur);
            // если нашлась, то добавляем в список остановок по автобусу
            if (!stop_cur->name.empty()) {
                // добавляем название остановки в перечень уникальных
                unique_stops.insert(stop_cur->name);
                // перемещаем остановку в массив остановок
                stops_on_route.push_back(std::move(stop_cur));
            }
            // если не нашлась, то остановка не будет добавлена
            // можно добавлять "пустую" остановку, имеющую только имя
        }

        // заполняем информацию о маршруте
        transport::Bus bus_cur;
        bus_cur.name = bus_name;
        bus_cur.stops_on_route = std::move(stops_on_route);
        bus_cur.unique_stops = std::move(unique_stops);
        
        // добавляем маршрут в справочник 
        catalogue.AddBus(std::move(bus_cur));
    }
    return;
}


void InputReader::ApplyCommands([[maybe_unused]] transport::TransportCatalogue& catalogue) const {
    // Реализуйте метод самостоятельно

    // 0. Обработка запросов на добавление остановок
    AddStopsToCatalogue(catalogue);

    // 1. Обработка запросов на добавление маршрутов автобусов
    AddBusesToCatalogue(catalogue);


    return;

}