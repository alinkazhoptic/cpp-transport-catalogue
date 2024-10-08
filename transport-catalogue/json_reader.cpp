#include "json_reader.h"

#include <iostream>
#include <algorithm>

/*
 * Здесь размещен код наполнения транспортного справочника данными из JSON,
 * а также код обработки запросов к базе и формирование массива ответов в формате JSON
 */

using namespace request_detail;
using namespace std::literals;

// Читает JSON из потока
json::Document JsonReader::ReadJson(std::istream& input) const {
    return json::Load(input);
}

// Загружает Json в данный класс
void JsonReader::LoadJson(std::istream& input) {
    document_with_requests_ = std::move(ReadJson(input));
}


/**
 * Удаляет пробелы в начале и конце строки
 */
static std::string_view Trim(std::string_view string) {
    const auto start = string.find_first_not_of(' ');
    if (start == string.npos) {
        return {};
    }
    return string.substr(start, string.find_last_not_of(' ') + 1 - start);
}


// Преобразует узел json-документа в цвет
// Необходима, так как цвет может быть записан в json разными способами: строкой, в координатах RBG или RGBa
static svg::Color GetColorFromJsonNode(const json::Node& color_node) {
    if (color_node.IsArray()) {
        json::Array color_array = color_node.AsArray();
        if (color_array.size() == 3) {
            svg::Rgb rgb_color = svg::Rgb{static_cast<uint8_t>(color_array[0].AsInt()), 
                                        static_cast<uint8_t>(color_array[1].AsInt()), 
                                        static_cast<uint8_t>(color_array[2].AsInt())};
            return rgb_color; 
        }
        else if (color_array.size() == 4) {
            svg::Rgba rgba_color = svg::Rgba{static_cast<uint8_t>(color_array.at(0).AsInt()), 
                                        static_cast<uint8_t>(color_array.at(1).AsInt()), 
                                        static_cast<uint8_t>(color_array.at(2).AsInt()),
                                        color_array.at(3).AsDouble()};
            return rgba_color;
        }
        // to do:
        else {
            std::cerr << "LOG err: in GetColorFromJsonNode Uncorrect color array in json"sv << std::endl;
        }
    }
    else if (color_node.IsString()) {
        return svg::Color(color_node.AsString());
    }
    else {
        std::cerr << "LOG err: in GetColorFromJsonNode Uncorrect color node in json"sv << std::endl;
    }
    // если сюда дошли, то значит была ошибка чтения цвета, вернется пустой цвет "none"
    return svg::NoneColor; 
}


// Вспомогательная функция для обнаружения параметров в settings map json и записи их в структуру, 
// Предназначена для сокращения функции GetRenderSettingsFromDocument
static renderer::RenderingFormatOptions ParseRenderingParameters(const json::Dict& settings_map) {
    renderer::RenderingFormatOptions render_options;
    if (settings_map.count("width")) {
        render_options.picture_size_.width = settings_map.at("width").AsDouble();       
    }
    if (settings_map.count("height")) {
        render_options.picture_size_.height = settings_map.at("height").AsDouble();       
    }
    if (settings_map.count("padding")) {
        render_options.padding = settings_map.at("padding").AsDouble();       
    }
    if (settings_map.count("line_width")) {
        render_options.line_width_ = settings_map.at("line_width").AsDouble();       
    }
    if (settings_map.count("stop_radius")) {
        render_options.stop_radius_ = settings_map.at("stop_radius").AsDouble();       
    }
    if (settings_map.count("bus_label_font_size")) {
        render_options.bus_label_font_size_ = settings_map.at("bus_label_font_size").AsInt();       
    }
    if (settings_map.count("bus_label_offset")) {
        svg::Point p;
        p.x = settings_map.at("bus_label_offset").AsArray().at(0).AsDouble();
        p.y = settings_map.at("bus_label_offset").AsArray().at(1).AsDouble();
        render_options.bus_label_offset = p;       
    }
    if (settings_map.count("stop_label_font_size")) {
        render_options.stop_label_font_size_ = settings_map.at("stop_label_font_size").AsInt();       
    }
    if (settings_map.count("stop_label_offset")) {
        svg::Point p;
        p.x = settings_map.at("stop_label_offset").AsArray().at(0).AsDouble();
        p.y = settings_map.at("stop_label_offset").AsArray().at(1).AsDouble();
        render_options.stop_label_offset_ = p;       
    }
    if (settings_map.count("underlayer_color")) {
        json::Node color_node = settings_map.at("underlayer_color");
        svg::Color color = GetColorFromJsonNode(color_node);
        
        render_options.underlayer_color_ = color;       
    }
    if (settings_map.count("underlayer_width")) {
        render_options.underlayer_width_ = settings_map.at("underlayer_width").AsDouble();       
    }
    if (settings_map.count("color_palette")) {
        if (!settings_map.at("color_palette").IsArray()) {
            // ошибка, должен быть непустой массив
            std::cerr << "LOG err: in ParseRenderingParameters color palette is not an array!"sv << std::endl;
        }
        json::Array colors_json_array = settings_map.at("color_palette").AsArray();
        std::vector<svg::Color> color_vector;
        for (const json::Node& color_node :  colors_json_array) {
            color_vector.push_back( GetColorFromJsonNode(color_node) );
        } 
        render_options.color_palete_ = std::move(color_vector);       
    }

    return render_options;
}


