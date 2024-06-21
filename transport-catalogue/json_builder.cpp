#include "json_builder.h"

#include <iostream>

using namespace std::literals;
using namespace json;


void Builder::ThrowOnEmptyNodeStack() const {
    if (nodes_stack_.empty()) {
        throw std::logic_error("LOG err: There is NO any opened nodes to add value. Call Build!\n"s);
    }
}

/*
Проверка вызова Value, StartDict или StartArray где-либо, 
кроме как после конструктора, после Key или после предыдущего 
элемента массива. Если true, то можно выполнять команду.
*/
bool Builder::CheckCommandsOrderForValueAndStart() const {
    return ((commands_.back() != Commands::CONSTRUCTOR) 
        && (commands_.back() != Commands::KEY) 
        && (commands_.back() != Commands::ADD_TO_ARRAY)
        && (commands_.back() != Commands::START_ARRAY)
        && (!nodes_stack_.back()->IsArray()));
}

// Исключение при вызове метода Key снаружи словаря 
// или сразу после другого Key.
void Builder::CheckAndThrowForKeyUsing() const {
    if (opened_containers_.empty()) {
        throw std::logic_error("LOG err: to use Key method - There is NO opened containers\n"s);
    }
    // Сюда пришли, если есть открытые контейнеры => проверяем, что есть открытый Dict
    if (opened_containers_.back() != ContainerType::DICT) {
        throw std::logic_error("LOG err: to use Key method - Key methods is called out of Dict\n"s);
    }
    if (commands_.back() == Commands::KEY) {
        throw std::logic_error("LOG err: to use Key method - Key methods is called after another Key\n"s);
    }
    return;
}

// Исключения при вызове EndDict или EndArray в контексте другого контейнера.
void Builder::CheckForEndingDict() const {
    if (nodes_stack_.back()->IsArray()) {
        throw std::logic_error("EndDict failed: opened Array\n"s);
    }
    if (opened_containers_.back() != ContainerType::DICT) {
        throw std::logic_error("EndDict failed: Dict is not last opened\n"s);
    }
    if (!nodes_stack_.back()->IsDict()) {
        throw std::logic_error("EndDict failed: opened container is not a Dict\n"s);
    }
    return;
}

// Исключение при вызове EndDict или EndArray в контексте другого контейнера.
void Builder::CheckForEndingArray() const {
    if (nodes_stack_.back()->IsDict()) {
        throw std::logic_error("EndArray failed due to opened Dict\n"s);
    }
    if (opened_containers_.back() != ContainerType::ARRAY) {
        throw std::logic_error("EndArray failed due to Dict is not last opened\n"s);
    }
    if (!nodes_stack_.back()->IsArray()) {
        throw std::logic_error("EndArray failed: opened container is not an Array\n");
    }
    return;
}


//
void Builder::AddCurrentValueToPreviousOpenedNode() {
    // формируем временный узел
    Node::Value& cur_node = nodes_stack_.back()->GetVarValue();
    json::Node value_tmp; 
    if (std::holds_alternative<json::Dict>(cur_node)) {
        value_tmp = std::get<json::Dict>(nodes_stack_.back()->GetVarValue());
    }
    else if (std::holds_alternative<json::Array>(cur_node)) {
        value_tmp = std::get<json::Array>(nodes_stack_.back()->GetVarValue());
    }
    
    // убираем завершенный узел из стека
    nodes_stack_.pop_back();

    // если ПРЕДЫДУЩИЙ открытый узел - массив, то добавляем в него новый элемент
    if (nodes_stack_.back()->IsArray()) {
        std::get<json::Array>(nodes_stack_.back()->GetVarValue()).emplace_back(value_tmp);
    }
    // если ПРЕДЫДУЩИЙ открытый узел - словарь, то добавляем завершенный элемент в словарь
    else if (nodes_stack_.back()->IsDict()) {
        // Помещаем новое значение в словарь, при этом ключ - последний в стеке ключей
        std::string last_key = dict_keys_.back();  // получаем актуальный ключ
        std::get<json::Dict>(nodes_stack_.back()->GetVarValue()).emplace(last_key, value_tmp);
        // Удаляем ключ, так как по нему только что записали 
        dict_keys_.pop_back();
    }
    else {
        // Закрываем узел: записываем значение и удаляем из стека незавершенных
        nodes_stack_.back()->GetVarValue() = value_tmp.GetValue();
        nodes_stack_.pop_back();
    } 
    return;
}

