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
        
        std::cout << "\nsearching for \"" << search_text << "\n";
        auto results = test.explain_search({.search_text = search_text});

        std::cout << "\nsearch_scores: [\n";
        for ( auto & result : results )
        {
            std::cout
                << "  " << result.index << ": \"" << test.item_at(result.index) << "\" --> "
                << result.score << "  // " << result.explanation << '\n';
        }
        std::cout << "]\n\n\n";
        std::cin.clear();
    }
    return 0;
}
