#pragma once

/*
 * В этом файле вы можете разместить классы/структуры, которые являются частью предметной области (domain)
 * вашего приложения и не зависят от транспортного справочника. Например Автобусные маршруты и Остановки. 
 *
 * Их можно было бы разместить и в transport_catalogue.h, однако вынесение их в отдельный
 * заголовочный файл может оказаться полезным, когда дело дойдёт до визуализации карты маршрутов:
 * визуализатор карты (map_renderer) можно будет сделать независящим от транспортного справочника.
 *
 * Если структура вашего приложения не позволяет так сделать, просто оставьте этот файл пустым.
 *
 */

#include <string>
#include <unordered_set>
#include <vector>


#include "geo.h"

namespace domain {

struct Stop {
    std::string name;
    geo::Coordinates coordinates;
};


struct Bus {
    std::string name;
    std::vector<const Stop*> stops_on_route;
    std::unordered_set<std::string_view> unique_stops;
    bool is_round;
};


struct BusInfo {
    std::string name;
    int num_of_stops_on_route;
    int num_of_unique_stops;
    double route_length;
    double roads_route_length;
    double geo_route_length;
};


struct StopInfo {
    std::string name;
    std::vector<std::string_view> buses_list;
};

using StopsPair = std::pair<const Stop*, const Stop*>;

// Класс (структура) для хеша, учитывающего пару остановок
struct StopsPairHasher {
    size_t operator()(const StopsPair& stops_pair) const {
        // вытаскиваем имена остановок из пары и складываем как строки
        std::string names_cobination = stops_pair.first->name + stops_pair.second->name;                 
        return hasher(names_cobination);
    }

    std::hash<std::string> hasher;
};

// Класс(структура) для хеша указателя на остановку
// чтобы запихнуть в unordered_set<Stop*>
struct StopHasher {
    size_t operator()(const Stop* stop_ptr) const {
        // вытаскиваем имя остановки
        return hasher(stop_ptr->name);
    }

    std::hash<std::string> hasher;
};

// проверяет, есть ли остановка/инфа об остановке по указателю
bool IsStop(const Stop* stop_to_check);

// проверяет, есть ли маршрут/инфа о маршруте по указателю
bool IsBus(const Bus* bus_to_check);

double ComputeGeoRouteLength(const Bus* bus);


} //namespace domain

