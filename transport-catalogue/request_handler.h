#pragma once

#include "transport_catalogue.h"
#include "map_renderer.h"

/*
 * Здесь можно было бы разместить код обработчика запросов к базе, содержащего логику, которую не
 * хотелось бы помещать ни в transport_catalogue, ни в json reader.
 *
 * В качестве источника для идей предлагаем взглянуть на нашу версию обработчика запросов.
 * Вы можете реализовать обработку запросов способом, который удобнее вам.
 *
 * Если вы затрудняетесь выбрать, что можно было бы поместить в этот файл,
 * можете оставить его пустым.
 */

// Класс RequestHandler играет роль Фасада, упрощающего взаимодействие JSON reader-а
// с другими подсистемами приложения.
// См. паттерн проектирования Фасад: https://ru.wikipedia.org/wiki/Фасад_(шаблон_проектирования)


class RequestHandler {
public:
    // MapRenderer понадобится в следующей части итогового проекта
    RequestHandler(const transport::TransportCatalogue& db, renderer::MapRenderer& renderer);
    // RequestHandler(const transport::TransportCatalogue& db);

    // Возвращает информацию о маршруте (запрос Bus)
    std::optional<domain::BusInfo> GetBusStat(const std::string_view& bus_name) const;

    // Возвращает маршруты, проходящие через остановку (запрос Stop)
    // const std::unordered_set<BusPtr>* GetBusesByStop(const std::string_view& stop_name) const;
    std::optional<domain::StopInfo> GetBusesByStop(const std::string_view& stop_name) const;

    /*
    Возвращает вектор всех маршрутов с данными об остановках в сортированном порядке по названию
    */
    std::vector<std::pair<std::string_view, const domain::Bus*>> GetAllBusesForMap() const;

    /*
    Возвращает вектор всех остановок, имеющихся в каталоге
    */
    std::vector<const domain::Stop*> GetAllStopsForMap() const;

    // Этот метод будет нужен в следующей части итогового проекта
    void RenderMap(std::ostream& ouput_stream);

    renderer::MapRenderer& GetMaprenderer();

    const transport::TransportCatalogue& GetTransportCatalogue() const;

    std::optional<int> GetDistanceBetweenAdjacentStops(std::string_view stop_A_name, std::string_view stop_B_name) const;



private:
    // RequestHandler использует агрегацию объектов "Транспортный Справочник" и "Визуализатор Карты"
    const transport::TransportCatalogue& db_;
    renderer::MapRenderer& map_renderer_;

};
