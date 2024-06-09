
#include "json.h"

#include <sstream>
#include <set>

using namespace std;

namespace json {

// --- Функции проверки типа ---

bool Node::IsInt() const {
    return std::holds_alternative<int>(node_value_);
}

// Возвращает true, если в Node хранится int либо double.
bool Node::IsDouble() const {
    return (std::holds_alternative<int>(node_value_)) || (std::holds_alternative<double>(node_value_));
}

// Возвращает true, если в Node хранится double.
bool Node::IsPureDouble() const {
    return std::holds_alternative<double>(node_value_);
} 

bool Node::IsBool() const {
    return std::holds_alternative<bool>(node_value_);
}

bool Node::IsString() const {
    return std::holds_alternative<string>(node_value_);
}

bool Node::IsNull() const {
    return std::holds_alternative<std::nullptr_t>(node_value_);
}

bool Node::IsArray() const {
    return std::holds_alternative<Array>(node_value_);
}

bool Node::IsMap() const {
    return std::holds_alternative<Dict>(node_value_);
}

// --- Возвращение значений ---

bool Node::AsBool() const {
    if (!IsBool() || IsNull() ) {
        throw std::logic_error("Node type is not Bool");
    }
    return get<bool>(node_value_);
}

int Node::AsInt() const {
    if (!IsInt() || IsNull()) {
        throw std::logic_error("Node type is not Int");
    }
    return get<int>(node_value_);
}

double Node::AsDouble() const {
    if (IsInt()) {
        return static_cast<double>(get<int>(node_value_));
    }
    else if (IsDouble()) {
        return get<double>(node_value_);;
    }
    else {
        throw std::logic_error("Node type is not Int or Double");
    }
}

const string& Node::AsString() const {
    if (!IsString() || IsNull()) {\
        throw std::logic_error("Node type is not String");
    }
    return get<string>(node_value_);
}

const Array& Node::AsArray() const {
    if (!IsArray() || IsNull()) {
        throw std::logic_error("Node type is not an Array");
    }
    return get<Array>(node_value_);
}

const Dict& Node::AsMap() const {
    if (!IsMap() || IsNull()) {
        throw std::logic_error("Node type is not a Map");
    }
    return get<Dict>(node_value_);
}

// --- Опреаторы сравнения ---

bool IsTypesTheSame(const Node& lhs, const Node& rhs) {
    bool is_both_int = lhs.IsInt() && rhs.IsInt();
    bool is_both_double = lhs.IsDouble() && rhs.IsDouble();
    bool is_both_bool = lhs.IsBool() && rhs.IsBool();
    bool is_both_string = lhs.IsString() && rhs.IsString();
    bool is_both_array = lhs.IsArray() && rhs.IsArray();
    bool is_both_map = lhs.IsMap() && rhs.IsMap();
    return (is_both_int || is_both_double || is_both_bool ||is_both_string || is_both_array || is_both_map);
}

bool Node::operator==(const Node& other) const {
    // Пустые узлы всегда равны
    if (this->IsNull() && other.IsNull()) {
        return true;
    }
    if (IsTypesTheSame(*this, other)) {
        return node_value_ == other.node_value_;
    }
    return false;  
}

bool Node::operator!=(const Node& other) const {
    return !(*this == other);
}


// --- Загрузка  документов ---


Document::Document(Node root)
    : root_(move(root)) {
}

const Node& Document::GetRoot() const {
    return root_;
}

bool Document::operator==(const Document& other) {
    return GetRoot() == other.GetRoot();
}

bool Document::operator!=(const Document& other){
    return GetRoot() != other.GetRoot();
}



namespace load_detail {

using Number = std::variant<int, double>;

// Объявляем базовую функцию заранее, т.к. частные "загрузчики" будут её использовать
Node LoadNode(istream& input);


// using Number = std::variant<int, double>;

Number LoadNumber(std::istream& input) {
    using namespace std::literals;

    std::string parsed_num;

    // Считывает в parsed_num очередной символ из input
    auto read_char = [&parsed_num, &input] {
        parsed_num += static_cast<char>(input.get());
        if (!input) {
            throw ParsingError("Failed to read number from stream"s);
        }
    };

    // Считывает одну или более цифр в parsed_num из input
    auto read_digits = [&input, read_char] {
        if (!std::isdigit(input.peek())) {
            throw ParsingError("A digit is expected"s);
        }
        while (std::isdigit(input.peek())) {
            read_char();
        }
    };

    if (input.peek() == '-') {
        read_char();
    }
    // Парсим целую часть числа
    if (input.peek() == '0') {
        read_char();
        // После 0 в JSON не могут идти другие цифры
    } else {
        read_digits();
    }

    bool is_int = true;
    // Парсим дробную часть числа
    if (input.peek() == '.') {
        read_char();
        read_digits();
        is_int = false;
    }

    // Парсим экспоненциальную часть числа
    if (int ch = input.peek(); ch == 'e' || ch == 'E') {
        read_char();
        if (ch = input.peek(); ch == '+' || ch == '-') {
            read_char();
        }
        read_digits();
        is_int = false;
    }

    try {
        if (is_int) {
            // Сначала пробуем преобразовать строку в int
            try {
                return std::stoi(parsed_num);
            } catch (...) {
                // В случае неудачи, например, при переполнении,
                // код ниже попробует преобразовать строку в double
            }
        }
        return std::stod(parsed_num);
    } catch (...) {
        throw ParsingError("Failed to convert "s + parsed_num + " to number"s);
    }
}

// Считывает содержимое строкового литерала JSON-документа
// Функцию следует использовать после считывания открывающего символа ":
std::string LoadString(std::istream& input) {
    using namespace std::literals;
    
    auto it = std::istreambuf_iterator<char>(input);
    auto end = std::istreambuf_iterator<char>();
    std::string s;
    while (true) {
        if (it == end) {
            // Поток закончился до того, как встретили закрывающую кавычку?
            throw ParsingError("String parsing error");
        }
        const char ch = *it;
        if (ch == '"') {
            // Встретили закрывающую кавычку
            ++it;
            break;
        } else if (ch == '\\') {
            // Встретили начало escape-последовательности
            ++it;
            if (it == end) {
                // Поток завершился сразу после символа обратной косой черты
                throw ParsingError("String parsing error");
            }
            const char escaped_char = *(it);
            // Обрабатываем одну из последовательностей: \\, \n, \t, \r, \"
            switch (escaped_char) {
                case 'n':
                    s.push_back('\n');
                    break;
                case 't':
                    s.push_back('\t');
                    break;
                case 'r':
                    s.push_back('\r');
                    break;
                case '"':
                    s.push_back('"');
                    break;
                case '\\':
                    s.push_back('\\');
                    break;
                default:
                    // Встретили неизвестную escape-последовательность
                    throw ParsingError("Unrecognized escape sequence \\"s + escaped_char);
            }
        } else if (ch == '\n' || ch == '\r') {
            // Строковый литерал внутри- JSON не может прерываться символами \r или \n
            throw ParsingError("Unexpected end of line"s);
        } else {
            // Просто считываем очередной символ и помещаем его в результирующую строку
            s.push_back(ch);
        }
        ++it;
    }

    return s;
}


Node LoadArray(istream& input) {
    // Проверяем, что данные есть:
    char c_first;
    input >> c_first;
    if (!c_first ) {
        throw ParsingError("Failed to read Array from stream"s);
    }
    input.putback(c_first);

    // Считываем посимвольно и записываем в массив
    vector<Node> result;
    for (char c; input >> c && c != ']';) {
        if (c != ',') {
            input.putback(c);
        }
        result.push_back(LoadNode(input));
    }

    return Node(move(result));
}


// Считывает один набор значений между {}
Node LoadDict(istream& input) {
    // Проверяем, что данные есть:
    char c_first;
    input >> c_first;
    if (!c_first ) {
        throw ParsingError("Failed to read Array from stream"s);
    }
    input.putback(c_first);

    // Считываем посимвольно и записываем в словарь
    map<string, Node> result;
    for (char c; input >> c && c != '}';) {
        if (c == ',') {
            input >> c;
        }

        string key = LoadString(input);
        input >> c;
        result.insert({move(key), LoadNode(input)});
    }

    return Node(move(result));
}

std::string ReadLexeme(istream& input) {
    string str;
    char c;
    set<char> spec_symbols({'[', ']', '{', '}', ' '});
    
    while (input >> c) {
        if (spec_symbols.count(c)) {
            input.putback(c);
            break;
            // throw ParsingError(("Read Lexeme failed due to spec symbol "s + str + c));
        }
        if (c == ',') {
            break;
        }
        str.push_back(c);
    }
    return str;
}

Node LoadNode(istream& input) {
    char c;
    input >> c;
    std::string str;
    int opened_brackets = 0;
    switch (c) {
        case '[':\
            return LoadArray(input);
            break;
        case '{':
            ++opened_brackets;
            return LoadDict(input);
            break;
        case '}':
            --opened_brackets;
            break;
        case '\"':
            return Node(LoadString(input));
            break;
        /* Обработка случаев null, true, false одинаковая: считываем слово и в зависимости от его фида формируем Node*/ 
        case 'n': 
            // Атрибут [[fallthrough]] (провалиться) ничего не делает, и является
            // подсказкой компилятору и человеку, что здесь программист явно задумывал
            // разрешить переход к инструкции следующей ветки case, а не случайно забыл
            // написать break, return или throw.
            // В данном случае, встретив t или f, переходим к попытке парсинга
            // литералов true либо false
            [[fallthrough]];
        case 't': 
            [[fallthrough]];
        case 'f':
            input.putback(c);
            str = ReadLexeme(input);
            if (str == "null"s) { return Node();}
            else if (str == "true"s) { return Node(true);}
            else if (str == "false"s) { return Node(false);}
            else { throw ParsingError("Failed to read special word: "s + str);}
            break;
        default:
            input.putback(c);
            Number num = LoadNumber(input);
            if (holds_alternative<int>(num)) {
                return Node(get<int>(num));
            }
            else if (holds_alternative<double>(num)) {
                return Node(get<double>(num));
            }
            // считываем строку до следующего пробела
            break;
    }

    
    
    // проверяем, что количества открытых и закрытых скобочек совпадают
    if (opened_brackets != 0) {
        throw ParsingError(("Number of opended and closed brackets are not the same: "s + to_string(opened_brackets) + "rest opend"s));
    }
    // на всякий случай, иначе не компилируется в песочнице
    return Node();
}

// конец пространства load_detail
}

Document Load(istream& input) {
    return Document{load_detail::LoadNode(input)};
}


// --- Печать узла ---
namespace print_detail {

// Перегрузка функции PrintValue для вывода значений int
void PrintValue(int number, std::ostream& out) {
    out << number;
}

// Перегрузка функции PrintValue для вывода значений double
void PrintValue(double number_d, std::ostream& out) {
    out << number_d;
}

// Перегрузка функции PrintValue для вывода значений null
void PrintValue(std::nullptr_t, std::ostream& out) {
    out << "null"sv;
}

// Перегрузка функции PrintValue для вывода значений string
void PrintValue(std::string str, std::ostream& out) {
    out << "\""s;
    // Выводим посимвольно, 
    // обрабатывая встречающиеся escape-последовательности
    for (char c : str) {
        switch (c) {
            case '\\':
                out << "\\\\"s;
                break;
            case '\"':
                out << "\\\""s;
                break;
            case '\t':
                out << "\\t"s;
                break;
            case '\n':
                out << "\\n"s;
                break;
            case '\r':
                out << "\\r"s;
                break;
            default:
                out << c;
        }
    }
    out << "\""s;
}

// Перегрузка функции PrintValue для вывода значений bool
void PrintValue(bool bool_value, std::ostream& out) {
    out << std::boolalpha << bool_value;
}

// Перегрузка функции PrintValue для вывода значений ArrayMap
void PrintValue(Array nodes_array, std::ostream& out) {
    bool is_first = true;
    out << "["s;
    for (const auto& node : nodes_array) {
        // выводим разделитель
        if (!is_first) {
            out << ", "s;
        }
        is_first = false;
        // выводим элемент массива
        PrintNode(node, out);
    }
    out << "]\n"s;
}

// Перегрузка функции PrintValue для вывода значений Dict (Map)
void PrintValue(Dict nodes_dict, std::ostream& out) {
    bool is_first = true;
    out << "\n{\n"s;
    for (const auto& [str, node] : nodes_dict) {
        if (!is_first) {
            out << ", \n"s;
        }
        is_first = false;
        // выводим ключ
        PrintValue(str, out);
        // выводим разделитель
        out << ": "s;
        // выводим значение
        PrintNode(node, out);
    }
    out << "\n}"s;
}


// Контекст вывода, хранит ссылку на поток вывода и текущий отсуп
struct PrintContext {
    std::ostream& out;
    int indent_step = 2;
    int indent = 0;

    void PrintIndent() const {
        for (int i = 0; i < indent; ++i) {
            out.put(' ');
        }
    }

    // Возвращает новый контекст вывода с увеличенным смещением
    PrintContext Indented() const {
        return {out, indent_step, indent_step + indent};
    }
};


template <typename Value>
void PrintNodeValue(const Value& value, const PrintContext& ctx) {
    // Надо реализовать
    auto& out = ctx.out;
    std::visit(
        // лямбда-функция, так как надо передать поток вывода как параметр
        [&out] (const auto& val) { PrintValue(val, out); },  
        value);
}

} // namespace print_detail

void PrintNode(const Node& node, std::ostream& out) {
    print_detail::PrintContext out_context({out, 2, 0});
    // сдвигаем на два пробела каждый логический узел
    print_detail::PrintNodeValue(node.GetValue(), out_context);
}


void Print(const Document& doc, std::ostream& output) {
    // output << "{\n"sv;
    PrintNode(doc.GetRoot(), output);
    // output << "\n}"sv;

    // Реализуйте функцию самостоятельно
}


// закрываем пространство имен json
}