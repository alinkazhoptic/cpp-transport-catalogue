#pragma once

#include "transport_catalogue.h"
#include "graph.h"
#include "router.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>


namespace routing {

const double METERS_IN_KM = 1000;
const double MINUTES_IN_HOUR = 60;

using Duration = double;

struct EdgeWeight {
    Duration duration = 0;
    int bus_ind = 0;  // для ребра ожидания на остановке bus_ind = -1
    int span_count = 0;
    
    EdgeWeight() = default;

    EdgeWeight(Duration time, int bus_index, int span_number)
    : duration(std::move(time))
    , bus_ind(std::move(bus_index))
    , span_count(std::move(span_number)) {}
    
    EdgeWeight operator+(EdgeWeight rhs) const;

    EdgeWeight operator+(const EdgeWeight& rhs);
};

bool operator<(const EdgeWeight& lhs, const EdgeWeight& rhs); 
bool operator>(const EdgeWeight& lhs, const EdgeWeight& rhs); 
bool operator<=(const EdgeWeight& lhs, const EdgeWeight& rhs);
bool operator>=(const EdgeWeight& lhs, const EdgeWeight& rhs);


using BusesList = std::vector<std::pair<std::string_view, const domain::Bus*>>;
using StopsList = std::vector<const domain::Stop*>;
using TransportGraph = graph::DirectedWeightedGraph<EdgeWeight>;


struct RoutingSettings {
    size_t bus_wait_time = 0;
    double bus_velocity = 1.0;
};


struct StopVertexes {
    size_t depart_ind = 0; // индекс узла, из которого можно уехать
    size_t arrive_ind = 0; // индекс узла, в который можно приехать

    StopVertexes() = default;

    bool empty() {
        return (depart_ind == 0) && (arrive_ind == 0);
    }

    bool operator ==(const StopVertexes& other) {
        return (this->depart_ind == other.depart_ind && this->arrive_ind == other.arrive_ind);
    }
};


class TransportGraphMaker {
public:
    // конструктор 
    TransportGraphMaker(const transport::TransportCatalogue& transport, RoutingSettings settings);

    TransportGraphMaker(const TransportGraphMaker& other) = delete;
    TransportGraphMaker& operator=(const TransportGraphMaker& other) = delete;

    TransportGraphMaker(TransportGraphMaker&& other) = default;
    TransportGraphMaker& operator=(TransportGraphMaker&& other) = default;

    
    const TransportGraph& GetGraph() const;

    std::optional<StopVertexes> GetStopIndexesByName(std::string_view stop_name) const;
    std::string_view GetStopNameByIndex(size_t ind) const;

    const RoutingSettings& GetSettings() const;

    std::string_view GetBusNameByIndex(size_t ind) const;
    std::size_t GetBusIndexByName(std::string_view bus_name) const;


private:
    const transport::TransportCatalogue& tc_;
    RoutingSettings settings_;

    std::unique_ptr<TransportGraph> graph_;

    BusesList index_vs_buses_;
    std::unordered_map<std::string_view, int> buses_vs_index_;

    // хранение остановок и их узлов, 
    // в паре соответственно: first - узел отправления, second - узел прибытия
    std::unordered_map<std::string_view, StopVertexes> stop_vs_indexes_; 
    std::vector<std::string_view> index_vs_stop_;
    size_t stop_vertex_number_;
    std::vector<graph::Edge<EdgeWeight>> edges_;

    double bus_velocity_in_m_per_minute_ = 0.0;

    // формирует граф по каталогу
    void CreateAndFillGraph()  {
        using namespace graph;
        // 1. Инициализировать граф
        std::unique_ptr<TransportGraph> graph_tmp = std::make_unique<TransportGraph>(stop_vertex_number_);
        // 2. Заполнить граф ребрами
        for (const Edge<EdgeWeight>& edge_cur : edges_) {
            graph_tmp->AddEdge(edge_cur);
        }
        graph_ = std::move(graph_tmp);

        // PrintGraph();

        return;
    }

    double ConvertVelocity(double velocity_in_km_per_hour) {
        return velocity_in_km_per_hour * METERS_IN_KM / MINUTES_IN_HOUR;
    }


    const StopVertexes& AddStopAndGetIndexes(std::string_view stop_name);

