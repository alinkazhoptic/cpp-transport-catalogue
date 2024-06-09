#include "map_renderer.h"
#include "geo.h"
#include "svg.h"


#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace renderer;


// Формирует svg из набора элементов/рисунков/объектов типа Object
void MapRenderer::Render(std::ostream& output) const {
    map_document_.Render(output);
}

svg::Polyline MapRenderer::MakeRouteLine(std::vector<const domain::Stop*> stops, const detail::SphereProjector& projector, svg::Color line_color, double line_width) const {
    svg::Polyline route_line;
    // Идем по остановкам и добавляем координаты в полилинию
    for (const auto& stop_ptr : stops) {
        // Проецируем точку = формируем точку с координатами x, y
        svg::Point stop_point_on_map = projector(stop_ptr->coordinates);
        // Записываем точку в полилинию
        route_line.AddPoint(stop_point_on_map);
        // Задаем форматирование линии
        route_line.SetFillColor(svg::NoneColor)  // по ТЗ нет заливки
            .SetStrokeLineCap(svg::StrokeLineCap::ROUND)  // по ТЗ круглая форма "кисти"
            .SetStrokeLineJoin(svg::StrokeLineJoin::ROUND)  // по ТЗ соединение линий тоже круглое 
            .SetStrokeWidth(line_width)  // ширина линии задается снаружи
            .SetStrokeColor(line_color);  // цвет линии задается снаружи
    }
    return route_line;
}


// Формирует линии маршрутов в соответствии с заданным правилом преобразования координат и добавляет их в документ
void MapRenderer::DrawRoutes(std::vector<std::pair<std::string_view, const domain::Bus*>> all_buses_data, const detail::SphereProjector& coord_projector) {
    auto color_it = render_options_.color_palete_.begin();
    // Двойной цикл, внешний - по автобусам
    for (const auto& cur_bus_data : all_buses_data ) {
        const domain::Bus* bus_cur = cur_bus_data.second;
        // Если остановок нет, то рисовать нечего, переходим к следующему автобусу
        if (bus_cur->stops_on_route.empty()) {
            continue;
        }
        // Остановки ест => формируем линию маршрута
        svg::Polyline route_line = MakeRouteLine(bus_cur->stops_on_route, coord_projector, *color_it, render_options_.line_width_);

        // Добавляем линию на рисунок (к документу)
        map_document_.Add(route_line);

        // Переходим к следующему цвету линии, перебирая цвета "по кругу"
        if (++color_it == render_options_.color_palete_.end()) {
            color_it = render_options_.color_palete_.begin();
        }
    }
    return;
}

// Формирует надпись для автобуса в виде пары элементов: 
// first - подложка, second - надпись
Label MapRenderer::MakeBusLable(const std::string& text, const geo::Coordinates& coordinates, const detail::SphereProjector& projector, const svg::Color& text_color) const {
    // 
    svg::Text bus_lable_underlayer;
    svg::Text bus_lable_text;

    // Расчитываем координаты c учетом смещения текста относительно заданных координат остановки
    svg::Point pos_point = projector(coordinates);
    // pos_point.x += render_options_.bus_label_offset.x;
    // pos_point.y += render_options_.bus_label_offset.y;
    
    // Добавляем текст
    bus_lable_underlayer.SetData(text);
    bus_lable_text.SetData(text);
    // Добавляем положение
    bus_lable_underlayer.SetPosition(pos_point);
    bus_lable_text.SetPosition(pos_point);
    // Добавляем смещение
    bus_lable_underlayer.SetOffset(render_options_.bus_label_offset);
    bus_lable_text.SetOffset(render_options_.bus_label_offset);
    // Устанавливаем размер шрифта
    bus_lable_underlayer.SetFontSize(render_options_.bus_label_font_size_);
    bus_lable_text.SetFontSize(render_options_.bus_label_font_size_);
    // Устанавливаем шрифт
    bus_lable_underlayer.SetFontFamily("Verdana");
    bus_lable_text.SetFontFamily("Verdana");
    // Устанавливаем ширину текста
    bus_lable_underlayer.SetFontWeight("bold");
    bus_lable_text.SetFontWeight("bold");
    
    // Устанавливаем параметры, специфические для подложки
    bus_lable_underlayer.SetFillColor(render_options_.underlayer_color_);  // заливка
    bus_lable_underlayer.SetStrokeColor(render_options_.underlayer_color_);  // линия текста
    bus_lable_underlayer.SetStrokeWidth(render_options_.underlayer_width_);
    bus_lable_underlayer.SetStrokeLineCap(svg::StrokeLineCap::ROUND);
    bus_lable_underlayer.SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);

    // Устанавливаем заданный цвет текста
    bus_lable_text.SetFillColor(text_color);

    return {bus_lable_underlayer, bus_lable_text};

}


