#include <typeahead/search.h>
#include "example-data.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

int main()
{
    search::strings test {};
    test.load(example_data::sc_unit_names);
    //test.load({"Zerg Zergling", "Zerg Egg"});
    std::cout << "\n----------\n";
    while ( true )
    {
        std::string search_text {};
        std::cout << ": ";
        std::getline(std::cin, search_text);
        if ( search_text == "exit" )
            return 1;

        test.search_for(search_text);
        std::cin.clear();
    }
    return 0;
}