// Общая функция для формирования структуры настроек отрисовки
static renderer::RenderingFormatOptions GetRenderSettingsFromDocument(const json::Document& document) {
    // Создаем структуру, которую будем заполнять значениями из документа
    // По умолчанию она заполнена некоторыми адекватными значениями
    renderer::RenderingFormatOptions render_options;
    try {
        // 0. Получаем map запросов
        if (!document.GetRoot().IsDict()) {
            std::cerr << "LOG err: in GetRenderSettingsFromDocument document.GetRoot() is not a map! \n Default render options is be used!"sv << std::endl;
            return render_options;
        }
        const json::Dict& main_node = document.GetRoot().AsDict();
        
        // 1. Парсим запросы "render_settings"
        const json::Node& settings_node = main_node.at("render_settings");
        
        // Проверяем, что render_settings правильно считалась как map
        if (!settings_node.IsDict()) {
            std::cerr << "There is no render_settings" << std::endl;
        }
        // 2. Получаем словарь параметров
        const json::Dict& settings_map = settings_node.AsDict(); 

        // 3. Поэлементно добавляем в структуру параметров параметры из json-документа
        render_options = ParseRenderingParameters(settings_map); 

    }
    catch (std::logic_error& e) {
        std::cerr << "LOG err: from json_reader GetRenderSettingsFromDocument: main_node is not a map => "s << e.what() << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << "LOG err: from json_reader GetRenderSettingsFromDocument: "s << e.what() << std::endl;
    }

    return render_options;
}


// Обработка запросов на добавление остановок
// Идем по списку команд и обрабатывааем только запросы на добавление остановок
void JsonReader::AddStopsToCatalogue(transport::TransportCatalogue& catalogue) const {
    for(const auto& command_cur : stop_commands_) {
        // если запрос не про остановку, то переходим к следующему запросу
        if (Trim(command_cur.GetCommandType()) != "Stop"s) {
            continue;
        }
        
        // формируем остановку
        domain::Stop stop_cur;
        stop_cur.name = command_cur.GetName();
        stop_cur.coordinates = command_cur.GetCoordinates(); 
        // добавляем в каталог
        catalogue.AddStop(stop_cur, command_cur.GetDistances());  
    }
    return;
}

