#include "transport_router.h"

#include <string>
#include <iostream>

using namespace routing;
using namespace std::literals;


bool routing::operator<(const EdgeWeight& lhs, const EdgeWeight& rhs) {
    return (lhs.duration < rhs.duration /* || 
            (lhs.duration == rhs.duration && lhs.span_count < rhs.span_count) */);
}

bool routing::operator>(const EdgeWeight& lhs, const EdgeWeight& rhs) {
    return (lhs.duration > rhs.duration /* || 
            (lhs.duration == rhs.duration && lhs.span_count > rhs.span_count) */);
}

bool routing::operator<=(const EdgeWeight& lhs, const EdgeWeight& rhs) {
    return (lhs.duration <= rhs.duration);
}

bool routing::operator>=(const EdgeWeight& lhs, const EdgeWeight& rhs) {
    return (lhs.duration >= rhs.duration);
}

EdgeWeight EdgeWeight::operator+(EdgeWeight rhs) const {
    EdgeWeight new_weight (this->duration + rhs.duration, this->bus_ind + rhs.bus_ind, this->span_count);
    return new_weight;
}

EdgeWeight EdgeWeight::operator+(const EdgeWeight& rhs) {
    this->duration += rhs.duration; 
    this->bus_ind += rhs.bus_ind;
    // this->span_count += rhs.span_count;
    return *this;
}



TransportGraphMaker::TransportGraphMaker(const transport::TransportCatalogue& transport, RoutingSettings settings) 
: tc_(transport)
, settings_(std::move(settings)) 
, index_vs_buses_(transport.GetAllBuses()){
    // 0. Сформировать контейнер всех остановок, причем каждая остановка посторяется столько раз, сколько через нее проходит автобусов
    // 1. Сформировать контейнер ребер
    FillContainersWithStopsAndEdges(tc_.GetAllBuses());
    // 2. Создать и заполнить граф
    CreateAndFillGraph();
}

const TransportGraph& TransportGraphMaker::GetGraph() const {
    return *graph_;
}

std::optional<StopVertexes> TransportGraphMaker::GetStopIndexesByName(std::string_view stop_name) const {
    if (!stop_vs_indexes_.count(stop_name)) {
        return {};
    }
    return stop_vs_indexes_.at(stop_name);
}

std::string_view TransportGraphMaker::GetStopNameByIndex(size_t ind) const {
    /* if (ind > index_vs_stop_.size()) {
        throw std::out_of_range("LOG err: In TransportGraphMaker::GetStopNameByIndex - Index exceed graph size"s);
    } */
    return index_vs_stop_.at(ind);
}

const RoutingSettings& TransportGraphMaker::GetSettings() const {
    return settings_;
}

std::string_view TransportGraphMaker::GetBusNameByIndex(size_t ind) const {
    return index_vs_buses_.at(ind).first;
}

std::size_t TransportGraphMaker::GetBusIndexByName(std::string_view bus_name) const {
    return buses_vs_index_.at(bus_name);
}


// Добавляет остановку, если её еще нет в базе вершин и возвращает индексы соответствующих остановке узлов
const StopVertexes& TransportGraphMaker::AddStopAndGetIndexes(std::string_view stop_name) {
    // случай 1 - остановки ещё не в списке вершин
    if (!stop_vs_indexes_.count(stop_name)) {
        size_t n = index_vs_stop_.size();
        // Формируем новые индексы для вершин остановки
        StopVertexes new_stop_indexes;
        new_stop_indexes.depart_ind = n;
        new_stop_indexes.arrive_ind = n + 1;
        
        // добавляем в map 
        stop_vs_indexes_[stop_name] = (new_stop_indexes);
        // добавляем в vector остановок
        index_vs_stop_.push_back(stop_name);
        index_vs_stop_.push_back(stop_name);

        // добавляем ребро между кусками остановки from и to
        EdgeWeight one_stop_weight({static_cast<Duration>(settings_.bus_wait_time), -1, 0});

        // to do: скорее всего нужно оставить только вторую из этих двух строчек:
        // edges_.emplace_back(new_stop_indexes.depart_ind, new_stop_indexes.arrive_ind, one_stop_weight);
        edges_.emplace_back(new_stop_indexes.arrive_ind, new_stop_indexes.depart_ind, one_stop_weight);

        // проверяем адекватность добавления
        if (index_vs_stop_.size() != (n + 2)) {
            std::cout << "LOG err: in AddStopAndGetIndexes - Uncorrect Stop Addind"s << std::endl;
        }
    }

    return stop_vs_indexes_.at(stop_name);
}

