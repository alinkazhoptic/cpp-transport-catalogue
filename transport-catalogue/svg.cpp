#include "svg.h"

#include <iomanip>
#include <utility>

namespace svg {

using namespace std::literals;

void Object::Render(const RenderContext& context) const {
    // Выводим отступы
    context.RenderIndent();

    // Делегируем вывод тега своим подклассам
    RenderObject(context);

    context.out << std::endl;
}

// ----- operator << for dif types ----

std::ostream& operator<<(std::ostream& out, StrokeLineCap linecap) {
    using namespace std::literals;
    switch (linecap)
    {
    case StrokeLineCap::BUTT:
        out << "butt"sv;
        break;
    case StrokeLineCap::ROUND:
        out << "round"sv;
        break;
    case StrokeLineCap::SQUARE:
        out << "square"sv;
        break;
    
    default:
        break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, StrokeLineJoin linejoin) {
    using namespace std::literals;
    switch (linejoin)
    {
    case StrokeLineJoin::ARCS:
        out << "arcs"sv;
        break;
    case StrokeLineJoin::BEVEL:
        out << "bevel"sv;
        break;
    case StrokeLineJoin::MITER:
        out << "miter"sv;
        break;
    case StrokeLineJoin::MITER_CLIP:
        out << "miter-clip"sv;
        break;
    case StrokeLineJoin::ROUND:
        out << "round"sv;
        break;
    
    default:
        break;
    }
    return out;
}


std::string ColorPrinter::operator()(std::monostate) const {
    std::ostringstream out;
    out << "none"sv;
    std::string out_str = out.str();
    return out_str;
}

std::string ColorPrinter::operator()(svg::Rgb rgb) const {
    std::ostringstream out;
    out << "rgb("s << std::to_string(rgb.red) << ","s << std::to_string(rgb.green) << ","s << std::to_string(rgb.blue) << ")"s; 
    std::string out_str = out.str();
    return out_str;
}

std::string ColorPrinter::operator()(svg::Rgba rgba) const {
    std::ostringstream out;
    out << "rgba("s << std::to_string(rgba.red) << ","s << std::to_string(rgba.green) << ","s << std::to_string(rgba.blue) << ","s << rgba.opacity << ")"s;
    std::string out_str = out.str();
    return out_str;
}

std::string ColorPrinter::operator()(const std::string& color_str) const {
    return color_str;
}


std::ostream& operator<<(std::ostream& out, svg::Color color) {
    out << std::visit(ColorPrinter{}, color);
    return out;
}


// ---------- Circle ------------------

Circle& Circle::SetCenter(Point center)  {
    center_ = center;
    return *this;
}

Circle& Circle::SetRadius(double radius)  {
    radius_ = radius;
    return *this;
}

void Circle::RenderObject(const RenderContext& context) const {
    auto& out = context.out;
    out << "<circle cx=\""sv << center_.x << "\" cy=\""sv << center_.y << "\" "sv;
    out << "r=\""sv << radius_ << "\" "sv;
    // Выводим атрибуты, унаследованные от PathProps
    RenderAttrs(context.out);
    out << "/>"sv;
}

// ---------- Polyline ------------------

Polyline& Polyline::AddPoint(Point point) {
    points_list_.push_back(std::move(point));
    return *this;
}

void Polyline::RenderObject(const RenderContext& context) const {
    auto& out = context.out;
    out << "<polyline points=\""sv;
    bool is_first = true;
    for (const auto& point : points_list_) {
        if (!is_first) { 
            out << " "sv;
        }
        out << std::setprecision(6) << point.x << ","sv << point.y;
        is_first = false;
    }
    out << "\"";
    // Выводим атрибуты, унаследованные от PathProps
    RenderAttrs(context.out);
    out << "/>";
    
}

// ---------- Text ------------------

// Задаёт координаты опорной точки (атрибуты x и y)
Text& Text::SetPosition(Point pos) {
    pos_ = std::move(pos);
    return *this;
}

// Задаёт смещение относительно опорной точки (атрибуты dx, dy)
Text& Text::SetOffset(Point offset) {
    offset_ = std::move(offset);
    return *this;
}

// Задаёт размеры шрифта (атрибут font-size)
Text& Text::SetFontSize(uint32_t size) {
    size_ = size;
    return *this;
}

// Задаёт название шрифта (атрибут font-family)
Text& Text::SetFontFamily(std::string font_family) {
    font_family_ = std::move(font_family);
    return *this;
}

// Задаёт толщину шрифта (атрибут font-weight)
Text& Text::SetFontWeight(std::string font_weight) {
    font_weight_ = std::move(font_weight);
    return *this;
}

// Задаёт текстовое содержимое объекта (отображается внутри тега text)
Text& Text::SetData(std::string data) {
    data_ = std::move(data);
    return *this;
}

// Выводит в поток out параметр c названием attr_name и его значение
template <typename AttrType>
void RenderOneAttribute(std::ostream& out, std::string_view attr_name, const AttrType& attr_value) {
    out << attr_name << "=\""sv;
    out << attr_value;
    out << "\" "sv;
}

// Отрисовка - вывод данных
void Text::RenderObject(const RenderContext& context) const {
    auto& out = context.out;
    out << "<text "sv;
    // добавляем координаты положения текста
    RenderOneAttribute(out, "x", pos_.x);
    RenderOneAttribute(out, "y", pos_.y);
    
    // добавляем смещение текста
    RenderOneAttribute(out, "dx", offset_.x);
    RenderOneAttribute(out, "dy", offset_.y);
    // добавляем размер текста
    RenderOneAttribute(out, "font-size", size_);
    // добавляем шрифт, если он задан:
    if (font_family_.has_value()) {
        RenderOneAttribute(out, "font-family", font_family_.value());
    }
    // добавляем стиль (начертание) шрифта, если оно задано:
    if (font_weight_.has_value()) {
        RenderOneAttribute(out, "font-weight", font_weight_.value());
    }
    // Выводим атрибуты, унаследованные от PathProps
    RenderAttrs(context.out);
    out << ">"sv;

    PrintText(out);

    // конец текста
    out << "</text>"sv;
}

// Адаптируем текст, заменяя спецсимволы на их названия и выводим в поток
void Text::PrintText(std::ostream& out) const {
    for (char ch : data_) {
        switch (ch) {
            case '\"':
                out << "&quot;"sv;
                break;
            case '\'':
                out << "&apos;"sv;
                break;
            case '<':
                out << "&lt;"sv;
                break;
            case '>':
                out << "&gt;"sv;
                break;
            case '&':
                out << "&amp;"sv;
                break;
            default:
                out << ch;

        }
    }
}


// ---------- Document ------------------

void Document::AddPtr(std::unique_ptr<Object>&& obj) {
    // to do: Вот тут возможно нужно перемещать по-другому
    objects_.emplace_back(std::move(obj));
}

// Выводит в ostream svg-представление документа
void Document::Render(std::ostream& out) const {
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \n"sv; 
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n"sv;
    RenderContext out_context(out, 2, 2);
    for (const auto& obj_ptr : objects_) {
        obj_ptr->Render(out_context);
    }
    out << "</svg>"sv;
}


}  // namespace svg