// Обработка запросов на добавление маршрутов автобусов
// идем по списку команд и обрабатываем только добавление автобусов
void JsonReader::AddBusesToCatalogue(transport::TransportCatalogue& catalogue) const {
        // 1. Обработка запросов на добавление маршрутов автобусов
    for(const BusCommand& command_cur : bus_commands_) {
        // если запрос не про остановку, то переходим к следующему запросу
        if (Trim(command_cur.GetCommandType()) != "Bus"s) {
            continue;
        }
        // Формируем вектор остановок с учетом типа маршрута
        // немного криво, потому что надо преобразовать тип элементов массива из string в string_view  
        std::vector<std::string_view> stops_names_on_route;
        for (const auto& stop_name : command_cur.GetStops()) {
            stops_names_on_route.emplace_back(stop_name);
        }

        // std::vector<std::string> stops_names_on_route = command_cur.GetStops();
        if (!command_cur.IsRoundTrip()) {
            stops_names_on_route.insert(stops_names_on_route.end(), std::next(stops_names_on_route.rbegin()), stops_names_on_route.rend());
        }
        // добавляем в каталог 
        catalogue.AddBus(command_cur.GetName(), stops_names_on_route, command_cur.IsRoundTrip());
    }
    return;
}


void JsonReader::ApplyCommands(transport::TransportCatalogue& catalogue) {
    // Реализуйте метод самостоятельно
    // Считываем команды из документа 
    FormAllRequestsData(document_with_requests_);

    // 0. Обработка запросов на добавление остановок
    AddStopsToCatalogue(catalogue);

    // 1. Обработка запросов на добавление маршрутов автобусов
    AddBusesToCatalogue(catalogue);

    return;

}

void JsonReader::ApplyRenderSettings(renderer::MapRenderer& renderer) const {
    // Формируем структуру
    renderer::RenderingFormatOptions parameters = GetRenderSettingsFromDocument(document_with_requests_);
    // Задаем/передаем параметры отрисовщику 
    renderer.SetFormatOptions(parameters);

}


static StopCommand FormStopCommand(const json::Dict& request_map) {
    if (request_map.at("type").AsString() != "Stop"s) {
        throw std::logic_error("LOG err: in FormStopCommand command type is not \"Stop\" => command_type : "s + request_map.at("type").AsString() );
    }
    // инициируем новую команду на добавление остановки
    StopCommand stop_command;
    // Записываем имя
    stop_command.SetName(request_map.at("name").AsString());
    // Вытаскиваем и записываем координаты
    stop_command.SetCoordinates(request_map.at("latitude").AsDouble(), request_map.at("longitude").AsDouble());
    // Вытаскиваем и записываем расстояния
    const json::Dict& distances_in_request = request_map.at("road_distances").AsDict();
    for (const auto& [stop, distance] : distances_in_request) {
        stop_command.AddDistance(stop, distance.AsInt());
    }

    return stop_command;
}


static BusCommand FormBusCommand(const json::Dict& request_map) {
    // инициируем новую команду на добавление маршрута
    BusCommand bus_command;
    // Записываем имя
    bus_command.SetName(request_map.at("name").AsString());
    // Записываем тип маршрута
    request_map.at("is_roundtrip").AsBool() ? bus_command.SetRoundTrip() : bus_command.SetStraightTrip();
    // Вытаскиваем список остановок и записываем
    const json::Array& stops_array = request_map.at("stops").AsArray();

    for (const auto& stop_node : stops_array) {
        std::string stop_name_tmp = stop_node.AsString();
        bus_command.AddStop(stop_name_tmp);
    }

    return bus_command;
}

// Добавляет запрос типа base_requests в список комманд stop_commands_ или bus_commands_
void JsonReader::AddRequestToCommands(const json::Dict& request_map) {
    // считываем тип запроса
    const std::string& request_type = request_map.at("type").AsString();
    if (request_type == "Stop"s) {
        // инициируем новую команду на добавление остановки
        StopCommand stop_command = FormStopCommand(request_map);
        // Добавляем сформированную команду в перечень команд на задание остановки
        stop_commands_.push_back(stop_command);
        // Переходим к следующему запросу
    }
    else if (request_type == "Bus"s) {
        // инициируем новую команду на добавление маршрута
        BusCommand bus_command = FormBusCommand(request_map);

        // Добавляем сформированную комманду в перечень команд на задание маршрута
        bus_commands_.push_back(bus_command);
        // Переходим к следующему запросу
    }
    else {
        throw std::logic_error("LOG err: in FormAllRequestsData -> Unknown command type"s);
    }
}

