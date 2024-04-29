#pragma once

#include <iosfwd>
#include <string_view>

#include "transport_catalogue.h"

namespace request_processing {

struct RequestDescription {
    // Определяет, задана ли команда (поле command непустое)
    explicit operator bool() const {
        return !request.empty();
    }

    bool operator!() const {
        return !operator bool();
    }

    std::string request;      // Название команды
    std::string id;           // id маршрута или остановки
    // std::string description;  // Параметры команды
};


RequestDescription ParseRequestDescription(std::string_view line);


void ParseAndPrintStat(const transport::TransportCatalogue& tansport_catalogue, std::string_view request,
                       std::ostream& output);



}  // namespace request_processing