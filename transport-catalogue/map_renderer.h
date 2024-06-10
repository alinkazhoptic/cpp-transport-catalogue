#pragma once

#include "domain.h"
#include "svg.h"

#include <algorithm>
#include <cstdlib>


/*
 * В этом файле вы можете разместить код, отвечающий за визуализацию карты маршрутов в формате SVG.
 * Визуализация маршртутов вам понадобится во второй части итогового проекта.
 * Пока можете оставить файл пустым.
 */


namespace renderer {

namespace detail {
    
inline const double EPSILON = 1e-6;

bool IsZero(double value);

/*
Класс SphereProjector реализцут проекцию сферических координат(long and lat), широты и долготы,
на координаты пикселей (x, y) соответственно. 
Он масштабирует их так, чтобы вписать в прямоугольник шириной 
    width−2∗padding 
и высотой 
    height−2∗padding,
где padding - отступ краёв карты от границы svg-документа в количесвте пикселей. 

Алгоритм проецирования следующий:
1. Вычисление минимальных и максимальных координат. 
Так как минимальная долгота соответствует минимальной координате x, 
а максимальная широта - минимальной координате y (с svg документа ось y сверху вниз).
Параметры: min_lat, min_lon, max_lat, max_lon

2. Вычисление коэффициентов масштабирования по x и y как размер изображения (ширина или высота), 
деленый на диапазон географицеских координат (долгот или широт).
width_zoom_coef = (width - 2 * padding) / (max_lon - min_lon)
height_zoom_coef = (height - 2 * padding) / (max_lat - min_lat)


3. Итоговый коэффициент масштабирования берется как минимальный из двух, 
но если один из коэффициентов нулевой, то берется второй. Если оба коэфициента нулевые, то есть 
все остановки с одинаковыми координатами, то брать ноль: zoom_coef = 0.

4. Формулы вычисления по широте и долготе - линейная аппроксимация с наклоном, 
равным zoom_coef и подставкой padding.

*/

class SphereProjector {
public:
    // points_begin и points_end задают начало и конец интервала элементов geo::Coordinates
    template <typename PointInputIt>
    SphereProjector(PointInputIt points_begin, PointInputIt points_end,
                    double max_width, double max_height, double padding)
        : padding_(padding) //
    {
        // Если точки поверхности сферы не заданы, вычислять нечего
        if (points_begin == points_end) {
            return;
        }

        // Находим точки с минимальной и максимальной долготой
        const auto [left_it, right_it] = std::minmax_element(
            points_begin, points_end,
            [](auto lhs, auto rhs) { return lhs.lng < rhs.lng; });
        min_lon_ = left_it->lng;
        const double max_lon = right_it->lng;

        // Находим точки с минимальной и максимальной широтой
        const auto [bottom_it, top_it] = std::minmax_element(
            points_begin, points_end,
            [](auto lhs, auto rhs) { return lhs.lat < rhs.lat; });
        const double min_lat = bottom_it->lat;
        max_lat_ = top_it->lat;

        // Вычисляем коэффициент масштабирования вдоль координаты x
        std::optional<double> width_zoom;
        if (!IsZero(max_lon - min_lon_)) {
            width_zoom = (max_width - 2 * padding) / (max_lon - min_lon_);
        }

        // Вычисляем коэффициент масштабирования вдоль координаты y
        std::optional<double> height_zoom;
        if (!IsZero(max_lat_ - min_lat)) {
            height_zoom = (max_height - 2 * padding) / (max_lat_ - min_lat);
        }

        if (width_zoom && height_zoom) {
            // Коэффициенты масштабирования по ширине и высоте ненулевые,
            // берём минимальный из них
            zoom_coeff_ = std::min(*width_zoom, *height_zoom);
        } else if (width_zoom) {
            // Коэффициент масштабирования по ширине ненулевой, используем его
            zoom_coeff_ = *width_zoom;
        } else if (height_zoom) {
            // Коэффициент масштабирования по высоте ненулевой, используем его
            zoom_coeff_ = *height_zoom;
        }
    }