// Формирует названия автобусов и добавляет их в документ
void MapRenderer::DrawBusLables(std::vector<std::pair<std::string_view, const domain::Bus*>> all_buses_data, const detail::SphereProjector& coord_projector) {
    // 0. Задаем начальный цвет
    auto color_it = render_options_.color_palete_.begin();
    // 1. Идем по автобусам
    for (const auto& [bus_name, bus_ptr] : all_buses_data) {
        size_t stops_count = bus_ptr->stops_on_route.size();
        // Если остановок нет, то и выводить название автобуса не требуется
        if (stops_count == 0) {
            continue;
        }

        std::string first_stop_name = bus_ptr->stops_on_route.at(0)->name;
        
        Label first_lable = MakeBusLable(std::string(bus_name), bus_ptr->stops_on_route.at(0)->coordinates, coord_projector, *color_it);
        std::string last_stop_name = bus_ptr->stops_on_route.at(stops_count/2)->name;

        // Добавляем на рисунок-документ
        map_document_.Add(first_lable.underlayer);
        map_document_.Add(first_lable.text);

        // Отрисовываем конечную остановку, если она не совпадает с начальной
        if (!bus_ptr->is_round && (last_stop_name != first_stop_name)) {
            
            Label last_lable = MakeBusLable(std::string(bus_name), bus_ptr->stops_on_route.at(stops_count/2)->coordinates, coord_projector, *color_it);
            // Добавляем на рисунок-документ
            map_document_.Add(last_lable.underlayer);
            map_document_.Add(last_lable.text);
        }

        // Переходим к следующему цвету линии, перебирая цвета "по кругу"
        if (++color_it == render_options_.color_palete_.end()) {
            color_it = render_options_.color_palete_.begin();
        }
    }
}


svg::Circle MapRenderer::MakeOneStopCircle(const geo::Coordinates coordinates, const detail::SphereProjector& projector) const {
    svg::Circle stop_circle;
    // Расчитываем координаты
    svg::Point pos_point = projector(coordinates);
    
    stop_circle.SetCenter(pos_point);
    stop_circle.SetRadius(render_options_.stop_radius_);
    stop_circle.SetFillColor("white");
    
    return stop_circle;
}

// Добавляет кружки остановок на рисунок.
// На вход принимает вектор указателей на остановки. 
// До применения этого метода остановки должны быть отсортированы нужным Пользователю образом
void MapRenderer::DrawStops(const std::vector<const domain::Stop*>& stops, const detail::SphereProjector& coord_projector) {
    for (const auto& stop : stops) {
        svg::Circle stop_circle = MakeOneStopCircle(stop->coordinates, coord_projector);
        map_document_.Add(stop_circle);
    }
}

Label MapRenderer::MakeOneStopLable(const std::string& stop_name, const geo::Coordinates coordinates, const detail::SphereProjector& projector) const {
    svg::Text stop_underlayer;
    svg::Text stop_text;

    // Расчитываем координаты c учетом смещения текста относительно заданных координат остановки
    svg::Point pos_point = projector(coordinates);
    
    // Добавляем текст
    stop_underlayer.SetData(stop_name);
    stop_text.SetData(stop_name);
    // Добавляем положение и смещение
    stop_underlayer.SetPosition(pos_point).SetOffset(render_options_.stop_label_offset_);
    stop_text.SetPosition(pos_point).SetOffset(render_options_.stop_label_offset_);
    // Добавляем размер текста и шрифт
    stop_underlayer.SetFontSize(render_options_.stop_label_font_size_)
                    .SetFontFamily("Verdana");
    stop_text.SetFontSize(render_options_.stop_label_font_size_)
                .SetFontFamily("Verdana");
    
    // Указываем специфичные для полдожки свойства
    stop_underlayer.SetFillColor(render_options_.underlayer_color_)
                    .SetStrokeColor(render_options_.underlayer_color_)
                    .SetStrokeWidth(render_options_.underlayer_width_)
                    .SetStrokeLineCap(svg::StrokeLineCap::ROUND)
                    .SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
    
    // Указываем специфичные свойства надписи
    stop_text.SetFillColor("black"); 

    return {stop_underlayer, stop_text};
    


}

// Формирует названия остановок и добавляет их в документ (на рисунок)
void MapRenderer::DrawStopLables(const std::vector<const domain::Stop*>& stops, const detail::SphereProjector& coord_projector) {
    for (const auto& stop : stops) {
        Label stop_label = MakeOneStopLable(stop->name, stop->coordinates, coord_projector);
        map_document_.Add(stop_label.underlayer);
        map_document_.Add(stop_label.text);
    }
}

/*
Добавляет все объекты в документ (рисунки маршрутов, остановок, тексты)
Требование: должен быть сформирован вектор остановок all_stops, 
которые должны присутствовать на карте.
Фильтрация остановок (какие рисовать, какие нет) - на совести Пользователя
*/
void MapRenderer::DrawMap(const std::vector<const domain::Stop*>& all_stops, const std::vector<std::pair<std::string_view, const domain::Bus*>>& all_buses) {
    // Сформировать вектор остановок по которым строить SphereProjector
    detail::SphereProjector projector = ConfigureCoordinateProjector(all_stops);
    
    // Отрисовать маршруты  
    DrawRoutes(all_buses, projector);
    
    // Отрисовть названия маршрутов
    DrawBusLables(all_buses, projector);
    
    // Отрисовать остановки, пока закомментировано, потому что не требуется
    DrawStops(all_stops, projector); 

    // Отрисовать названия остановок
    DrawStopLables(all_stops, projector);
   
}


// Формирует правило преобразования (масштабирования) координат  
detail::SphereProjector MapRenderer::ConfigureCoordinateProjector(std::vector<const domain::Stop*> all_stops_data) const {
    // 0. Создаем вектор всех координат
    std::vector<geo::Coordinates> all_coordinates;
    // резервируем место, чтоб не тратить ресурсы на накладные расходы
    all_coordinates.reserve(all_stops_data.size());
    for (const auto& stop_ptr : all_stops_data) {
        all_coordinates.push_back(stop_ptr->coordinates);
    }
    
    // 1. Создаем объект SphereProjector
    detail::SphereProjector projector{
        all_coordinates.begin(), all_coordinates.end(), 
        render_options_.picture_size_.width, render_options_.picture_size_.height, render_options_.padding};

    // 2. Возвращаем проектор
    return projector;
}


bool detail::IsZero(double value) {
    return std::abs(value) < EPSILON;
}



