#pragma once

#include <cstdint>
#include <iostream>
#include <ostream>
#include <sstream>
#include <deque>
#include <vector>
#include <memory>
#include <optional>
#include <string>
#include <variant>


namespace svg {


struct Rgb {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
};


struct Rgba {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    double opacity = 1.0;
};

// using Color = std::string;
using Color = std::variant<std::monostate, std::string, Rgb, Rgba>;

// Объявив в заголовочном файле константу со спецификатором inline,
// мы сделаем так, что она будет одной на все единицы трансляции,
// которые подключают этот заголовок.
// В противном случае каждая единица трансляции будет использовать свою копию этой константы
inline const Color NoneColor{"none"};

enum class StrokeLineCap {
    BUTT,
    ROUND,
    SQUARE,
};

enum class StrokeLineJoin {
    ARCS,
    BEVEL,
    MITER,
    MITER_CLIP,
    ROUND,
};


struct ColorPrinter {
    std::string operator()(std::monostate) const; // было std::string_view
    std::string operator()(Rgb rgb) const;
    std::string operator()(Rgba rgba) const;
    std::string operator()(const std::string& color_str) const;
};

/* std::ostream& operator<<(std::ostream& out, Rgb rgb); 
std::ostream& operator<<(std::ostream& out, Rgba rgba); 
*/

std::ostream& operator<<(std::ostream& out, svg::Color color);

std::ostream& operator<<(std::ostream& out, StrokeLineCap linecap);
std::ostream& operator<<(std::ostream& out, StrokeLineJoin linejoin); 



struct Point {
    Point() = default;
    Point(double x, double y)
        : x(x)
        , y(y) {
    }
    double x = 0;
    double y = 0;
};

/*
 * Вспомогательная структура, хранящая контекст для вывода SVG-документа с отступами.
 * Хранит ссылку на поток вывода, текущее значение и шаг отступа при выводе элемента
 */
struct RenderContext {
    RenderContext(std::ostream& out)
        : out(out) {
    }

    RenderContext(std::ostream& out, int indent_step, int indent = 0)
        : out(out)
        , indent_step(indent_step)
        , indent(indent) {
    }

    RenderContext Indented() const {
        return {out, indent_step, indent + indent_step};
    }

    void RenderIndent() const {
        for (int i = 0; i < indent; ++i) {
            out.put(' ');
        }
    }

    std::ostream& out;
    int indent_step = 0;
    int indent = 0;
};

/* Вспомогательный класс, содержащий свойства, управляющие параметрами заливки и контура
Принимает шаблонный параметр Owner, который задаёт тип класса, 
владеющего этими свойствами.
*/
template <typename Owner>
class PathProps {
public:
    Owner& SetFillColor(Color fill_color) {
        fill_color_ = std::move(fill_color);
        return AsOwner();
    }

    Owner& SetStrokeColor(Color stroke_color) {
        stroke_color_ = std::move(stroke_color);
        return AsOwner();
    }

    Owner& SetStrokeWidth(double width) {
        stroke_width_ = std::move(width);
        return AsOwner();
    }

    Owner& SetStrokeLineCap(StrokeLineCap line_cap) {
        stroke_linecap_ = std::move(line_cap);
        return AsOwner();
    }

    Owner& SetStrokeLineJoin(StrokeLineJoin line_join) {
        stroke_linejoin_ = std::move(line_join);
        return AsOwner();
    }

protected:
    ~PathProps() = default;

    /*
    // Метод RenderAttrs выводит в поток общие для всех путей атрибуты fill и stroke
    Защищённый метод PathProps::RenderAttrs используется в дочерних классах 
    для вывода атрибутов, общих для всех наследников PathProps.
    */
    void RenderAttrs(std::ostream& out) const {
        using namespace std::literals;

        if (fill_color_) {
            out << " fill=\""sv << fill_color_.value() << "\""sv;
        }

        if (stroke_color_) {
            out << " stroke=\""sv << stroke_color_.value() << "\""sv;
        }

        if (stroke_width_) {
            out << " stroke-width=\""sv << *stroke_width_ << "\""sv;
        }

        if (stroke_linecap_) {
            out << " stroke-linecap=\""sv << *stroke_linecap_ << "\""sv;
        }

        if (stroke_linejoin_) {
            out << " stroke-linejoin=\""sv << *stroke_linejoin_ << "\""sv;
        }
        
    }

private:
    Owner& AsOwner() {
        // static_cast безопасно преобразует *this к Owner&,
        // если класс Owner — наследник PathProps
        return static_cast<Owner&>(*this);
    }

    std::optional<Color> fill_color_;

    std::optional<Color> stroke_color_;

    std::optional<double> stroke_width_;
    std::optional<StrokeLineCap> stroke_linecap_;
    std::optional<StrokeLineJoin> stroke_linejoin_;

};



/*
 * Абстрактный базовый класс Object служит для унифицированного хранения
 * конкретных тегов SVG-документа
 * Реализует паттерн "Шаблонный метод" для вывода содержимого тега
 */
class Object {
public:
    void Render(const RenderContext& context) const;