    // Проецирует широту и долготу в координаты внутри SVG-изображения
    svg::Point operator()(geo::Coordinates coords) const {
        return {
            (coords.lng - min_lon_) * zoom_coeff_ + padding_,
            (max_lat_ - coords.lat) * zoom_coeff_ + padding_
        };
    }

private:
    double padding_;
    double min_lon_ = 0;
    double max_lat_ = 0;
    double zoom_coeff_ = 0;
};


}  // namespace detail


struct CanvasParameters {
    double width;
    double height;   
};

struct Label {
    svg::Text underlayer;
    svg::Text text;
};

// Задает параметры для рисования + есть значения по умолчанию
struct RenderingFormatOptions{
    // Размер изображения
    CanvasParameters picture_size_ = {1000, 1000};
    
    // отступ зоны рисунка от границ svg-документа
    double padding;

    // Палитра цветов для отрисовки линий. Цвета используются в порядке очереди.
    std::vector<svg::Color> color_palete_ = {"black", "red"};
    
    // --- Параметры рисунков ---
    double line_width_ = 10.0; // ширина линии для маршрута
    double stop_radius_ = 30.0; // размер круга для остановки

    // --- Параметры текста ---
    
    int bus_label_font_size_ = 20;
    svg::Point bus_label_offset = {5.0, 5.0};
    int stop_label_font_size_ = 20;
    svg::Point stop_label_offset_ = {5.0, 5.0};

    // --- Параметры подложки текста ---

    svg::Color underlayer_color_ = "white";  // цвет фона для текста
    double underlayer_width_ = 5.0;  // толщина фона вокруг текста
};


// Основной класс для отрисовки маршрута
class MapRenderer {
public:
    // Формирует svg из набора элементов/рисунков/объектов типа Object
    void Render(std::ostream& output) const;
    
    /*
    Добавляет все объекты в документ (рисунки маршрутов, остановок, тексты)
    Требование: должен быть сформирован вектор остановок all_stops, 
    которые должны присутствовать на карте.
    Фильтрация остановок (какие рисовать, какие нет) - на совести Пользователя
    */
    void DrawMap(const std::vector<const domain::Stop*>& all_stops, 
                const std::vector<std::pair<std::string_view, const domain::Bus*>>& all_buses);

    /*
    Сохраняет параметры форматирования: формат линий, заливки, размеры и т.п.
    */
    void SetFormatOptions(RenderingFormatOptions render_options) {
        render_options_ = std::move(render_options);
    }
    


private:   
    // Хранит массив объектов для рисования (Render)
    svg::Document map_document_;

    // Хранит параметры рисования-отображения карты
    RenderingFormatOptions render_options_;

    // Формирует линии маршрутов и добавляет их в документ
    void DrawRoutes(const std::vector<std::pair<std::string_view, const domain::Bus*>>& all_buses_data, const detail::SphereProjector& coord_projector);

    // Формирует линию для одного маршрута
    svg::Polyline MakeRouteLine(const std::vector<const domain::Stop*>& stops, const detail::SphereProjector& projector, svg::Color line_color, double line_width) const; 

    // Формирует названия автобусов и добавляет их в документ
    void DrawBusLables(const std::vector<std::pair<std::string_view, const domain::Bus*>>& all_buses_data, const detail::SphereProjector& coord_projector);

    // Формирует заданный текст в заданных герграфических координатах
    svg::Text MakeOneLable(const std::string& text, const geo::Coordinates& coordinates, const detail::SphereProjector& projector);

    // Формирует надпись для автобуса в виде пары элементов: подложка и надпись
    // Добавляет форматирование в текст, созданный с помощью MakeOneLable
    Label MakeBusLable(const std::string& text, const geo::Coordinates& coordinates, const detail::SphereProjector& projector, const svg::Color& text_color) const;

    /* // Меняет цвет на следующий по круговому правилу
    void ChangeColor(std::vector<svg::Color>::iterator color_it) const;
 */
    // Добавляет кружки остановок
    void DrawStops(const std::vector<const domain::Stop*>& stops, const detail::SphereProjector& coord_projector);

    svg::Circle MakeOneStopCircle(const geo::Coordinates coordinates, const detail::SphereProjector& projector) const;    

    // Формирует названия остановок и добавляет их в документ (на рисунок)
    void DrawStopLables(const std::vector<const domain::Stop*>& stops, const detail::SphereProjector& coord_projector);

    Label MakeOneStopLable(const std::string& text, const geo::Coordinates coordinates, const detail::SphereProjector& projector) const; 

    // Формирует правило преобразования (масштабирования) координат  
    detail::SphereProjector ConfigureCoordinateProjector(const std::vector<const domain::Stop*>& all_stops_data) const;


};


}  // namespace renderer