    // Формирует по списку остановок узлы графа - маршруты между остановками 
    template<typename Iterator>
    void AddRouteEdges(Iterator it_start, Iterator it_end, std::string_view bus_name) {
        for (auto it_from = it_start; it_from != it_end; it_from++) {
            // добавляем остановку (если она новая) и получаем индексы её вершин в будущем графе
            const StopVertexes& stop_from_vertexes = AddStopAndGetIndexes((*it_from)->name);
            int distance = 0;
            int span_count = 0;
            std::string_view prev_to_stop_name = (*it_from)->name;
    
            for (auto it_to = std::next(it_from); it_to != it_end; it_to++) {
                std::string_view cur_to_stop_name = (*it_to)->name;

                // добавляем остановку (если она новая) и получаем индексы её вершин в будущем графе
                const StopVertexes& stop_to_vertexes = AddStopAndGetIndexes(cur_to_stop_name);
                
                // Накапливаем расстояние, удаляясь от остановки отправления
                // Тут подразумеваем, что расстояние однозначно определено, 
                // так как остановки берутся из автобусной инфы
                distance += tc_.GetDistanceBetweenStops(prev_to_stop_name, cur_to_stop_name).value();
                // Считаем время в пути
                Duration duration = (distance * 1.0) / bus_velocity_in_m_per_minute_;  // * 1.0 для приведения к double
                span_count += 1;
                std::size_t cur_bus_ind = GetBusIndexByName(bus_name);
                EdgeWeight cur_weight({duration, static_cast<int>(cur_bus_ind), span_count});
                
                // Формируем и добавляем новое ребро: 
                // от остановки из внешнего цикла до текущей остановки из внутреннего цикла
                edges_.emplace_back(stop_from_vertexes.depart_ind, stop_to_vertexes.arrive_ind, cur_weight);

                // обновляем предыдущую остановку для следующей итерации
                prev_to_stop_name = cur_to_stop_name;
            }
        }
        
        return;
    }

    void ParseAndFillLinearBusTrack(const domain::Bus* bus_ptr);
    void ParseAndFillRoundBusTrack(const domain::Bus* bus_ptr);

    // Формирует таблицу индексов остановок (узлов будущего графа) и хеш-таблицу (вектор) ребер
    void FillContainersWithStopsAndEdges(const BusesList& buses); 
    
    void PrintGraph() const;
};


struct BusRouteItem {
    const std::string type = "Bus";
    std::string bus;
    int span_count = 0;
    Duration duration = 0;
    
    BusRouteItem() = default;

    BusRouteItem(std::string bus_name, int n_stops, Duration time) 
    : bus(std::move(bus_name))
    , span_count(n_stops)
    , duration(time) {}

    void Clear() {
        bus.clear();
        span_count = 0;
        duration = 0;
    }

    bool Empty() {
        return (bus.empty() && span_count == 0 && duration == 0);
    }
};

struct WaitRouteItem {
    const std::string type = "Wait"; 
    std::string stop;
    Duration duration = 0;

    WaitRouteItem() = default;

    WaitRouteItem(std::string stop_name, Duration time)
    : stop(std::move(stop_name))
    , duration(time) {}

    void Clear() {
        stop.clear();
        duration = 0;
    }
    bool Empty(){
        return (stop.empty() && duration == 0);
    }

};

using TransportRouteItems = std::vector<std::variant<std::monostate, WaitRouteItem, BusRouteItem>>;
using GraphRouteInfo = graph::Router<EdgeWeight>::RouteInfo;


class TransportRouter {
private:
    TransportGraphMaker graph_maker_;
    std::unique_ptr<graph::Router<EdgeWeight>> router_;
    
public:
    TransportRouter(TransportGraphMaker&& graph_maker) 
        : graph_maker_(std::move(graph_maker)) {
        router_ = std::make_unique<graph::Router<EdgeWeight>>(graph_maker_.GetGraph());
    }

    std::optional<std::pair<TransportRouteItems, Duration>> GetRouteInfo(std::string_view from_stop, std::string_view to_stop) const;

private:

    std::optional<GraphRouteInfo> FindFasterRoute(const std::vector<size_t>& from_stop_inds, const std::vector<size_t>& to_stop_inds) const {
        std::optional<GraphRouteInfo> fast_route;
        for (size_t from_ind : from_stop_inds) {
            for (size_t to_ind : to_stop_inds) {
                const auto& cur_route = router_->BuildRoute(from_ind, to_ind);
                if (!cur_route.has_value()) {
                    continue;
                }
                if (!fast_route.has_value() || (cur_route.value().weight < fast_route.value().weight)) {
                    fast_route = std::move(cur_route);
                }
            }
        }
        return fast_route;
    }

    void MoveWaitItemToList(WaitRouteItem& wait_item, TransportRouteItems& items, Duration& total_duration) const;

    void MoveBusItemToList(BusRouteItem& bus_item, TransportRouteItems& items, Duration& total_duration) const;

    // Формируеn вектор items (непрерывных шагов/активностей) и считает полную длительность маршрута
    std::pair<TransportRouteItems, Duration> CreateTransportRouteFromGraphRoute(const GraphRouteInfo& route_info) const;

};


}  // namespace routing