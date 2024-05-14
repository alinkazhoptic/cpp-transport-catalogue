#include <iostream>
#include <sstream>
#include <string>

#include "input_reader.h"
#include "stat_reader.h"

using namespace std;


void TestGetDistance() {
    string input_text = "13\nStop Tolstopaltsevo: 55.611087, 37.20829, 3900m to Marushkino\n"s
    + "Stop Marushkino: 55.595884, 37.209755, 9900m to Rasskazovka, 100m to Marushkino\n"
    + "Bus 256: Biryulyovo Zapadnoye > Biryusinka > Universam > Biryulyovo Tovarnaya > Biryulyovo Passazhirskaya > Biryulyovo Zapadnoye\n" + 
    + "Bus 750: Tolstopaltsevo - Marushkino - Marushkino - Rasskazovka\n"
    + "Stop Rasskazovka: 55.632761, 37.333324, 9500m to Marushkino\n"
    + "Stop Biryulyovo Zapadnoye: 55.574371, 37.6517, 7500m to Rossoshanskaya ulitsa, 1800m to Biryusinka, 2400m to Universam\n" +
    + "Stop Biryusinka: 55.581065, 37.64839, 750m to Universam\n"
    + "Stop Universam: 55.587655, 37.645687, 5600m to Rossoshanskaya ulitsa, 900m to Biryulyovo Tovarnaya\n"
    + "Stop Biryulyovo Tovarnaya: 55.592028, 37.653656, 1300m to Biryulyovo Passazhirskaya\n"
    + "Stop Biryulyovo Passazhirskaya: 55.580999, 37.659164, 1200m to Biryulyovo Zapadnoye\n"
    + "Bus 828: Biryulyovo Zapadnoye > Universam > Rossoshanskaya ulitsa > Biryulyovo Zapadnoye\n"
    + "Stop Rossoshanskaya ulitsa: 55.595579, 37.605757\n"
    + "Stop Prazhskaya: 55.611678, 37.603831\n"
    + "6\n"
    + "Bus 256\n"
    + "Bus 750\n"
    + "Bus 751\n"
    + "Stop Samara\n"
    + "Stop Prazhskaya\n"
    + "Stop Biryulyovo Zapadnoye\n"s;

    istringstream input_stream(input_text);

    transport::TransportCatalogue catalogue;

    int base_request_count;
    input_stream >> base_request_count >> ws;

    {
        loading::InputReader reader;
        for (int i = 0; i < base_request_count; ++i) {
            string line;
            getline(input_stream, line);
            reader.ParseLine(line);
        }
        reader.ApplyCommands(catalogue);
    }

    int stat_request_count;
    input_stream >> stat_request_count >> ws;
    for (int i = 0; i < stat_request_count; ++i) {
        string line;
        getline(input_stream, line);
        request_processing::ParseAndPrintStat(catalogue, line, cout);
    }


    cout << "GetDistance testing:\n" << endl;
    string stop1 =  "Biryusinka"s;
    string stop2 =  "Universam"s;
    string stop3 = "Biryulyovo Zapadnoye"s;
    
    // тут должно вывести 750 m
    cout << "Distance between "s << stop1 << " and "s << stop2 << " = "s << catalogue.GetDistanceBetweenStops(stop1, stop2) << endl;
    // а тут 1800 m
    cout << "Distance between "s << stop1 << " and "s << stop3 << " = "s << catalogue.GetDistanceBetweenStops(stop1, stop3) << endl;


    return;

}


int main() {
    // TestGetDistance();

    transport::TransportCatalogue catalogue;

    int base_request_count;
    cin >> base_request_count >> ws;

    {
        loading::InputReader reader;
        for (int i = 0; i < base_request_count; ++i) {
            string line;
            getline(cin, line);
            reader.ParseLine(line);
        }
        reader.ApplyCommands(catalogue);
    }

    int stat_request_count;
    cin >> stat_request_count >> ws;
    for (int i = 0; i < stat_request_count; ++i) {
        string line;
        getline(cin, line);
        request_processing::ParseAndPrintStat(catalogue, line, cout);
    }

}