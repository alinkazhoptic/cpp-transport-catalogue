#pragma once

#include "geo.h"

#include <string>
#include <deque>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

#include "domain.h"

namespace transport{

using namespace domain;


using StopsPair = std::pair<const Stop*, const Stop*>;
// Вектор пар <остановка-расстояние до нее>
using DistancesVector = std::vector<std::pair<std::string, int>>;



class TransportCatalogue {
    // Реализуйте класс самостоятельно
public:

    TransportCatalogue();

    ~TransportCatalogue(); // Деструктор здесь важен. Его тело будет в .cpp файле

    // Перемещающий конструктор класса unique_ptr не бросает исключений,
    // поэтому мы можем гарантировать отсутствие исключений при перемещении 
    TransportCatalogue(TransportCatalogue&&) noexcept;
    TransportCatalogue& operator=(TransportCatalogue&&) noexcept;

    // Копирующие конструктор и оператор присваивания - запрещаем, т.к. пока не требуются по заданию
    TransportCatalogue(const TransportCatalogue& other) = delete;
    TransportCatalogue& operator=(const TransportCatalogue& other) = delete;
    
    // Добавляет остановку в каталог 
    void AddStop(Stop new_stop);
    // Добавляет остановку и расстояния в каталог 
    void AddStop(Stop new_stop, const DistancesVector& distances_to_stops);

    // Добавляет автобус в каталог
    void AddBus(Bus new_bus);
    // Добавляет маршрут (автобус) и его остановки в каталог
    void AddBus(std::string_view bus_name, const std::vector<std::string_view>& stops_names, bool round_flag);

    const Stop* FindStop(std::string_view stop_name) const;
    const Bus* FindBus(std::string_view bus_name) const;

    /** 
     * возвращает инфу по маршруту (список остановок) в виде структуры
     * если маршрута нет, то вернет структуру со статусом valid_state  false
    */
    std::optional<BusInfo> GetBusInfo(std::string_view bus_name) const;

    /** 
     * возвращает инфу по остановке (список автобусов) в виде структуры
     * если остановки нет, то вернет структуру со статусом valid_state  false
     * если остановка есть, но автобусы через нее не проходят, то buses_list будет пуст
    */
    std::optional<StopInfo> GetStopInfo(std::string_view stop_name) const;


    /*
    Возвращает вектор всех автобусов в сортированном порядке
    */
    std::vector<std::pair<std::string_view, const Bus*>> GetAllBuses() const;

    /*
    Возвращает вектор всех остановок
    */
    std::vector<const Stop*> GetAllStops() const;

    /*
    Возвращает вектор всех остановок, через которые проходят автобусы
    */
    std::vector<const Stop*> GetAllStopsBusPassingThrough() const;



    // int GetDistanceBetweenStops(const Stop* stop1, const Stop* stop2) const;

    int GetDistanceBetweenStops(std::string_view stop_A_name, std::string_view stop_B_name) const;

    void SetDistanceBetweenStops(std::string_view stopA_name, std::string_view stopB_name, int distance);



private:
    struct Impl;
    std::unique_ptr<Impl> impl_; 

};



}  // namespace transport