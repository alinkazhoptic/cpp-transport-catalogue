#include "transport_catalogue.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>

namespace transport {

namespace detail {

// проверяет, есть ли остановка/инфа об остановке по указателю
bool IsStop(const Stop* stop_to_check) {
    if (stop_to_check == nullptr) {
        return false;
    }
    if (stop_to_check->name.empty()) {
        return false;
    }
    return true;
}


// проверяет, есть ли маршрут/инфа о маршруте по указателю
bool IsBus(const Bus* bus_to_check) {
    if (bus_to_check == nullptr) {
        return false;
    }
    if (bus_to_check->name.empty()) {
        return false;
    }
    return true;
}

double ComputeGeoRouteLength(const Bus* bus) {
    double route_length = 0;
    for (size_t i = 1; i < bus->stops_on_route.size(); i++) {
        route_length += geo::ComputeDistance(bus->stops_on_route[i]->coordinates, bus->stops_on_route[i-1]->coordinates);
    }
    return route_length;
}


} // namespace detail


// Структура TransportCatalogue::Impl содержит детали реализации класса TransportCatalogue
struct TransportCatalogue::Impl {

    Impl() = default;

    // Копирующий конструктор для структуры Impl
    // Если копирование структуры Impl не сводится к простому копированию её полей,
    // то пишем пользовательский конструктор копирования
    Impl (const Impl& other) = default;
/*         :buses_(other.buses_),
        buses_dictionary_(other.buses_dictionary_),
        busses_at_stop_(other.busses_at_stop_),
        stops_dictionary_(other.stops_dictionary_),
        stops_(other.stops_),
        distances_(other.distances_) {} */


    // Копирующий оператор присваивания
    // Если присваивание Impl не сводится к простому присваиванию полей,
    // нужно реализовать должным операцию присваивания вручную
    Impl& operator=(const Impl& other)  = default;
/*     {
        if (this == &other) {
            return *this;
        }
        this->buses_ = other.buses_;
        this->buses_dictionary_ = other.buses_dictionary_;
        this->busses_at_stop_ = other.busses_at_stop_;
        this->stops_dictionary_ = other.stops_dictionary_;
        this->stops_ = other.stops_;
        this->distances_ = other.distances_;
        return *this;
    } */
    

    // Добавляет остановку в каталог
    void AddStop(Stop new_stop) {
        // Если остановка уже существует, просто заполняем её поля
        Stop* stop_ptr = const_cast<Stop*>(FindStop(new_stop.name));
        if (stop_ptr) {
            // дополняем инфу об остановке
            stop_ptr->coordinates = std::move(new_stop.coordinates);
        }
        else {
            // добавляем остановку в контейнер (дек) всех остановок
            stops_.push_back(std::move(new_stop));
            // указатель на добавленную остановку помещаем в словарь 
            // ключ - название этой остановка
            Stop* added_stop_ptr = &stops_.back();
            std::string_view added_stop_name = added_stop_ptr->name;
            stops_dictionary_[added_stop_name] = added_stop_ptr;
            
            // добавляем остановку в список остановок с автобусами (список пуст, будет заполняться по мере добавления автобусов)
            busses_at_stop_[added_stop_name] = {};
        }
    }


    // Добавляет остановки и расстояния до соседних остановок в каталог
    void AddStop(Stop new_stop, const DistancesVector& distances_to_stops) {
        std::string stop_name = new_stop.name;
        
        // добавляем остановку 
        AddStop(std::move(new_stop));

        // Вытаскиваем расстояния и заполняем map distances_
        // остановка отправления (A)
        const Stop* stop_base = FindStop(stop_name);
        for (const auto& [stop_dest_name, dist] : distances_to_stops) {
            // остановка назначения
            const Stop* stop_destin = FindStop(stop_dest_name);
            // если такая остановка еще не существует, создадим её
            if (!stop_destin) {
                // создаем "пустую" остановку и добавляем в каталог
                Stop new_stop_cur;
                new_stop_cur.name = stop_dest_name;
                AddStop(std::move(new_stop_cur));
                // обновляем указатель
                stop_destin = FindStop(stop_dest_name);
            }
            // формируем пары остановок: forward для A->B и reverse для B->A
            const StopsPair stop_pair_forward = std::make_pair(stop_base, stop_destin);
            const StopsPair stop_pair_reverse = std::make_pair(stop_destin, stop_base);
            // Добавляем расстояние в прямом направлении (A->B)
            // distances_.insert({stop_pair_forward, dist});
            distances_[stop_pair_forward]= dist;

            // Добавляем расстояние в обратном направлении (B->A), если оно ещё не указано
            if (!distances_.count(stop_pair_reverse)) {
                distances_.insert({stop_pair_reverse, dist});
            }
        }
    }


