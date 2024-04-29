#include "stat_reader.h"
#include "input_reader.h"

#include <iostream>
#include <iomanip>  // тут setprecision

// using namespace request;
using namespace transport;
using namespace std::literals;

namespace request_processing {

RequestDescription ParseRequestDescription(std::string_view line) {

    auto request_pos = line.find_first_not_of(' ', 0);
    if (request_pos == line.npos) {
        return {};
    }

    // пробел после названия запроса (Bus или Stop)
    auto space_pos = line.find(' ', request_pos);
    if (space_pos == line.npos) {
        return {};
    }

    // начало названия остановки или маршрута
    auto id_pos = line.find_first_not_of(' ', space_pos);
    if (space_pos == line.npos) {
        return {};
    }

    auto last_symbol = line.find_last_not_of(' ');
    
    // удаляем лишние пробелы
    // line = DeleteExcessIntraSpaces(line);

    return {std::string(line.substr(request_pos, space_pos - request_pos)),
            std::string(line.substr(id_pos, last_symbol + 1 - id_pos))};
}


void ParseAndPrintStat(const TransportCatalogue& transport_catalogue, std::string_view request,
                       std::ostream& output) {
    RequestDescription request_cur = ParseRequestDescription(request);
    // если строка оказалась пустая, то ничего не выводим в output
    if (!request_cur) {
        // output << ""s;
        return;
    }

    // обработка запроса Bus
    if (request_cur.request == "Bus"s) {
        BusInfo bus_info = transport_catalogue.GetBusInfo(request_cur.id);
        
        // если автобус не найден, выводим not found
        if (!bus_info.valid_state) {
            output << "Bus "s << request_cur.id << ": not found"s << std::endl;
            return;
        }

        // Bus X: R stops on route, U unique stops, L route length
        output << "Bus "s << request_cur.id << ": "s;
        output << bus_info.num_of_stops_on_route << " stops on route, "s;
        output << bus_info.num_of_unique_stops << " unique stops, "s;
        output << std::setprecision(6) << bus_info.route_length << " route length"s << std::endl;
    }

    if (request_cur.request == "Stop"s) {
        StopInfo stop_info = transport_catalogue.GetStopInfo(request_cur.id);
        // проверяем, существует ли остановка
        if (!stop_info.valid_state) {
            output << "Stop "s << request_cur.id << ": not found"s << std::endl;
            return; 
        }

        // есть ли маршруты через остановку
        if (stop_info.buses_list.empty()) {
            output << "Stop "s << request_cur.id << ": no buses"s << std::endl;
            return;
        }

        // Stop X: buses bus1 bus2 ... busN
        output << "Stop "s << request_cur.id << ": "s;
        output << "buses"s;

        for (auto bus_name : stop_info.buses_list) {
            output << " "s << bus_name;
        }
        output << std::endl;
    }

    return;
}


}  // namespace request_processing