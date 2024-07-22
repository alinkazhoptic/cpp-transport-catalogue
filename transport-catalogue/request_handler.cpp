#include "request_handler.h"

/*
 * Здесь можно было бы разместить код обработчика запросов к базе, содержащего логику, которую не
 * хотелось бы помещать ни в transport_catalogue, ни в json reader.
 *
 * Если вы затрудняетесь выбрать, что можно было бы поместить в этот файл,
 * можете оставить его пустым.
 */

// Конструктор по каталогу и отрисовщику
RequestHandler::RequestHandler(const transport::TransportCatalogue& db, renderer::MapRenderer& renderer)
    :db_(db), map_renderer_(renderer) {}

/* // Конструктор только по каталогу - при наличии рендера пришлось исключить
RequestHandler::RequestHandler(const transport::TransportCatalogue& db)
    :db_(db) {} */

// Возвращает информацию о маршруте (запрос Bus)
std::optional<domain::BusInfo> RequestHandler::GetBusStat(const std::string_view& bus_name) const {
    return db_.GetBusInfo(bus_name);
}

// Возвращает маршруты, проходящие через остановку (запрос Stop)
// const std::unordered_set<BusPtr>* GetBusesByStop(const std::string_view& stop_name) const;
std::optional<domain::StopInfo> RequestHandler::GetBusesByStop(const std::string_view& stop_name) const {
    return db_.GetStopInfo(stop_name);
}

/*
Возвращает вектор всех автобусов в сортированном порядке
*/
std::vector<std::pair<std::string_view, const domain::Bus*>> RequestHandler::GetAllBusesForMap() const {
    return db_.GetAllBuses();
}

std::vector<const domain::Stop*> RequestHandler::GetAllStopsForMap() const {
    return db_.GetAllStopsBusPassingThrough();
}

void RequestHandler::RenderMap(std::ostream& ouput_stream) {
    std::vector<const domain::Stop*> stops_to_draw = GetAllStopsForMap();
    std::vector<std::pair<std::string_view, const domain::Bus*>> routes_to_draw = GetAllBusesForMap();
    // Формируем документ
    map_renderer_.DrawMap(stops_to_draw, routes_to_draw);
    // Отрисовываем документ
    map_renderer_.Render(ouput_stream);
}

renderer::MapRenderer& RequestHandler::GetMaprenderer() {
    return map_renderer_;
}

const transport::TransportCatalogue& RequestHandler::GetTransportCatalogue() const {
    return db_;
}

std::optional<int> RequestHandler::GetDistanceBetweenAdjacentStops(std::string_view stop_A_name, std::string_view stop_B_name) const {
    return db_.GetDistanceBetweenStops(stop_A_name, stop_B_name);
}