    void AddBus(Bus new_bus) {
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


    void AddBus(std::string_view bus_name, const std::vector<std::string_view>& stops_names) {
        // сюда будем записывать набор уникальных остановок
        std::unordered_set<std::string_view> unique_stops;
        // а здесь собирать указатели на найденные остановки 
        std::vector<const transport::Stop*> stops_on_route;
        
        for (std::string_view stop_name_cur : stops_names) {
            // ищем остановку в справочнике
            const transport::Stop* stop_cur = FindStop(stop_name_cur);
            // если нашлась, то добавляем в список остановок по автобусу
            if (!stop_cur->name.empty()) {
                // добавляем название остановки в перечень уникальных
                unique_stops.insert(stop_cur->name);
                // перемещаем остановку в массив остановок
                stops_on_route.push_back(stop_cur);  // было с move, но т.к. возвращаем const указатель, то перемещать нельзя
            }
            // если не нашлась, то остановка не будет добавлена
            // можно добавлять "пустую" остановку, имеющую только имя
        }

        // заполняем информацию о маршруте
        transport::Bus bus_cur;
        bus_cur.name = bus_name;
        bus_cur.stops_on_route = stops_on_route;  // было с move
        bus_cur.unique_stops = std::move(unique_stops);

        // добавляем маршрут в справочник 
        AddBus(std::move(bus_cur));   
        
    }


    const Stop* FindStop(std::string_view stop_name) const{
        // если запрашиваемой остановки нет в базе, то возвращаем пустую "остановку"
        if (!stops_dictionary_.count(stop_name)) {
            return nullptr;
        }
        return stops_dictionary_.at(stop_name);
    }


    const Bus* FindBus(std::string_view bus_name) const {
        if (!buses_dictionary_.count(bus_name)) {
            return nullptr;
        }
        return buses_dictionary_.at(bus_name);
    }


/**
 * Возвращает информацию о маршруте в виде структуры BusInfo.
 * Если маршрут не найден, то статусное поле valid_state будет false.
*/
std::optional<BusInfo> GetBusInfo(std::string_view bus_name) const{
    const Bus* bus_ptr = FindBus(bus_name);
    if (!detail::IsBus(bus_ptr)) {
        return std::nullopt;
    }
    BusInfo bus_info;
    // bus_info.valid_state = true;  // автобус есть
    bus_info.name = bus_ptr->name;  // добавляем имя
    bus_info.num_of_stops_on_route = bus_ptr->stops_on_route.size();  // добавляем количество всех остановок
    bus_info.num_of_unique_stops = bus_ptr->unique_stops.size();  // // добавляем количество уникальных остановки
    bus_info.geo_route_length = detail::ComputeGeoRouteLength(bus_ptr);  // считаем и добавляем длину прямого пути
    bus_info.roads_route_length = ComputeRoadRouteLength(bus_ptr); // считаем и добавляем длину пути по дорогам

    return bus_info;
    
}


/** 
     * Возвращает инфу по остановrе (список автобусов) в виде структуры
     * Если остановки нет, то вернет структуру со статусом valid_state  false
     * Если остановка есть, но автобусы через нее не проходят, то buses_list будет пуст
    */
std::optional<StopInfo> GetStopInfo(std::string_view stop_name) const {
    const Stop* stop_ptr = FindStop(stop_name);
    if (!detail::IsStop(stop_ptr)) {
        return std::nullopt;
    }

    // вытаскиваем автобусы по данной остановке, список автобусов м.б. пуст
    std::vector<std::string_view> buses_at_stop = busses_at_stop_.at(stop_name);
    // сортируем по алфавиту
    std::sort(buses_at_stop.begin(), buses_at_stop.end());
    // формируем выходную информацию
    StopInfo stop_info = {std::string(stop_name), buses_at_stop};

    return stop_info;
}



private:
    std::deque<Stop> stops_;
    std::unordered_map<std::string_view, Stop*> stops_dictionary_;

    std::deque<Bus> buses_;
    std::unordered_map<std::string_view, Bus*> buses_dictionary_;

    std::unordered_map<std::string_view, std::vector<std::string_view>> busses_at_stop_;

    std::unordered_map<StopsPair, double, StopsPairHasher> distances_; 

    double ComputeRoadRouteLength(const Bus* bus) const {
        double roads_length = 0;
        for (int i = 0; i < bus->stops_on_route.size() - 1; i++) {
            const Stop* stop_A = bus->stops_on_route[i];
            const Stop* stop_B = bus->stops_on_route[i+1];
            StopsPair forward_route = std::make_pair(stop_A, stop_B);
            StopsPair reverse_route = std::make_pair(stop_B, stop_A);
            if (distances_.count(forward_route)) {
                roads_length += distances_.at(forward_route);
            }
            else if (distances_.count(reverse_route)) {
                roads_length += distances_.at(reverse_route);
            }
            else {
                std::cout << "Unknown distance between " << stop_A->name <<  " and " << stop_B->name << std::endl;
                continue;
            }
        }
        return roads_length;    
    }
};


// Конструктор класса, здесь необходимо проинициализировать структуру Impl 
TransportCatalogue::TransportCatalogue()
    : impl_(std::make_unique<Impl>()) {
}


// Деструктор следует разместить внутри .cpp-файла, где видно содержимое структуры Impl, причем ниже объявления этой структуры!
// Так как нас устраивает деструктор, сгенерированный компилятором, вместо
// тела деструктора напишем = default.
TransportCatalogue::~TransportCatalogue() = default;


// Явно объявляем перемещающий конструктор ниже объявления структуры Impl
TransportCatalogue::TransportCatalogue(TransportCatalogue&&) noexcept = default;


// Аналогично реализуем операцию перемещающего присваивания
TransportCatalogue& TransportCatalogue::operator=(TransportCatalogue&&) noexcept = default; 


void TransportCatalogue::AddStop(Stop new_stop) {
    impl_->AddStop(std::move(new_stop));  // тут была передача аргумента через std::move
}

void TransportCatalogue::AddStop(Stop new_stop, const DistancesVector& distances_to_stops) {
    impl_->AddStop(std::move(new_stop), distances_to_stops);  // тут была передача аргумента через std::move
} 

void TransportCatalogue::AddBus(Bus new_bus) {
    impl_->AddBus(std::move(new_bus));  // тут была передача аргумента через std::move
}

void TransportCatalogue::AddBus(std::string_view bus_name, const std::vector<std::string_view>& stops_names) {
    impl_->AddBus(bus_name, stops_names);
}

const Stop* TransportCatalogue::FindStop(std::string_view stop_name) const {
    return impl_->FindStop(stop_name);
}

const Bus* TransportCatalogue::FindBus(std::string_view bus_name) const {
    return impl_->FindBus(bus_name);
}

std::optional<BusInfo> TransportCatalogue::GetBusInfo(std::string_view bus_name) const{
    return impl_->GetBusInfo(bus_name);
}

std::optional<StopInfo> TransportCatalogue::GetStopInfo(std::string_view stop_name) const {
    return impl_->GetStopInfo(stop_name);
}


// Копирующий конструктор
TransportCatalogue::TransportCatalogue(const TransportCatalogue& other)
// Если other не пуст, копируем его поле impl_
: impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_) : nullptr) {
}


// Копирующий оператор присваивания - вариант 1
/* TransportCatalogue& TransportCatalogue::operator=(const TransportCatalogue& other) {
    if (this != std::addressof(other)) {
        if (!other.impl_) {     // Правый аргумент пуст?
            impl_.reset();
        } else if (impl_) {     // Левый и правый аргументы не пустые?
            assert(other.impl_);
            *impl_ = *other.impl_;
        } else {                // Левый аргумент пуст, а правый не пуст
            assert(!impl_ && other.impl_);
            impl_ = std::make_unique<Impl>(*other.impl_);
        }
    }
    return *this;
} */


// А Можно присваивать так, если присваивание Impl не даёт 
// заметных преимуществ в скорости или памяти перед копированием
// Копирующий оператор присваивания - вариант 2
TransportCatalogue& TransportCatalogue::operator=(const TransportCatalogue& other) {
    if (this != std::addressof(other)) {
        impl_ = other.impl_ ? std::make_unique<Impl>(*other.impl_) : nullptr;
    }
    return *this;
} 



}  // namespace transport