// Добавляет запрос типа stat_request в список запросов к базе данных requests_
void JsonReader::AddRequestToRequests(const json::Dict& request_map) {
    RequestDescription request;
    request.id = request_map.at("id").AsInt();
    if (request_map.count("name")){
        request.name = request_map.at("name").AsString();
    }
    request.type = request_map.at("type").AsString();
    // добавление инфы для запроса маршрута
    if (request.type == "Route"s) {
        request.route_from_stop = request_map.at("from").AsString();
        request.route_to_stop = request_map.at("to").AsString();
    }
    stat_requests_.push_back(std::move(request));
}


// Формирует списки команд на заполнение базы данных и запросов к ней 
void JsonReader::FormAllRequestsData(const json::Document& document) {
    try {
        // Проверяем, что запросы это map
        if (!document.GetRoot().IsDict()) {
            return;
        }
        // 0. Получаем map запросов
        const json::Dict& main_node = document.GetRoot().AsDict();
        
        // 1. Парсим запросы "base_requests"
        const json::Node& base_requests_node = main_node.at("base_requests");
        
        if (base_requests_node.IsArray()) {
            // Обрабатываем набор запросов
            const json::Array& base_request_array = base_requests_node.AsArray();
            for (const auto& request : base_request_array) {
                // вытаскиваем данные текущей команды
                const json::Dict& request_map = request.AsDict();
                AddRequestToCommands(request_map);
            }
        }
        else if (base_requests_node.IsDict()) {
            // Обрабатываем один запрос
            const json::Dict& request_map = base_requests_node.AsDict();
            AddRequestToCommands(request_map);
        }
        // to do: закомментировать следующий else
        else {
            std::cerr << "There is no base_requests" << std::endl;
        }

        // 2. Парсим запросы "stat_requests"
        const json::Node& stat_requests_node = main_node.at("stat_requests");

        // Когда запросов несколько, добавляем по одному
        if (stat_requests_node.IsArray()) {
            const json::Array& stat_requests_array = stat_requests_node.AsArray();
            for (const auto& request : stat_requests_array) {
                // вытаскиваем данные текущей команды
                const json::Dict& stat_request_map = request.AsDict();
                AddRequestToRequests(stat_request_map);
            }
        }
        // Когда запрос один
        else if (stat_requests_node.IsDict()) {
            const json::Dict& stat_request_map = stat_requests_node.AsDict();
            AddRequestToRequests(stat_request_map);
        }
        // to do: закомментировать следующий else
        else {
            std::cerr << "There is no stat_requests" << std::endl;
        }
    }
    catch (std::logic_error& e) {
        std::cerr << "LOG err: from json_reader FormAllRequestsData: main_node is not a map => "s << e.what() << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << "LOG err: from json_reader FormAllRequestsData: "s << e.what() << std::endl;
    }
// В результате заполнена база запросов
}

// Выводит JSON c ответами
void JsonReader::PrintResponse(std::ostream& output) const {
    // Проверка, что ответ есть
    if (response_document_.GetRoot().IsNull()) {
        std::cerr << "LOG err: in JsonReader::PrintResponse response is empty"s << std::endl;
        return;
    }
    json::Print(response_document_, output);
    return;
}

