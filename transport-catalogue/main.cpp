#include "json_reader.h"
#include "map_renderer.h"

#include <iostream>


int main() {
    /*
     * Примерная структура программы:
     *
     * Считать JSON из stdin
     * Построить на его основе JSON базу данных транспортного справочника
     * Выполнить запросы к справочнику, находящиеся в массива "stat_requests", построив JSON-массив
     * с ответами Вывести в stdout ответы в виде JSON
     */

    JsonReader json_reader;
    // 0. Считываем json из потока ввода 
    json_reader.LoadJson(std::cin);
    // 1. Создаем справочник
    transport::TransportCatalogue catalogue;
    // 2. Создаем пустой отрисовщик
    renderer::MapRenderer renderer;
    // 3. Создаем перенаправитель запросов к базе 
    RequestHandler request_handler(catalogue, renderer);
    // 4. Заполняем справочник
    json_reader.ApplyCommands(catalogue);
    
    // 5. Обрабатываем запросы и выводим результат
    json_reader.ProcessRequestsAndGetResponse(request_handler);
    json_reader.PrintResponse(std::cout); 
    
    
    /* 
    // 6. Настраиваем отрисовщикs
    json_reader.ApplyRenderSettings(renderer);
    // 7. Рисуем в поток вывода
    request_handler.RenderMap(std::cout);
    */
}