void TransportGraphMaker::ParseAndFillLinearBusTrack(const domain::Bus* bus_ptr) {
    const StopsList& stops = bus_ptr->stops_on_route; 
    size_t n_stops = stops.size();
    // to do: возможно, нужно сдвигать итератор лишь на n/2 без доп +1
    const auto it_final_stop = std::next(stops.begin(), n_stops/2);
    
    // Добавляем ребра - возможные поездки на автобусе без пересадок 
    // от начала маршрута до конечной (середины) и от середины до конца
    AddRouteEdges(stops.begin(), std::next(it_final_stop), bus_ptr->name);
    AddRouteEdges(it_final_stop, stops.end(), bus_ptr->name);

    return;
}

void TransportGraphMaker::ParseAndFillRoundBusTrack(const domain::Bus* bus_ptr) {
    const StopsList& stops = bus_ptr->stops_on_route; 
    
    // Добавляем ребра - возможные поездки на автобусе без пересадок 
    // от начала маршрута до конечной
    AddRouteEdges(stops.begin(), stops.end(), bus_ptr->name);
    return;
}

// Формирует таблицу индексов остановок (узлов будущего графа) и хеш-таблицу (вектор) ребер
void TransportGraphMaker::FillContainersWithStopsAndEdges(const BusesList& buses) {
    // инициализируем начальные индексы и остановки - это необходимо для заполнения хеш-таблицы путей
    bus_velocity_in_m_per_minute_ = ConvertVelocity(settings_.bus_velocity);
    int cur_bus_ind = 0;

    for (const auto& [bus_name, bus_ptr] : buses) {
        // добавляем автобус в список, чтобы задать индекс
        if (buses_vs_index_.count(bus_name)) {
            std::cerr << "LOG err: in FillContainersWithStopsAndEdges - Can not add the Bus, that already exists"s << std::endl;
        }
        buses_vs_index_[bus_name] = cur_bus_ind;

        if (bus_ptr->is_round) {
            ParseAndFillRoundBusTrack(bus_ptr); 
        }
        else {
            ParseAndFillLinearBusTrack(bus_ptr);
        }

        // обязательно инкрементировать автобусный индекс 
        cur_bus_ind++;
    }

    stop_vertex_number_ = index_vs_stop_.size();

    return;
}

void TransportGraphMaker::PrintGraph() const {
    // вывод графа
    size_t n_edges = graph_->GetEdgeCount();
    for (size_t i = 0; i < n_edges; i++) {
        graph::Edge edge = graph_->GetEdge(i);
        std::cout << edge.from << "->" << edge.weight.duration << "min" << "->" << edge.to << std::endl;
    }
}


void TransportRouter::MoveWaitItemToList(WaitRouteItem& wait_item, TransportRouteItems& items, Duration& total_duration) const {
    total_duration += wait_item.duration;
    // засовываем в список активностей
    items.push_back(std::move(wait_item));
    // обнуляем на всякий случай
    wait_item.Clear();
}

void TransportRouter::MoveBusItemToList(BusRouteItem& bus_item, TransportRouteItems& items, Duration& total_duration) const {
    total_duration += bus_item.duration;
    items.push_back(std::move(bus_item));
    bus_item.Clear();
}