Builder& Builder::Value(Node::Value value) {
    ThrowOnEmptyNodeStack();
    
    if (CheckCommandsOrderForValueAndStart()) {
        throw std::logic_error("Value methods is not followed Builder or Key or adding previous Array value"s);
    }
    // в случае словаря 
    if (nodes_stack_.back()->IsDict()) {
        // to do: кажется, тут лишние действия, возможно, надо брать ссылку из стека и туда emplace
        // Узнаем актуальный ключ
        std::string last_key = dict_keys_.back();
        // Берем ссылку на последний Node
        json::Dict& last_node_ref = std::get<json::Dict>(nodes_stack_.back()->GetVarValue());
        last_node_ref[last_key] = json::Node(std::move(value));

        // Обновляем последний элемент в стеке
        nodes_stack_.back()->GetVarValue() = last_node_ref ;

        // удаляем последний ключ из массива ключей
        dict_keys_.pop_back();
        // добавляем команду в стек
        commands_.push_back(Commands::ADD_TO_MAP);
    }
    else if (nodes_stack_.back()->IsArray()) {
        // Получаем ссылку на последний элемент-массив и добавляем новое значение
        // Node& array_ref = 
        std::get<json::Array>(nodes_stack_.back()->GetVarValue()).emplace_back(std::move(value));

        // Обновляем в стеке (а вдруг память перевыделится и указатель станет невалидным?)
        // nodes_stack_.back() = &array_ref;  //не скомпилировалось, 
        // добавляем команду в стек
        commands_.push_back(Commands::ADD_TO_ARRAY);
    }
    // Если никаких контейнеров не открывалось, то передача готового узла
    else if ((commands_.back() == Commands::CONSTRUCTOR) 
            || &root_ == nodes_stack_.back()) {        
        // записываем по указателю новое значение
        nodes_stack_.back()->GetVarValue() = std::move(value);
        // Удаляем указатель на этот узел из стека, так как он завершен
        nodes_stack_.pop_back();
        commands_.push_back(Commands::VALUE);
        
    }
    else {
        throw std::logic_error("Can't use method Value"s);
    }

    return *this;
}


DictItemContext Builder::StartDict() {
    
    // Исключение при вызове Value, StartDict или StartArray где-либо, 
    // кроме как после конструктора, после Key или после предыдущего 
    // элемента массива
    if (nodes_stack_.empty()) {
        throw std::logic_error("LOG err: from Value method - There is NO any opened nodes to add value"s);
    }
    if (CheckCommandsOrderForValueAndStart()) {
            throw std::logic_error("StartDict methods does not follow Builder or Key or adding previous Array value"s);
    }
    
    //to do: разобраться
    // создаем новый узел
    nodes_stack_.emplace_back(new Node{});
    // помещаем пустой словарь в новый узел
    nodes_stack_.back()->GetVarValue() = json::Dict{};
    
    // Записываем выполненную команду
    commands_.push_back(Commands::START_DICT);
    // Устанавливаем флаг открытия словаря
    opened_containers_.push_back(ContainerType::DICT);
    

    return DictItemContext(*this);
}
 


DictValueContext Builder::Key(std::string key) {
    // Исключение при вызове метода Key снаружи словаря 
    // или сразу после другого Key.
    CheckAndThrowForKeyUsing();
    // Добавляем ключ в массив ключей, 
    // при формировании словаря мы его оттуда вытащим
    dict_keys_.push_back(std::move(key));

    // Записываем выполненную команду
    commands_.push_back(Commands::KEY);

    return DictValueContext(*this);
}