// Обрабатывает один запрос типа Stop и возвращает словарь данных ответа на запрос
static json::Dict ProcessStopRequest(const RequestHandler& request_handler, const RequestDescription& request) {
    if (request.type != "Stop") {
        throw std::invalid_argument("Request is not about Stop"s);
    }
    // инициализируем map для ответа на текущий запрос
    // json::Dict response_map({{"request_id"s, json::Node(request.id)}});
    json::Dict response_map;

    std::optional<domain::StopInfo> stop_stat = request_handler.GetBusesByStop(*request.name);
    
    // случай 1) Остановка найдена:
    if (stop_stat.has_value()) {
        // получаем автобусы и записываем их в map
        json::Array buses_array;

        // цикл нужен, чтобы привести к string, т.к. в Node используется тип string
        for (const auto& bus_name : stop_stat.value().buses_list) {
            buses_array.emplace_back(json::Node(std::string(bus_name)));
        }
    
        // Инициируем map и сразу с помощью Builder добавляем данные по запросу и автобусам
        response_map = json::Builder{}
                                .StartDict()
                                    .Key("request_id"s).Value(request.id)
                                    .Key("buses"s).Value(buses_array)
                                .EndDict()
                                .Build()
                                .AsDict();
    }
    // случай 2) Остановка не найдена:
    else {
        response_map = json::Builder{}
                                .StartDict()
                                    .Key("request_id"s).Value(request.id)
                                    .Key("error_message"s).Value("not found"s)
                                .EndDict()
                                .Build()
                                .AsDict();

    }

    return response_map;
}


// Обрабатывает один запрос типа Bus и возвращает словарь данных ответа на запрос
static json::Dict ProcessBusRequest(const RequestHandler& request_handler, const RequestDescription& request) {
    if (request.type != "Bus") {
        throw std::invalid_argument("Request is not about Bus"s);
    }
    // инициализируем map для ответа на текущий запрос
    json::Dict response_map;

    std::optional<domain::BusInfo> bus_stat = request_handler.GetBusStat(*request.name);
    
    // случай 1) Автобус найден в каталоге:
    if (bus_stat.has_value()) {
        // считаем кривизну
        double curvature = bus_stat.value().roads_route_length / bus_stat.value().geo_route_length;
        // получаем параметры и записываем их в map-у
        response_map = json::Builder{}
                                .StartDict()
                                    .Key("request_id"s).Value(request.id)
                                    .Key("route_length"s).Value(bus_stat.value().roads_route_length)
                                    .Key("curvature"s).Value(curvature)
                                    .Key("stop_count"s).Value(bus_stat.value().num_of_stops_on_route)
                                    .Key("unique_stop_count"s).Value(bus_stat.value().num_of_unique_stops)
                                .EndDict()
                                .Build()
                                .AsDict();
    }
    // случай 2) Остановка не найдена:
    else {
        // response_map.insert({"error_message"s, json::Node("not found"s)});
        response_map = json::Builder{}
                                .StartDict()
                                    .Key("request_id"s).Value(request.id)
                                    .Key("error_message"s).Value("not found"s)
                                .EndDict()
                                .Build()
                                .AsDict();
    }

    return response_map;
}


// Обрабатывает один запрос типа Map и возвращает словарь данных ответа на запрос
json::Dict JsonReader::ProcessMapRequest(RequestHandler& request_handler, const request_detail::RequestDescription& request) const {
    if (request.type != "Map") {
        throw std::invalid_argument("Request is not a Map"s);
    }
    // инициализируем map для ответа на текущий запрос
    // json::Dict response_map({{"request_id"s, json::Node(request.id)}});
    json::Dict response_map;
    
    // Применяем параметры отображения svg
    ApplyRenderSettings(request_handler.GetMaprenderer());

    // "Рисуем" карту в строковый поток и вычленяем строку 
    std::ostringstream svg_stream;
    request_handler.RenderMap(svg_stream);
    std::string svg_text = svg_stream.str();

    response_map = json::Builder{}
                            .StartDict()
                                .Key("request_id"s).Value(request.id)
                                .Key("map"s).Value(svg_text)
                            .EndDict()
                            .Build()
                            .AsDict();

    return response_map;
}

