#include "input_reader.h"

#include <algorithm>
#include <cassert>
#include <iterator>

using namespace std::literals;
using namespace loading;

namespace detail {

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
 * Разбивает строку string на 2 строки, с помощью указанной строки-разделителя delim
 */
std::pair<std::string_view, std::string_view> SplitIntoTwoStrings(std::string_view string_sv, std::string delim) {
    std::pair<std::string_view, std::string_view> result;

    int pos_delim = string_sv.find(delim);
    std::string_view str_left = string_sv.substr(0, pos_delim);
    str_left = Trim(str_left);
    std::string_view str_right = string_sv.substr(pos_delim + delim.size());
    str_right = Trim(str_right);

    return {str_left, str_right};
}


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


/**
 * Парсит строку вида "   10.123,    " и возвращает одну координату (широту, долготу)
 */
double ParseOneCoordinate(std::string_view str) {
    std::string_view str_wo_spaces = Trim(str);
    double coordinate = std::stod(std::string(str_wo_spaces));
    return coordinate;
}


/**
 * Парсит две строки вида "   10.123,    " и возвращает координаты (широту, долготу)
 */
geo::Coordinates ParseCoordinates(std::string_view str_lat, std::string_view str_lng) {
    return {ParseOneCoordinate(str_lat), ParseOneCoordinate(str_lng)};
}


/**
 * Парсит маршрут.
 * Для кольцевого маршрута (A>B>C>A) возвращает массив названий остановок [A,B,C,A]
 * Для некольцевого маршрута (A-B-C-D) возвращает массив названий остановок [A,B,C,D,C,B,A]
 */
std::vector<std::string_view> ParseRoute(std::string_view route) {
    // случай линеййного маршрута
    if (route.find('>') != route.npos) {
        return Split(route, '>');
    }

    // случай кольцевого маршрута
    auto stops = Split(route, '-');
    std::vector<std::string_view> results(stops.begin(), stops.end());
    results.insert(results.end(), std::next(stops.rbegin()), stops.rend());

    return results;
}

std::string_view ExtractStopName(std::string_view stop_line_sv){

/*     int pos_to = stop_line_sv.find("to");
    stop_line_sv = stop_line_sv.substr(pos_to + 2);
    stop_line_sv = Trim(stop_line_sv);
    return stop_line_sv; */
    return SplitIntoTwoStrings(stop_line_sv, "to").second;
}


/**
 * Парсит одно расстояние из строки вида " 7500m to Rossoshanskaya ulitsa ".
*/
std::pair<std::string, int> ParseOneDistance(std::string_view str) {
    // удаляем крайние пробелы
    std::string_view str_wo_spaces = Trim(str);
    // разделяем строку на части по размерности [метры] - m
    std::pair<std::string_view, std::string_view> data_splited = SplitIntoTwoStrings(str_wo_spaces, "m");
    // вытаскиваем расстояние
    int distance = std::stoi(std::string(data_splited.first));
    // вытаскиваем название остановки
    // std::string_view stop_name_sv = data_splited.at(1);
    std::string stop_name = std::string(ExtractStopName(data_splited.second));
    return {stop_name, distance};
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

        // формируем вектор данных:
        std::vector<std::string_view> data_list = detail::Split(command_cur.description,',');
        geo::Coordinates coordinates = detail::ParseCoordinates(data_list.at(0), data_list.at(1));

        // формируем вектор названий остановок и расстояний до них:
        std::vector<std::pair<std::string, int>> distances_to_stops_cur;
        for (int i = 2; i < data_list.size(); i++) {
            distances_to_stops_cur.push_back(detail::ParseOneDistance(data_list.at(i)));
        } 


        /* // вытаскиваем координаты:
        std::string_view coordinates_str = detail::Trim(command_cur.description);
        // DeleteExcessIntraSpaces(coordinates_str);  // удалили лишние пробелы
        geo::Coordinates coordinates = detail::ParseCoordinates(coordinates_str);
        */

        transport::Stop stop_cur;
        stop_cur.name = stop_name;
        stop_cur.coordinates = coordinates; 

        catalogue.AddStop(std::move(stop_cur), distances_to_stops_cur);
        // catalogue.AddStop(std::move(stop_cur));
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
        // DeleteExcessIntraSpaces(bus_name);  // удаляем лишние пробелы в середине, 

        // вытаскиваем маршрут:
        std::string_view route_str = detail::Trim(command_cur.description);
        // DeleteExcessIntraSpaces(route_str);  // удалили лишние пробелы

        // вытаскиваем все остановки
        std::vector<std::string_view> stops_names_on_route = detail::ParseRoute(route_str);

        catalogue.AddBus(bus_name, stops_names_on_route);

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