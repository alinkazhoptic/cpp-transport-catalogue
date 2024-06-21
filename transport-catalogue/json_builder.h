
#include "json.h"

namespace json {

// Классы для формирования ошибок компиляции 
// при некорректной последовательности вызовов метоов Строителя

class ArrayItemContext;
class DictItemContext;
class DictValueContext;
class BaseContext;



class Builder {
public:

    Builder () {
        nodes_stack_.emplace_back(&root_);
        commands_.push_back(Commands::CONSTRUCTOR);
    };

    Builder& Value(Node::Value value);  // было Builder&

    DictItemContext StartDict();

    DictValueContext Key(std::string key);

    ArrayItemContext StartArray();

    BaseContext EndDict();

    BaseContext EndArray();

    Node Build();


private:
    // конструируемый объект
    Node root_;

    // стек указателей на те вершины JSON, которые ещё не построены: 
    // то есть текущее описываемое значение и цепочка его родителей. 
    // Он поможет возвращаться в нужный контекст после вызова End-методов.
    std::vector<Node*> nodes_stack_;

    // последний заданный ключ, для формирования словаря
    // Задается методом Key после начала формирования словаря StartDict 
    std::vector<std::string> dict_keys_;

    enum class ContainerType {ARRAY, DICT};

    std::vector<ContainerType> opened_containers_;


    enum class Commands {
        CONSTRUCTOR,
        VALUE,
        START_DICT,
        START_ARRAY,
        END_DICT,
        END_ARRAY,
        KEY,
        ADD_TO_ARRAY,
        ADD_TO_MAP,
        BUILD
    };

    std::vector<Commands> commands_;

    void ThrowOnEmptyNodeStack() const;

    /*
    Исключение при вызове Value, StartDict или StartArray где-либо, 
    кроме как после конструктора, после Key или после предыдущего 
    элемента массива
    */
    bool CheckCommandsOrderForValueAndStart() const; 

    // Исключение при вызове метода Key снаружи словаря 
    // или сразу после другого Key.
    void CheckAndThrowForKeyUsing() const;

    void CheckForEndingDict() const;
    void CheckForEndingArray() const;

    void AddCurrentValueToPreviousOpenedNode();

};


// Классы для формирования ошибок компиляции 
// при некорректной последовательности вызовов метоов Строителя

// Основной класс - класс со всеми методами Builder
class BaseContext {
private: 
    Builder& builder_;

public:
    explicit BaseContext(Builder& builder);

    virtual ~BaseContext() = default;

    BaseContext Value(Node::Value value);

    DictItemContext StartDict();

    DictValueContext Key(std::string key);

    ArrayItemContext StartArray();

    // Возможно, нужно другое возвращаемое значение
    BaseContext EndDict();

    BaseContext EndArray();

    Node Build();

    Builder& GetBuilder();
    

};


// Класс-наследник от Base для формирования значения по ключу
// Обеспечение первого правила:
// Код не должен компилироваться, если непосредственно после Key вызван 
// не Value, не StartDict и не StartArray. 
class DictValueContext final : public BaseContext {
public:
    explicit DictValueContext(Builder& builder);

    ~DictValueContext() = default;

    // Оставляем только вариант вызова Value для элемента словаря
    DictItemContext Value(Node::Value value);

    DictValueContext Key(std::string key) = delete;
    BaseContext EndDict() = delete;
    BaseContext EndArray() = delete; 
    Node Build() = delete;
private:
    // Builder& builder_;

}; 


// Класс-наследник от Base
// Обеспечение условия НЕкомпиляции если после вызова Value, последовавшего за Key, 
// вызваны не Key и не EndDict. А также если за вызовом StartDict следует не Key и не EndDict.
class DictItemContext final : public BaseContext {

public:
    
    explicit DictItemContext(Builder& builder) 
    : BaseContext(builder) {}

    ~DictItemContext() = default;

    BaseContext Value(Node::Value value) = delete;
    DictItemContext StartDict() = delete;    
    ArrayItemContext StartArray() = delete;
    BaseContext EndArray() = delete;
    Node Build() = delete;


}; 


// Класс-наследник от Base
// Обеспечение ошибок компиляции, если после StartArray следует
// не Value, не StartDict, не StartArray и не EndArray. 
// Аналогично после Value для массива должны слетдовать те же методы
class ArrayItemContext : public BaseContext {
public:
    explicit ArrayItemContext(Builder& builder) 
    : BaseContext(builder) {}
    
    ~ArrayItemContext() = default;

    // Только один вариант метода Value можете быть вызван 
    ArrayItemContext Value(Node::Value value);

    DictValueContext Key(std::string key) = delete;
    BaseContext EndDict() = delete;
    Node Build() = delete;

}; 

}  // namespace json

