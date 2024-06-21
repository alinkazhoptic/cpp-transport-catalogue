#pragma once

#include "map_renderer.h"
#include "request_handler.h"
#include "json.h"
#include "json_builder.h"

#include <vector>
#include <sstream>
#include <string>

/*
 * Здесь можно разместить код наполнения транспортного справочника данными из JSON,
 * а также код обработки запросов к базе и формирование массива ответов в формате JSON
 */
namespace request_detail {

using namespace std::literals;

class CommandDescription {
public:
    virtual ~CommandDescription() = default;
    // Определяет, задана ли команда (поле command непустое)
    explicit operator bool() const {
        return !command_type_.empty();
    }

    bool operator!() const {
        return !operator bool();
    }

    virtual std::string GetCommandType() const {
        return command_type_;
    }

    virtual CommandDescription& SetCommandType(std::string type) {
        command_type_ = std::move(type);
        return *this;
    }

    std::string GetName() const {
        return name_;
    }

    CommandDescription& SetName(std::string name) {
        name_ = std::move(name);
        return *this;
    }

private:
    std::string command_type_;      // Название команды
    std::string name_;           // название маршрута или остановки
};


class StopCommand final : public CommandDescription {
public:

    std::string GetCommandType() const override {
        return command_type_;
    }

    geo::Coordinates GetCoordinates() const { 
        return coordinates_; 
    } 
    
    StopCommand& SetCoordinates(double latitude, double longitude) {
        coordinates_.lat = latitude;
        coordinates_.lng = longitude;
        return *this;
    }

    const std::vector<std::pair<std::string, int>>& GetDistances() const { return road_distances_; }

    StopCommand& SetDistances(std::vector<std::pair<std::string, int>> stop_and_distances) {
        road_distances_ = std::move(stop_and_distances);
        return *this;
    }

    StopCommand& AddDistance(std::string stop_name, int distance) {
        road_distances_.push_back(std::move(std::make_pair(stop_name, distance)));
        return *this;
    }

private:
    std::string command_type_ = "Stop"s;
    geo::Coordinates coordinates_;
    std::vector<std::pair<std::string, int>> road_distances_;
};


class BusCommand final : public CommandDescription {
public:
    std::string GetCommandType() const override {
        return command_type_;
    }

    bool IsRoundTrip() const {
        return is_round_;
    }


    const std::vector<std::string>& GetStops() const {
        return stops_;
    } 


    void SetStops(std::vector<std::string> stops) {
        stops_ = stops;
    } 


    void AddStop(std::string stop_name) {
        stops_.push_back(stop_name);
    }
    

    void SetRoundTrip() {
        is_round_ = true;
    }

    void SetStraightTrip() {
        is_round_ = false;
    }

private:
    bool is_round_ = false;
    std::string command_type_ = "Bus"s;
    // std::vector<std::string_view> stops_;
    std::vector<std::string> stops_;
};


struct RequestDescription {
    // Определяет, задана ли команда (поле command непустое)
    explicit operator bool() const {
        return !type.empty();
    }

    bool operator!() const {
        return !operator bool();
    }

    std::string type;      // Название команды
    std::string name;      // название маршрута или остановки
    int id;                // id (номер) запроса  
};

}  // namespace request_detail

class JsonReader {
public:
    JsonReader() = default;

    // Загружает Json в данный класс
    void LoadJson(std::istream& input);

    // Выводит JSON c ответами
    void PrintResponse(std::ostream& output) const;

    /**
     * Наполняет данными транспортный справочник, используя команды из commands_
    */
    void ApplyCommands(transport::TransportCatalogue& catalogue);

    // Задает параметры отрисовки
    void ApplyRenderSettings(renderer::MapRenderer& renderer) const;

    /**
     * Отправляет запросы к транспортному каталогу, принимает ответы и формирует json::Document 
    */
    // const json::Document& ProcessRequestsAndGetResponse(transport::TransportCatalogue& catalogue);
    const json::Document& ProcessRequestsAndGetResponse(RequestHandler& request_handler);



private:
    json::Document document_with_requests_ = json::Document(json::Node());
    json::Document response_document_ = json::Document(json::Node());
    
    std::vector<request_detail::StopCommand> stop_commands_;
    std::vector<request_detail::BusCommand> bus_commands_;
    std::vector<request_detail::RequestDescription> stat_requests_;

    // Читает JSON из потока
    json::Document ReadJson(std::istream& input) const;

    void AddStopsToCatalogue(transport::TransportCatalogue& catalogue) const;
    void AddBusesToCatalogue(transport::TransportCatalogue& catalogue) const;

    // Добавляет запрос типа base_request в базу к stop_requests или bus_requests 
    void AddRequestToCommands(const json::Dict& request_map);
    // Добавляет запрос типа stat_request в список запросов к базе данных requests_
    void AddRequestToRequests(const json::Dict& request_map);

    // Формирует списки команд на заполнение базы данных и запросов к ней 
    void FormAllRequestsData(const json::Document& document);
    
    // Обрабатывает один запрос и возвращает словарь данных ответа на запрос
    json::Dict ProcessOneRequest(RequestHandler& request_handler, const request_detail::RequestDescription& request) const;

    // Обрабатывает один запрос типа Map и возвращает словарь данных ответа на запрос
    json::Dict ProcessMapRequest(RequestHandler& request_handler, const request_detail::RequestDescription& request) const;

};