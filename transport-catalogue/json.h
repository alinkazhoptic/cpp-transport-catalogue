#pragma once

#include <istream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <variant>

// начинаем пространство имен для исключения конфликтов
namespace json {

class Node;

using Dict = std::map<std::string, Node>;
using Array = std::vector<Node>;

// Эта ошибка должна выбрасываться при ошибках парсинга JSON
class ParsingError : public std::runtime_error {
public:
    using runtime_error::runtime_error;
};

class Node {
public:
    // Объявляем тип данных Node Как std::variant
    using Value = std::variant<std::nullptr_t, Array, Dict, bool, int, double, std::string>;
    
    Node() = default;

    // Шаблонный конструктор
    template<typename T>
    Node(T value) : node_value_(std::move(value)){}
    

    const Value& GetValue() const { return node_value_; }

    // --- Методы для проверки типа ---

    bool IsInt() const;
    // Возвращает true, если в Node хранится int либо double.
    bool IsDouble() const;
    // Возвращает true, если в Node хранится double.
    bool IsPureDouble() const; 
    bool IsBool() const;
    bool IsString() const;
    bool IsNull() const;
    bool IsArray() const;
    bool IsMap() const;

    // --- Методы для возвращения значения ---

    bool AsBool() const;
    int AsInt() const;
    // Возвращает значение типа double, если внутри хранится double либо int. 
    // В последнем случае возвращается приведённое в double значение.
    double AsDouble() const;
    const std::string& AsString() const;
    const Array& AsArray() const;
    const Dict& AsMap() const;

    bool operator==(const Node& other) const;
    bool operator!=(const Node& other) const;


private:
    Value node_value_;

};

// Базовая функция для печати узла. 
// Служебные (частные по типам) функции в cpp в пр-ве имен print_detail
void PrintNode(const Node& node, std::ostream& out);

class Document {
public:
    explicit Document(Node root);

    /* 
    explicit Document(const Document& other) {
        root_ = other.GetRoot();
    } 
     */

    /* 
    Document& operator=(const Document& other) {
        root_ = other.GetRoot();
        return *this;
    } 
    */


    const Node& GetRoot() const;

    bool operator==(const Document& other);
    bool operator!=(const Document& other);

private:
    Node root_;
};

Document Load(std::istream& input);

void Print(const Document& doc, std::ostream& output);


// namespace json
}