
#include "json.h"

#include <memory>
#include <optional>

namespace json {


class Builder {
private:
    // Классы для формирования ошибок компиляции 
    // при некорректной последовательности вызовов метоов Строителя

    class ArrayItemContext;
    class DictItemContext;
    class DictValueContext;
    class BaseContext;

public:

    Builder () {
        nodes_stack_.emplace_back(&root_);
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

    void ThrowOnEmptyNodeStack() const;

    void AddNewNodeValueToStack(Node::Value value, bool need_pop_back);


};


// Классы для формирования ошибок компиляции 
// при некорректной последовательности вызовов метоов Строителя

// Основной класс - класс со всеми методами Builder
class Builder::BaseContext {
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
class Builder::DictValueContext final : public BaseContext {
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
class Builder::DictItemContext final : public BaseContext {

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
class Builder::ArrayItemContext : public BaseContext {
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

