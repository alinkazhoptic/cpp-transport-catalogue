#include "json_builder.h"

#include <iostream>

using namespace std::literals;
using namespace json;


void Builder::ThrowOnEmptyNodeStack() const {
    if (nodes_stack_.empty()) {
        throw std::logic_error("LOG err: There is NO any opened nodes to add value. Call Build!\n"s);
    }
}


// Записывает значение в json и в стек: 
// для формирования новых пустых узлов поставить open_new_node = true
// для записи значения в существующие узла open_new_node = false
void Builder::AddNewNodeValueToStack(Node::Value value, bool open_new_node) {
    ThrowOnEmptyNodeStack();
    Node::Value& cur_node_value = nodes_stack_.back()->GetValue();

    if (nodes_stack_.back()->IsArray()) {
        // Получаем ссылку на последний элемент-массив и добавляем новое значение
        Node& added_node = std::get<json::Array>(cur_node_value).emplace_back(std::move(value));
        if (open_new_node) {
            nodes_stack_.push_back(&added_node);
        }
    }
    // Для элемента словаря или просто значения - добавляем и удаляем из списка текущих
    else {
        // для записи нового значения ячейка должна быть пустой
        if (!std::holds_alternative<std::nullptr_t>(cur_node_value)) {
            throw std::logic_error("The place for new value is not empty"s);
        }
        cur_node_value = std::move(value);
        if (!open_new_node) {
            nodes_stack_.pop_back();
        }
    }
}


Builder& Builder::Value(Node::Value value) {
    ThrowOnEmptyNodeStack();
    // Добавляем элемент
    AddNewNodeValueToStack(value, /*open_new*/ false);
    
    return *this;
}


Builder::DictItemContext Builder::StartDict() {
    // проверяем, что есть куда записывать
    ThrowOnEmptyNodeStack();
    AddNewNodeValueToStack(json::Dict{}, /*open new*/ true);

    return DictItemContext(*this);
}
 

Builder::DictValueContext Builder::Key(std::string key) {
    // проверяем, что есть куда писать новую пару ключ-значение
    ThrowOnEmptyNodeStack();
    
    Node::Value& cur_node_value = nodes_stack_.back()->GetValue();

    // проверяем, что открытый узел является словарем
    if (!std::holds_alternative<json::Dict>(cur_node_value)){
        throw std::logic_error("LOG err: In Key - Opened Node is not a Dict"s);
    }
    
    // "открываем" новые пустой узел = элемент словаря
    json::Dict& cur_dict = std::get<json::Dict>(cur_node_value);
    nodes_stack_.push_back(&cur_dict[key]);

    return DictValueContext(*this);
}


Builder::ArrayItemContext Builder::StartArray() {
    ThrowOnEmptyNodeStack();

    AddNewNodeValueToStack(json::Array{}, /*open new*/ true);

    return ArrayItemContext(*this);
}


Builder::BaseContext Builder::EndDict() {
    // Проверяем, есть ли что завершать
    ThrowOnEmptyNodeStack();

    // Сюда зашли, если есть незавершенные контейнеры
    // Проверяем, что отрыт именно словарь
    Node::Value& cur_node_value = nodes_stack_.back()->GetValue();
    if (!std::holds_alternative<Dict>(cur_node_value)) {
        throw std::logic_error("LOG err: In EndDict - Last opened Node is not a Dict"s);
    }
    // закрываем открытый словарь 
    nodes_stack_.pop_back();

    return BaseContext(*this);
}



Builder::BaseContext Builder::EndArray() {
    // Проверяем, есть ли что завершать
    ThrowOnEmptyNodeStack();
    // Сюда зашли, если есть незавершенные контейнеры

    // Проверяем, что отрыт массив
    Node::Value& cur_node_value = nodes_stack_.back()->GetValue();
    if (!std::holds_alternative<Array>(cur_node_value)) {
        throw std::logic_error("LOG err: In EndArray - Last opened Node is not an Array"s);
    }
    // закрываем открытый массив 
    nodes_stack_.pop_back();

    return BaseContext(*this);
}





Node Builder::Build() {
    if (!nodes_stack_.empty() && nodes_stack_.back() != &root_) {
        throw std::logic_error("Build failed: There is some unfinished nodes in stack"s);
    } 
    
    return root_;
}


// Реализация методов для контекста основного (BaseContext) 

Builder::BaseContext::BaseContext(Builder& builder) 
:builder_(builder) {}

Builder::BaseContext Builder::BaseContext::Value(Node::Value value) {
    return BaseContext(builder_.Value(std::move(value)));
}


Builder::DictItemContext Builder::BaseContext::StartDict() {
    return builder_.StartDict();
} 


Builder::DictValueContext Builder::BaseContext::Key(std::string key) {
    // DictValueContext dict_context(builder_.Key(key));
    return builder_.Key(std::move(key));
}


Builder::ArrayItemContext Builder::BaseContext::StartArray() {
    return builder_.StartArray();
}


Builder::BaseContext Builder::BaseContext::EndDict() {
    return builder_.EndDict();
}

Builder::BaseContext Builder::BaseContext::EndArray(){
    return builder_.EndArray();
}

Node Builder::BaseContext::Build() {
    return builder_.Build();
}

Builder& Builder::BaseContext::GetBuilder() {
    return builder_;
}


// Реализация некоторых методов для контекста значения в словаре

Builder::DictValueContext::DictValueContext(Builder& builder) : Builder::BaseContext(builder) {}

// Реализация некоторых методов для контекста элемента словаря

Builder::DictItemContext Builder::DictValueContext::Value(Node::Value value) {
    DictItemContext res(GetBuilder().Value(std::move(value)));
    return res;
}

// Реализация некоторых методов для контекста элемента массива
Builder::ArrayItemContext Builder::ArrayItemContext::Value(Node::Value value) {
    return ArrayItemContext(GetBuilder().Value(std::move(value)));
}