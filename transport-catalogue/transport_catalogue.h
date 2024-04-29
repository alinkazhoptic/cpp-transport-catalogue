#pragma once

#include "geo.h"

#include <string>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

namespace transport{

struct Stop {
    std::string name;
    geo::Coordinates coordinates;
};


struct Bus {
    std::string name;
    std::vector<Stop*> stops_on_route;
    std::unordered_set<std::string_view> unique_stops;
};


struct BusInfo {
    std::string name;
    int num_of_stops_on_route;
    int num_of_unique_stops;
    double route_length;
    bool valid_state = false;
};

struct StopInfo {
    std::string name;
    std::vector<std::string_view> buses_list;
    bool valid_state = false;
};


using StopsPair = std::pair<Stop*, Stop>;


// Класс (структура) для хеша, учитывающего пару остановок
struct StopsPairHasher {
    size_t operator()(const StopsPair& stops_pair) const {
        // вытаскиваем имена остановок из пары и складываем как строки
        std::string names_cobination = stops_pair.first->name + stops_pair.second.name;                 
        return hasher(names_cobination);
    }
    std::hash<std::string> hasher;
};


class TransportCatalogue {
    // Реализуйте класс самостоятельно
public:

    TransportCatalogue() = default;

    // запрещаем копирование, ибо негоже тырить наши данные
    TransportCatalogue(const TransportCatalogue& other) = delete;
    
    void AddStop(Stop new_stop);
    void AddBus(Bus new_bus);

    Stop* FindStop(std::string_view stop_name) const;
    Bus* FindBus(std::string_view bus_name) const;

    /** 
     * возвращает инфу по маршруту (список остановок) в виде структуры
     * если маршрута нет, то вернет структуру со статусом valid_state  false
    */
    BusInfo GetBusInfo(std::string_view bus_name) const;

    /** 
     * возвращает инфу по остановке (список автобусов) в виде структуры
     * если остановки нет, то вернет структуру со статусом valid_state  false
     * если остановка есть, но автобусы через нее не проходят, то buses_list будет пуст
    */
    StopInfo GetStopInfo(std::string_view stop_name) const;



private:
    std::deque<Stop> stops_;
    std::unordered_map<std::string_view, Stop*> stops_dictionary_;

    std::deque<Bus> buses_;
    std::unordered_map<std::string_view, Bus*> buses_dictionary_;

    std::unordered_map<std::string_view, std::vector<std::string_view>> busses_at_stop_;

    std::unordered_map<StopsPair, double, StopsPairHasher> distances_; 

};

namespace detail {

// Проверяет наличие остановки по указателю
bool IsStop(Stop* stop_to_check);
// Проверяет наличие автобуса по указателю
bool IsBus(Bus* bus_to_check);
// Считает длину пути по маршруту
double ComputeRouteLength(Bus* bus);

}  // namespace detail


}  // namespace transport