    virtual ~Object() = default;

private:
    virtual void RenderObject(const RenderContext& context) const = 0;
};

/*
 * Класс Circle моделирует элемент <circle> для отображения круга
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/circle
 * Наследованием от PathProps<Circle> мы «сообщаем» родителю,
 * что владельцем свойств является класс Circle
 */
class Circle final : public Object, public PathProps<Circle>  {
public:
    Circle& SetCenter(Point center);
    Circle& SetRadius(double radius);

private:
    void RenderObject(const RenderContext& context) const override;

    Point center_;
    double radius_ = 1.0;
};

/*
 * Класс Polyline моделирует элемент <polyline> для отображения ломаных линий
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/polyline
 */
class Polyline final : public Object, public PathProps<Polyline> {
public:
    // Добавляет очередную вершину к ломаной линии
    Polyline& AddPoint(Point point);

private:
    // Отрисовка - вывод данных
    void RenderObject(const RenderContext& context) const override;

    std::deque<Point> points_list_ = {};

    /*
     * Прочие методы и данные, необходимые для реализации элемента <polyline>
     */
};

/*
 * Класс Text моделирует элемент <text> для отображения текста
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/text
 */
class Text final : public Object, public PathProps<Text> {
public:
    // Задаёт координаты опорной точки (атрибуты x и y)
    Text& SetPosition(Point pos);

    // Задаёт смещение относительно опорной точки (атрибуты dx, dy)
    Text& SetOffset(Point offset);

    // Задаёт размеры шрифта (атрибут font-size)
    Text& SetFontSize(uint32_t size);

    // Задаёт название шрифта (атрибут font-family)
    Text& SetFontFamily(std::string font_family);

    // Задаёт толщину шрифта (атрибут font-weight)
    Text& SetFontWeight(std::string font_weight);

    // Задаёт текстовое содержимое объекта (отображается внутри тега text)
    Text& SetData(std::string data);

private:

    // Отрисовка - вывод данных
    void RenderObject(const RenderContext& context) const override;
    
    // Адаптируем текст, заменяя спецсимволы на их названия и выводим в поток
    void PrintText(std::ostream& out) const;

    // Прочие данные и методы, необходимые для реализации элемента <text>
    Point pos_;
    Point offset_;
    uint32_t size_ = 1;
    std::optional<std::string> font_family_;
    std::optional<std::string> font_weight_;
    std::string data_;

};


/*
ObjectContainer задаёт интерфейс для доступа к контейнеру SVG-объектов. 
Через этот интерфейс Drawable-объекты могут визуализировать себя, 
добавляя в контейнер SVG-примитивы. svg::Document — пока единственный класс библиотеки, реализующий интерфейс 
ObjectContainer.
*/
class ObjectContainer {
public:
    virtual ~ObjectContainer() = default;
    /* 
    Добавляет в svg-документ объект-наследник svg::Object
    Чисто виртуальный метод, который переопределен в классах-наследниках, например, в Document
    */
    virtual void AddPtr(std::unique_ptr<Object>&& obj) = 0;

    /*
     Метод Add добавляет в svg-документ любой объект-наследник svg::Object.
     Метод ObjectContainer::Add реализуйте на основе чисто виртуального метода ObjectContainer::AddPtr, 
     принимающего unique_ptr<Object>&&.
    */
    template <typename Obj>
    void Add(Obj obj) {
        std::unique_ptr<Obj> obj_ptr = std::make_unique<Obj>(std::move(obj));
        AddPtr(std::move(obj_ptr));
        // objects_.emplace_back(std::make_unique<Obj>(std::move(obj)));
    }

private:

};



class Document : public ObjectContainer {
public:
    /*
     Метод Add добавляет в svg-документ любой объект-наследник svg::Object.
     Пример использования:
     Document doc;
     doc.Add(Circle().SetCenter({20, 30}).SetRadius(15));
     Метод унаследован, его реализация не требуется
    */
    /*     
    template <typename Obj>
    void Add(Obj obj) {
        objects_.emplace_back(std::make_unique<Obj>(std::move(obj)));
    } 
    */

    // Добавляет в svg-документ объект-наследник svg::Object
    void AddPtr(std::unique_ptr<Object>&& obj);

    // Выводит в ostream svg-представление документа
    void Render(std::ostream& out) const;

    // Прочие методы и данные, необходимые для реализации класса Document

private:
    std::vector<std::unique_ptr<Object>> objects_;
    



};


/*
Интерфейс Drawable унифицирует работу с объектами, которые можно нарисовать, 
подключив SVG-библиотеку. Для этого в нём есть метод Draw, 
принимающий ссылку на интерфейс ObjectContainer.
*/
class Drawable {
public:
    virtual ~Drawable() = default;
    virtual void Draw(ObjectContainer& container) const = 0;


private:

};


}  // namespace svg