// Преобразует данные маршрута в формат json::Array, 
// чтобы потом его можно было запихнуть в словарь-ответа на запрос 
json::Array TransformRouteItemsToJsonArray(const routing::TransportRouteItems& items) {
    using namespace routing;
    json::Array json_items;

    for (size_t i = 0; i < items.size(); i++) {
        json::Node node_tmp;
        // шаг 1 - ожидание на остановке
        if (std::holds_alternative<WaitRouteItem>(items[i])) {
            // получаем кусок пути
            WaitRouteItem wait_item = std::get<WaitRouteItem>(items[i]);
            node_tmp = json::Builder{}.StartDict()
                                            .Key("type"s).Value(wait_item.type)
                                            .Key("time"s).Value(wait_item.duration)
                                            .Key("stop_name"s).Value(wait_item.stop)
                                        .EndDict()
                                        .Build();

        }
        // шаг 2 - поездка на автобусе 
        else if (std::holds_alternative<BusRouteItem>(items[i])) {
            // получаем кусок пути
            BusRouteItem bus_item = std::get<BusRouteItem>(items[i]);
            node_tmp = json::Builder{}.StartDict()
                                            .Key("type"s).Value(bus_item.type)
                                            .Key("time"s).Value(bus_item.duration)
                                            .Key("bus"s).Value(bus_item.bus)
                                            .Key("span_count"s).Value(bus_item.span_count)
                                        .EndDict()
                                        .Build();
        }
        else {
            throw std::invalid_argument("LOG err: in TransformRouteItemsToJsonArray - The item type in route is not Wait or Bus"s);
        }
        json_items.push_back(std::move(node_tmp));
    }

    return json_items;
}

json::Dict JsonReader::ProcessRouteRequest(const request_detail::RequestDescription& request) const {
    // std::cout << "LOG start: ProcessRouteRequest ("s << request.id << ")"s << std::endl;
    using namespace routing;
    if (request.type != "Route") {
        throw std::invalid_argument("Request is not a Route"s);
    }
    if (!request.route_from_stop || !request.route_to_stop) {
        throw std::invalid_argument("Request is not a Route"s);
    }
    // инициализируем map для ответа на текущий запрос
    json::Dict response_map;

    std::optional<std::pair<TransportRouteItems, Duration>> route_info = router_ptr_->GetRouteInfo(*request.route_from_stop, *request.route_to_stop);
    
    // случай 1 - путь не найден
    if (!route_info.has_value()) {
        response_map = json::Builder{}.StartDict()
                                            .Key("request_id"s).Value(request.id)
                                            .Key("error_message"s).Value("not found"s)
                                        .EndDict()
                                        .Build()
                                        .AsDict();
        return response_map;
    }

    // случай 2 - остановка отправления и назначения совпадают - путь нулевой
    if (route_info.value().first.empty() && route_info.value().second == 0) {
        response_map = json::Builder{}.StartDict()
                                            .Key("request_id"s).Value(request.id)
                                            .Key("total_time").Value(0)
                                            .Key("items").StartArray().EndArray()
                                        .EndDict()
                                        .Build()
                                        .AsDict();
        return response_map;
    }

    // случай 3 - общий / невырожденный
    const json::Array& json_route_items = TransformRouteItemsToJsonArray(route_info.value().first);

    response_map = json::Builder{}
                            .StartDict()
                                .Key("request_id"s).Value(request.id)
                                .Key("total_time"s).Value(route_info.value().second)
                                .Key("items"s).Value(json_route_items)
                            .EndDict()
                            .Build()
                            .AsDict();
    return response_map;

}


// Обрабатывает один запрос и возвращает словарь данных ответа на запрос
json::Dict JsonReader::ProcessOneRequest(RequestHandler& request_handler, const RequestDescription& request) const {
    json::Dict response_map;
    if (request.IsStop() /* request.type == "Stop" */) {
        response_map = ProcessStopRequest(request_handler, request);
    }
    else if (request.IsBus() /* request.type == "Bus" */) {
        response_map = ProcessBusRequest(request_handler, request);
    }
    else if (request.IsMap() /* request.type == "Map" */) {
        response_map = ProcessMapRequest(request_handler, request);
    }
    else if (request.IsRoute()) {
        response_map = ProcessRouteRequest(request);
    }
    return response_map;
}

