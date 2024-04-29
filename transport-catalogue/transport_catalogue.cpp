#include "transport_catalogue.h"

#include <algorithm>
#include <numeric>

namespace transport {

void TransportCatalogue::AddStop(Stop new_stop) {
    // добавляем остановку в контекнер (дек) всех остановок
    stops_.push_back(std::move(new_stop));
    // указатель на добавленную остановку помещаем в словарь 
    // ключ - название этой остановка
    Stop* added_stop_ptr = &stops_.back();
    std::string_view added_stop_name = added_stop_ptr->name;
    stops_dictionary_[added_stop_name] = added_stop_ptr;
    
    // добавляем остановку в список остановок с автобусами (список пуст, будет заполняться по мере добавления автобусов)
    busses_at_stop_[added_stop_name] = {};
}


void TransportCatalogue::AddBus(Bus new_bus) {
    // добавляем автобус в контекнер (дек) с автобусами
    buses_.push_back(std::move(new_bus));
    // в словарь автобусов помещаем добавленный автобус, 
    // при этом ключ - это назваие маршрута (= автобуса = его номер)
    Bus* added_bus_ptr = &buses_.back();
    std::string_view added_bus_name = added_bus_ptr->name;
    buses_dictionary_[added_bus_name] = added_bus_ptr;
    
    // добавляем автобус на остановку (в список автобусов по каждой остановке)
    for (auto stop_tmp : added_bus_ptr->unique_stops) {
        busses_at_stop_[stop_tmp].push_back(added_bus_name);
    }

}


Stop* TransportCatalogue::FindStop(std::string_view stop_name) const{
    // если запрашиваемой остановки нет в базе, то возвращаем пустую "остановку"
    if (!stops_dictionary_.count(stop_name)) {
        Stop* empty_stop = new Stop{};
        return empty_stop;
    }
    return stops_dictionary_.at(stop_name);
}


Bus* TransportCatalogue::FindBus(std::string_view bus_name) const {
    if (!buses_dictionary_.count(bus_name)) {
        Bus* empty_bus = new Bus{};
        return empty_bus;
    }
    return buses_dictionary_.at(bus_name);
}


/**
 * Возвращает информацию о маршруте в виде структуры BusInfo.
 * Если маршрут не найден, то статусное поле valid_state будет false.
*/
BusInfo TransportCatalogue::GetBusInfo(std::string_view bus_name) const{
    Bus* bus_ptr = FindBus(bus_name);
    if (!detail::IsBus(bus_ptr)) {
        BusInfo bus_empty_info = {};
        // статус valid_state будет false
        return bus_empty_info;
    }
    BusInfo bus_info;
    bus_info.valid_state = true;  // автобус есть
    bus_info.name = bus_ptr->name;  // добавляем имя
    bus_info.num_of_stops_on_route = bus_ptr->stops_on_route.size();  // добавляем количество всех остановок
    bus_info.num_of_unique_stops = bus_ptr->unique_stops.size();  // // добавляем количество уникальных остановки
    bus_info.route_length = detail::ComputeRouteLength(bus_ptr);  // считаем и добавляем длину пути

    return bus_info;
    
}


/** 
     * Возвращает инфу по остановrе (список автобусов) в виде структуры
     * Если остановки нет, то вернет структуру со статусом valid_state  false
     * Если остановка есть, но автобусы через нее не проходят, то buses_list будет пуст
    */
StopInfo TransportCatalogue::GetStopInfo(std::string_view stop_name) const {
    Stop* stop_ptr = FindStop(stop_name);
    if (!detail::IsStop(stop_ptr)) {
        // статус valid_state будет false
        return {};
    }

    // вытаскиваем автобусы по данной остановке, список автобусов м.б. пуст
    std::vector<std::string_view> buses_at_stop = busses_at_stop_.at(stop_name);
    // сортируем по алфавиту
    std::sort(buses_at_stop.begin(), buses_at_stop.end());
    // формируем выходную информацию

    return {std::string(stop_name), buses_at_stop, true};
}


namespace detail {

// проверяет, есть ли остановка/инфа об остановке по указателю
bool IsStop(Stop* stop_to_check) {
    if (stop_to_check->name.empty()) {
        return false;
    }
    return true;
}


// проверяет, есть ли маршрут/инфа о маршруте по указателю
bool IsBus(Bus* bus_to_check) {
    if (bus_to_check->name.empty()) {
        return false;
    }
    return true;
}

double ComputeRouteLength(Bus* bus) {
    double route_length = 0;
    for (size_t i = 1; i < bus->stops_on_route.size(); i++) {
        route_length += geo::ComputeDistance(bus->stops_on_route[i]->coordinates, bus->stops_on_route[i-1]->coordinates);
    }
    return route_length;
}

} // namespace detail

}  // namespace transport