// Формируеn вектор items (непрерывных шагов/активностей) и считает полную длительность маршрута
std::pair<TransportRouteItems, Duration> TransportRouter::CreateTransportRouteFromGraphRoute(const GraphRouteInfo& route_info) const {
    TransportRouteItems items;
    Duration duration_total = 0;
    // Проверяем, есть ли маршрут в принципе
    if (route_info.edges.empty()) {
        return {TransportRouteItems(), 0};
    }
    // 

    // первое ребро - начало поездки
    graph::EdgeId first_edge_id = route_info.edges.at(0);
    // первая остановка
    size_t first_stop_ind = graph_maker_.GetGraph().GetEdge(first_edge_id).from;
    std::string_view first_stop = graph_maker_.GetStopNameByIndex(first_stop_ind);
    
    // добавляем ожидание первого транспорта в Items
    WaitRouteItem first_item(/*название первой остановки*/std::string(first_stop), static_cast<Duration>(graph_maker_.GetSettings().bus_wait_time));
    MoveWaitItemToList(first_item, items, duration_total);

    // идем по вектору id ребер и собираем информацию. 
    // Если у соседних ребер один автобус, то накапливаем 
    // и только после смены добавляем в вектор

    BusRouteItem bus_item;
    WaitRouteItem wait_item;

    for (const graph::EdgeId& edge_id : route_info.edges) {
        const graph::Edge<EdgeWeight>& edge_cur = graph_maker_.GetGraph().GetEdge(edge_id);
        std::string_view first_stop_name = graph_maker_.GetStopNameByIndex(edge_cur.from);
        std::string_view second_stop_name = graph_maker_.GetStopNameByIndex(edge_cur.to);
        int bus_ind = edge_cur.weight.bus_ind;

        if (bus_ind < 0) { // ребро ожидания
            // на всякий случай проверяем, что для ребра ожидания остановки from и to совпадают
            if (first_stop_name != second_stop_name) {
                throw std::logic_error("LOG err: in GetRoute Start and Stop name do not match for wait edge"s);
            }
            // Добавляем item ожидания
            wait_item.duration = edge_cur.weight.duration;
            wait_item.stop = std::string(first_stop_name);

            MoveWaitItemToList(wait_item, items, duration_total);
            wait_item.Clear();  // на всякий случай очищаем
        }
        else { // ребро поездки на автобусе 
            // на всякий случай проверим, что не осталось инфы о предыдущей поездке
            if ( !bus_item.Empty() ) {  // 
                throw std::logic_error("LOG err: in GetRoute Can\'t write new bus trip because the previous is not empty"s);
            }
            // формируем новую поездку
            bus_item.bus = graph_maker_.GetBusNameByIndex(static_cast<size_t>(bus_ind));
            bus_item.span_count = edge_cur.weight.span_count;
            bus_item.duration = edge_cur.weight.duration;
            
            // перемещаем её
            MoveBusItemToList(bus_item, items, duration_total);
            bus_item.Clear();

        }
    }

    // Записываем последнюю поездку
    if (!bus_item.Empty()) {
        MoveBusItemToList(bus_item, items, duration_total);
    }
    return {items, duration_total};
}


std::optional<std::pair<TransportRouteItems, Duration>> TransportRouter::GetRouteInfo(std::string_view from_stop, std::string_view to_stop) const {
    // 0. Определяем индексы остановок отправления и назначения
    std::optional<StopVertexes> from_stop_inds = graph_maker_.GetStopIndexesByName(from_stop);
    std::optional<StopVertexes> to_stop_inds = graph_maker_.GetStopIndexesByName(to_stop);
    if (!from_stop_inds || !to_stop_inds) {
        return{};
    }

    std::optional<GraphRouteInfo> fast_route;

    if (from_stop == to_stop) {
        EdgeWeight empty_weight;
        fast_route = {empty_weight, {}};
    }
    else {
        // 1. Находим самый быстрый маршрут
        fast_route = router_->BuildRoute((*from_stop_inds).depart_ind, (*to_stop_inds).arrive_ind);   
    }
    
    // проверяем, что маршрут найден
    if (!fast_route) {
        return {};
    }

    // 2. Формируем вектор items (непрерывных шагов/активностей)
    std::pair<TransportRouteItems, Duration> transport_route_info = CreateTransportRouteFromGraphRoute(fast_route.value());
    
    return transport_route_info;
}