// Вспомогательная функция для GetRoutingSettingsFromDocument 
// для извлечения параметров построения маршрута из словаря json::Dict
routing::RoutingSettings ParseRoutingParameters(const json::Dict& settings_map) {
    routing::RoutingSettings routing_options;
    if (settings_map.count("bus_wait_time")) {
        // время ожидания - целое число в минутах
        routing_options.bus_wait_time = settings_map.at("bus_wait_time").AsInt();       
    }
    else {
        std::cerr << "There is NO parameter bus_wait_time in commands. bus_wait_time = 0 minutes will be used\n"s;
    }

    if (settings_map.count("bus_velocity")) {
        // скорость автобуса - вещественное число в км/ч
        routing_options.bus_velocity = settings_map.at("bus_velocity").AsDouble();
    }
    else {
        std::cerr << "There is NO parameter bus_velocity in commands. bus_velocity = 1 km/hour will be used\n"s;
    }
    return routing_options; 
}

// Возвращает параметры построения маршрута, обнаруженные в json-документе
routing::RoutingSettings GetRoutingSettingsFromDocument (const json::Document& document) {
    // Создаем структуру, которую будем заполнять значениями из документа
    // По умолчанию она заполнена некоторыми адекватными значениями
    routing::RoutingSettings routing_params;
    try {
        // 0. Получаем map запросов
        if (!document.GetRoot().IsDict()) {
            std::cerr << "LOG err: in GetRoutingSettingsFromDocument document.GetRoot() is not a map! \n Default routing options will be used!"sv << std::endl;
            return routing_params;
        }

        const json::Dict& main_node = document.GetRoot().AsDict();
        
        // 1. Парсим запросы "routing_settings"
        const json::Node& route_settings_node = main_node.at("routing_settings");
        
        // Проверяем, что render_settings правильно считалась как map
        if (!route_settings_node.IsDict()) {
            std::cerr << "There is no routing_settings" << std::endl;
        }
        // 2. Получаем словарь параметров
        const json::Dict& settings_map = route_settings_node.AsDict(); 

        // 3. Поэлементно добавляем в структуру параметров параметры из json-документа
        routing_params = ParseRoutingParameters(settings_map);  
    }
    catch (std::logic_error& e) {
        std::cerr << "LOG err: from json_reader GetRenderSettingsFromDocument: main_node is not a map => "s << e.what() << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << "LOG err: from json_reader GetRenderSettingsFromDocument: "s << e.what() << std::endl;
    }

    return routing_params;
}


void JsonReader::BuildRouterForRouteRequests(RequestHandler& request_handler) {
    using namespace routing;
    RoutingSettings routing_settings = GetRoutingSettingsFromDocument(document_with_requests_);
    // сроим граф
    TransportGraphMaker graph_maker(request_handler.GetTransportCatalogue(), routing_settings);
    std::unique_ptr<TransportRouter> router_ptr_tmp = std::make_unique<TransportRouter>(std::move(graph_maker));
    router_ptr_ = std::move(router_ptr_tmp);
}

/**
 * Отправляет запросы к транспортному каталогу, 
 * принимает ответы и формирует json::Document 
*/
const json::Document& JsonReader::ProcessRequestsAndGetResponse(RequestHandler& request_handler) {
    // При наличии запросов на маршруты создаем router 
    const auto it = std::find_if(stat_requests_.begin(), stat_requests_.end(), 
                            [](const RequestDescription& req) {
                                return req.IsRoute();
                            });
    if (it != stat_requests_.end()) {
        BuildRouterForRouteRequests(request_handler);
    }
    
    // проверяем, есть ли запросы к базе:
    if (stat_requests_.size() == 0) {
        response_document_ = json::Document(json::Builder{}.Value("null"s).Build());
        return response_document_;
    }
    else if (stat_requests_.size() >= 1) {
        json::Array response_array;
        for (const auto& request_cur : stat_requests_) {
            const json::Dict& response_map_cur = ProcessOneRequest(request_handler, request_cur);
            response_array.emplace_back(response_map_cur);
        }
        // актуализируем документ ответов 
        response_document_ = json::Document(json::Builder{}.Value(response_array).Build());
        
    }
    // возвращаем измененный документ с ответами
    return response_document_;
}