ArrayItemContext Builder::StartArray() {
    
    // Исключение при вызове Value, StartDict или StartArray где-либо, 
    // кроме как после конструктора, после Key или после предыдущего 
    // элемента массива
    if (nodes_stack_.empty()) {
        throw std::logic_error("LOG err: from Value method - There is NO any opened nodes to add value"s);
    }
    if (CheckCommandsOrderForValueAndStart()) {
            throw std::logic_error("StartArray methods does not follow Builder or Key or adding previous Array value"s);
    }
    // Добавляем в стек новый пустой массив
    nodes_stack_.emplace_back(new Node{});
    nodes_stack_.back()->GetVarValue() = json::Array{};

    //  Устанавливаем флаг открытия массива
    opened_containers_.push_back(ContainerType::ARRAY);
    // Добавляем выполненную команду в словарь
    commands_.push_back(Commands::START_ARRAY);

    return ArrayItemContext(*this);
}



BaseContext Builder::EndDict() {
    // Проверяем, есть ли что завершать
    ThrowOnEmptyNodeStack();
    // Сюда зашли, если есть незавершенные контейнеры
    
    CheckForEndingDict();
    // Сюда дошли только если незавершенный контейнер - Dict
    
    // Убираем флаг открытого словаря
    opened_containers_.pop_back();

    // Записываем вызванную команду в стек
    commands_.push_back(Commands::END_DICT);

    AddCurrentValueToPreviousOpenedNode();

    return BaseContext(*this);
}



BaseContext Builder::EndArray() {
    // Проверяем, есть ли что завершать
    ThrowOnEmptyNodeStack();

    CheckForEndingArray();
    
    // Сюда зашли, если все хорошо
    // 1) удаляем флаг открытого массива из стека контекнеров
    opened_containers_.pop_back();
    // записываем команду в стек
    commands_.push_back(Commands::END_ARRAY);

    AddCurrentValueToPreviousOpenedNode();

    // на всякий случай
    return BaseContext(*this);
} 



Node Builder::Build() {
    if (commands_.back() == Commands::CONSTRUCTOR) {
        throw std::logic_error("Build failed: The Build method can not be called after Builder"s);
    }
    if (!opened_containers_.empty()) {
        throw std::logic_error("Build failed: There is some opened containers"s);
    }
    if (!nodes_stack_.empty() && nodes_stack_.back() != &root_) {
        throw std::logic_error("Build failed: There is some unfinished nodes in stack"s);
    }
    
    return root_;
}


// Реализация методов для контекста основного (BaseContext) 

BaseContext::BaseContext(Builder& builder) 
:builder_(builder) {}

BaseContext BaseContext::Value(Node::Value value) {
    return BaseContext(builder_.Value(value));
}


DictItemContext BaseContext::StartDict() {
    return builder_.StartDict();
} 


DictValueContext BaseContext::Key(std::string key) {
    // DictValueContext dict_context(builder_.Key(key));
    return builder_.Key(key);
}


ArrayItemContext BaseContext::StartArray() {
    return builder_.StartArray();
}


BaseContext BaseContext::EndDict() {
    return builder_.EndDict();
}

BaseContext BaseContext::EndArray(){
    return builder_.EndArray();
}

Node BaseContext::Build() {
    return builder_.Build();
}

Builder& BaseContext::GetBuilder() {
    return builder_;
}


// Реализация некоторых методов для контекста значения в словаре

DictValueContext::DictValueContext(Builder& builder) : BaseContext(builder) {}

// Реализация некоторых методов для контекста элемента словаря

DictItemContext DictValueContext::Value(Node::Value value) {
    DictItemContext res(GetBuilder().Value(value));
    return res;
}

// Реализация некоторых методов для контекста элемента массива
ArrayItemContext ArrayItemContext::Value(Node::Value value) {
    return ArrayItemContext(GetBuilder().Value(value));
}