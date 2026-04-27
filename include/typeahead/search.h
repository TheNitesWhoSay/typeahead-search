#pragma once
#include <slotmap/slotmap.h>
#include <unicode/ustring.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <iostream> // TODO: remove

namespace icux
{
    class case_converter
    {
        std::unique_ptr<UChar[]> utf16_input_buffer = nullptr;
        std::unique_ptr<UChar[]> utf16_output_buffer = nullptr;
        std::size_t buffer_size = 0;

        void expand_buffers(std::size_t size)
        {
            if ( buffer_size < size )
            {
                utf16_input_buffer = std::make_unique<UChar[]>(size);
                utf16_output_buffer = std::make_unique<UChar[]>(size);
                buffer_size = size;
            }
        }

    public:
        std::string to_lower(const std::string & utf8_str)
        {
            std::int32_t utf8_input_size = static_cast<std::int32_t>(utf8_str.size());
            if ( utf8_input_size == 0 )
                return utf8_str;

            std::int32_t op_buffer_size = utf8_input_size*2+1;
            std::string utf8_lcase(static_cast<std::size_t>(op_buffer_size), '\0');
            expand_buffers(op_buffer_size);
            
            std::int32_t utf8_lcase_size = 0;
            std::int32_t utf16_string_size = 0;
            std::int32_t sub_count = 0;
            UChar32 sub = (UChar32)'?';
            UErrorCode error_code = UErrorCode::U_ZERO_ERROR;

            u_strFromUTF8WithSub(utf16_input_buffer.get(), buffer_size, &utf16_string_size, &utf8_str[0], utf8_input_size, sub, &sub_count, &error_code);
            utf16_string_size = u_strToLower(utf16_output_buffer.get(), utf16_string_size, utf16_input_buffer.get(), utf16_string_size, NULL, &error_code);
            u_strToUTF8(&utf8_lcase[0], buffer_size, &utf8_lcase_size, utf16_output_buffer.get(), utf16_string_size, &error_code);
            utf8_lcase.resize(utf8_lcase_size);
            return utf8_lcase;
        }
    };
}

namespace search
{
    static constexpr std::size_t max_prefix_size = sizeof(std::size_t);

    std::size_t prefix_value(std::string_view str) // TODO: something to strip special characters & maybe spacing before or during the function
    {
        std::size_t num = 0;
        char* num_ptr = reinterpret_cast<char*>(&num);
        const char* char_ptr = str.data();
        const std::size_t len = std::min(max_prefix_size, str.size());
        for ( std::size_t i=0; i<len; ++i )
            num_ptr[i] = char_ptr[len-1-i];

        return num;
    }

    auto tokenize(std::string_view text)
    {
        auto same_category = [](unsigned char lhs, unsigned char rhs) { return std::isspace(lhs) == std::isspace(rhs); };
        auto not_whitespace = [](auto chunk) { return !std::isspace(static_cast<unsigned char>(chunk[0])); };
        return text | std::views::chunk_by(same_category) | std::views::filter(not_whitespace);
    }

    bool is_token_start(std::string_view::size_type pos, std::string_view text)
    {
        return pos == 0 && (text.size() == 0 || !std::isspace(text[0])) || (pos > 0 && pos < text.size() && std::isspace(text[pos-1]));
    }

    std::string_view last_token_in(std::string_view text)
    {
        for ( std::ptrdiff_t i=static_cast<std::ptrdiff_t>(text.size())-1; i>=0; --i )
        {
            if ( std::isspace(text[i]) )
                return text.substr(i+1);
        }
        return text; // Entire string is a token
    }

    class strings
    {
        static constexpr std::hash<std::string_view> get_hash {};
        
        using item_key_type = std::uint64_t;
        using token_key_type = std::uint64_t;

        struct item_type
        {
            std::string base_text = "";
            std::uint32_t token_count = 0;
            std::size_t index = 0;
        };

        struct search_token_type : item_type
        {
            std::string lowercase = "";
        };

        struct token_type
        {
            struct owner
            {
                item_key_type item_key {}; // Index of searchable items
                std::uint32_t item_token_index = 0; // Index of this token within the searchable item

                constexpr bool operator==(const token_type::owner & other) { return item_key == other.item_key && item_token_index == other.item_token_index; }
            };

            std::string text {};
            std::vector<owner> owners {};
        };

        slot_map<item_type> items {};
        slot_map<token_type> tokens {};

        std::unordered_multimap<std::size_t, token_key_type> token_map {}; // map token hash -> token key

        // [i] = prefixes of length i+1, map prefix value (codepoints mapped to integer) -> vector of token keys
        std::array<std::unordered_map<std::size_t, std::vector<token_key_type>>, max_prefix_size> prefix_lookup {};

        void load_prefixes(token_key_type token_key, std::string_view token_text)
        {
            std::size_t token_text_size = token_text.size();
            if ( token_text_size > 1 ) // tokens of size 1 or 0 have no prefix
            {
                std::size_t token_max_prefix_size = std::min(max_prefix_size, token_text_size-1);
                for ( std::size_t i=0; i<token_max_prefix_size; ++i )
                {
                    std::size_t prefix_size = i+1;
                    auto & prefix_map = prefix_lookup[i];
                    std::size_t prefix = prefix_value(std::string_view(&token_text[0], prefix_size));
                    auto found = prefix_map.find(prefix);
                    if ( found != prefix_map.end() )
                        found->second.push_back(token_key);
                    else
                        prefix_map.emplace(prefix, std::vector<token_key_type>{token_key});
                }
            }
        }

        void load_prefixes()
        {
            const auto token_count = tokens.size();
            const auto & token_values = tokens.unordered_data();
            const auto & keys = tokens.data_keys();
            for ( std::size_t key_index=0; key_index<token_count; ++key_index)
            {
                const auto token_key = keys[key_index];
                const token_type & token = token_values[key_index];
                std::string_view token_text = token.text;
                load_prefixes(token_key, token_text);
            }

            /*for ( std::size_t i=0; i<max_prefix_size; ++i )
            {
                std::size_t prefix_size = i+1;
                const auto & prefix_map = prefix_lookup[i];
                if ( prefix_map.empty() )
                    continue;

                std::cout << "prefixes_" << prefix_size << " { \n";
                for ( const auto & entry : prefix_map )
                {
                    const auto & tokens = entry.second;
                    token_key_type first_token_key = tokens.front();
                    const token_type & first_token = this->tokens[first_token_key];
                    std::string_view prefix(&first_token.text[0], std::min(prefix_size, first_token.text.size()));
                    std::cout << "  \"" << prefix << "\": {\n";
                    for ( token_key_type token_key : tokens )
                    {
                        const token_type & token = this->tokens[token_key];
                        const auto & owners = token.owners;
                        std::cout << "    \"" << token.text << "\": { ";
                        bool first = true;
                        for ( const auto & owner : owners )
                        {
                            if ( first )
                            {
                                std::cout << "(" << owner.item_key << ":" << owner.item_token_index << ")";
                                first = false;
                            }
                            else
                                std::cout << ", (" << owner.item_key << ":" << owner.item_token_index << ")";
                        }
                        std::cout << " },\n";
                    }
                    std::cout << "  },\n";
                }
                std::cout << "}\n";
            }*/
        }

        template <bool Load_prefixes>
        void upsert_item_tokens(const std::string & item_text, item_key_type item_key, item_type & item, icux::case_converter & case_conv)
        {
            std::string item_lcase_storage = case_conv.to_lower(item_text);
            std::string_view item_lcase = item_lcase_storage;
            item.token_count = 0;
            for ( auto token : tokenize(item_lcase) )
            {
                std::uint32_t item_token_index = static_cast<std::uint32_t>(item.token_count);
                ++item.token_count;
                std::string_view token_text = std::string_view(token.begin(), token.end());
                std::size_t hash = strings::get_hash(token_text);

                bool found_matching_token = false;
                auto found = this->token_map.equal_range(hash);
                if ( found.first != this->token_map.end() )
                {
                    for ( auto it = found.first; it != found.second; ++it )
                    {
                        token_key_type token_key = it->second;
                        token_type & token = tokens[token_key];
                        if ( token_text == token.text )
                        {
                            token.owners.emplace_back(item_key, item_token_index);
                            found_matching_token = true;
                        }
                    }
                }

                if ( !found_matching_token )
                {
                    token_key_type token_key = this->tokens.push_back(token_type {
                        .text = std::string(token_text),
                        .owners = {token_type::owner{ .item_key = item_key, .item_token_index = item_token_index }}
                    });
                    this->token_map.emplace(hash, token_key);

                    if constexpr ( Load_prefixes )
                        load_prefixes(token_key, token_text);
                }
            }
        }

        void remove_item_tokens_by_key(item_key_type item_key)
        {
            const item_type & item = items[item_key];
            std::string item_lcase_storage = icux::case_converter{}.to_lower(item.base_text);
            std::string_view item_lcase = std::string_view(item_lcase_storage);
            
            std::uint32_t item_token_index = 0;
            for ( auto token_text_range : tokenize(item_lcase) )
            {
                std::string_view token_text(token_text_range);
                
                std::size_t hash = strings::get_hash(token_text);
                auto found = this->token_map.equal_range(hash);
                if ( found.first != this->token_map.end() )
                {
                    for ( auto it = found.first; it != found.second; ++it )
                    {
                        token_key_type token_key = it->second;
                        token_type & token = tokens[token_key];
                        if ( token_text != token.text )
                            continue; // Hash collision

                        auto found = std::find(token.owners.begin(), token.owners.end(),
                            token_type::owner{ .item_key = item_key, .item_token_index = item_token_index});
                        if ( found != token.owners.end() )
                        {
                            if ( token.owners.size() == 1 ) // This is the last owner, remove the entire token
                            {
                                // Remove the token prefixes...
                                std::size_t token_text_size = token_text.size();
                                if ( token_text_size > 1 ) // tokens of size 1 or 0 have no prefix
                                {
                                    std::size_t token_max_prefix_size = std::min(max_prefix_size, token_text_size-1);
                                    for ( std::size_t i=0; i<token_max_prefix_size; ++i )
                                    {
                                        std::size_t prefix_size = i+1;
                                        auto & prefix_map = prefix_lookup[i];
                                        std::size_t prefix = prefix_value(std::string_view(&token_text[0], prefix_size));
                                        auto found = prefix_map.find(prefix);
                                        if ( found != prefix_map.end() )
                                        {
                                            auto found_owner = std::find(found->second.begin(), found->second.end(), token_key);
                                            if ( found_owner != found->second.end() )
                                            {
                                                if ( found->second.size() == 1 ) // This is the last prefix token key, remove the prefix entirely
                                                    prefix_map.erase(found);
                                                else // This is not the last prefix owner, remove the prefix token key
                                                    found->second.erase(found_owner);
                                            }
                                        }
                                    }
                                }

                                // Remove the token_map and tokens entries
                                token_map.erase(it);
                                tokens.erase(token_key);
                            }
                            else // This is not the last owner of the token, remove this particular owner
                                token.owners.erase(found);
                        }
                        break;
                    }
                }
                ++item_token_index;
            }
        }

        void remove_item_by_key(item_key_type item_key)
        {
            remove_item_tokens_by_key(item_key);
            items.erase(item_key);
        }

        std::optional<item_key_type> get_item_key(std::size_t item_index)
        {
            const auto & unordered_items = items.unordered_data();
            std::size_t total_items = unordered_items.size();
            for ( std::size_t i=0; i<total_items; ++i )
            {
                const auto & item = unordered_items[i];
                if ( item.index == item_index )
                    return std::make_optional<item_key_type>(items.data_keys()[i]);
            }
            return std::nullopt;
        }

    public:
        void load(const std::vector<std::string> & new_items)
        {
            this->items.clear();
            this->tokens.clear();
            this->token_map.clear();

            std::size_t item_count = static_cast<std::size_t>(new_items.size());
            this->items.reserve(item_count);
            icux::case_converter case_conv {};
            std::size_t item_index = 0;
            for ( const std::string & item_text : new_items )
            {
                item_key_type item_key = this->items.push_back(item_type {
                    .base_text = item_text,
                    .index = item_index
                });
                item_type & item = this->items[item_key];

                upsert_item_tokens<false>(item_text, item_key, item, case_conv);
                ++item_index;
            }

            // TODO: remove debug prints
            /*std::cout << "search_set: [\n";
            const auto & item_keys = items.data_keys();
            const auto & item_values = items.unordered_data();
            for ( auto item_key : item_keys )
                std::cout << "  " << item_key << ": \"" << item_values[item_key].base_text << "\"\n";
            std::cout << "]\n";

            std::cout << "\ntoken_map: { // (item_index:item_token_index)\n";
            for ( const auto & entry : token_map )
            {
                token_key_type token_key = entry.second;
                const auto & token = this->tokens[token_key];
                std::cout << "  \"" << token.text << "\": { ";
                bool first = true;
                for ( const auto & owner : token.owners )
                {
                    if ( first )
                    {
                        std::cout << "(" << owner.item_key << ":" << owner.item_token_index << ")";
                        first = false;
                    }
                    else
                        std::cout << ", (" << owner.item_key << ":" << owner.item_token_index << ")";
                }
                std::cout << " }\n";
            }
            std::cout << "}\n";*/

            load_prefixes();
        }

        std::vector<std::size_t> search_for(std::string_view text, std::string_view::size_type caret_pos = std::string_view::npos)
        {
            std::vector<search_token_type> search_tokens {};
            icux::case_converter case_conv {};
            for ( auto token : tokenize(text) )
            {
                auto & new_item = search_tokens.emplace_back();
                new_item.base_text = std::string(token.begin(), token.end());
                new_item.lowercase = case_conv.to_lower(new_item.base_text);
            }
            if ( caret_pos != std::string_view::npos && caret_pos > 0 && text.size() >= 2 && caret_pos < text.size()-1 )
            {
                if ( !is_token_start(caret_pos, text) )
                {
                    std::string_view caret_token_text = last_token_in(text.substr(0, caret_pos));
                    auto & caret_item = search_tokens.emplace_back();
                    caret_item.base_text = std::string(caret_token_text);
                    caret_item.lowercase = case_conv.to_lower(caret_item.base_text);
                }
            }

            struct search_match {
                std::size_t item_token_index;
                std::size_t streak_count = 0;
                std::size_t partial_match_length = 0; // If 0, the full-token was matched
                std::size_t item_token_length = 0;
                
                constexpr bool is_full() const { return partial_match_length == 0; }
                constexpr bool is_partial() const { return partial_match_length > 0; }
            };

            // Search for full-token matches
            std::vector<std::unordered_map<item_key_type, std::vector<search_match>>> search_token_matches {}; // [search_token_index](item_iter->search_match)
            search_token_matches.assign(search_tokens.size(), {});
            std::size_t search_token_count = search_tokens.size();
            for ( std::size_t i=0; i<search_token_count; ++i )
            {
                const auto & search_token = search_tokens[i];
                auto found = this->token_map.equal_range(get_hash(search_token.lowercase));
                for ( auto it = found.first; it != found.second; ++it )
                {
                    token_key_type token_key = it->second;
                    const token_type & token = tokens[token_key];
                    if ( search_token.lowercase != token.text )
                        continue;

                    for ( const auto & owner : token.owners )
                    {
                        std::size_t streak_count = 0;
                        if ( i > 0 && owner.item_token_index > 0 )
                        {
                            std::size_t prev_item_token_index = owner.item_token_index-1;
                            for ( std::size_t j=i; j>0; --j )
                            {
                                auto & prev_matches = search_token_matches[j-1];
                                auto prev_match = prev_matches.find(owner.item_key);
                                if ( prev_match != prev_matches.end() && prev_match->second[0].is_full() && prev_match->second[0].item_token_index == prev_item_token_index )
                                {
                                    ++streak_count;
                                    --prev_item_token_index;
                                }
                                else
                                    break;
                            }
                        }

                        search_token_matches[i].emplace(owner.item_key, std::vector<search_match> {
                            search_match{
                                .item_token_index = owner.item_token_index,
                                .streak_count = streak_count,
                                .partial_match_length = 0,
                                .item_token_length = tokens[it->second].text.size()
                            }
                        });
                    }
                }
            }

            // Search for partial-matches
            for ( std::size_t i=0; i<search_token_count; ++i )
            {
                auto & search_token = search_tokens[i];
                const std::string & search_token_text = search_token.lowercase;
                std::size_t prefix_size = std::min(max_prefix_size, search_token_text.size());
                std::size_t prefix_value = search::prefix_value(search_token_text);
                const auto & prefix_map = prefix_lookup[prefix_size-1];
                const auto found = prefix_map.find(prefix_value);
                if ( found != prefix_map.end() )
                {
                    const auto & matching_tokens = found->second;
                    for ( const token_key_type matching_token_key : matching_tokens )
                    {
                        const token_type & matching_token = tokens[matching_token_key];
                        if ( matching_token.text.starts_with(search_token_text) )
                        {
                            const auto & owners = matching_token.owners;
                            for ( const auto & owner : owners )
                            {
                                auto existing_match = search_token_matches[i].find(owner.item_key);
                                if ( existing_match != search_token_matches[i].end() )
                                {
                                    existing_match->second.emplace_back(
                                        search_match{
                                            .item_token_index = owner.item_token_index,
                                            .streak_count = 0,
                                            .partial_match_length = prefix_size,
                                            .item_token_length = matching_token.text.size()
                                        }
                                    );
                                }
                                else
                                {
                                    search_token_matches[i].emplace(owner.item_key, std::vector<search_match> {
                                        search_match{
                                            .item_token_index = owner.item_token_index,
                                            .streak_count = 0,
                                            .partial_match_length = prefix_size,
                                            .item_token_length = matching_token.text.size()
                                        }
                                    });
                                }
                            }
                        }
                    }
                }
            }

            struct score_contributor : search_match {
                std::uint64_t score = 0;
            };

            struct search_result {
                std::uint64_t score = 0;
                std::vector<score_contributor> contributors {};
            };

            std::unordered_map<item_key_type, search_result> search_result_map {};
            //std::cout << "\nmatches_found: { // (item_index:item_token_index/partial_match_len) ^ streak_count\n";
            for ( std::size_t i=0; i<search_token_count; ++i )
            {
                auto & search_token = search_tokens[i];
                auto & matches = search_token_matches[i]; // For a particular search token, these are the matches
                //std::cout << "  \"" << search_token.lowercase << "\": { ";
                bool first = true;
                for ( const auto & match_set : matches )
                {
                    item_key_type item_key = match_set.first;
                    for ( const auto & match : match_set.second )
                    {
                        std::uint64_t match_score = 0;
                        if ( match.is_partial() ) // Partial-match
                        {
                            bool also_full_token = false; // This token matched this item previously as a full-search token
                            for ( const search_match & other_match : match_set.second )
                            {
                                if ( other_match.is_full() )
                                {
                                    also_full_token = true;
                                    break;
                                }
                            }

                            bool is_streak_ending = false; // This token came at the end of a streak of full-token matches
                            if ( i > 0 )
                            {
                                auto & prev_token_matches = search_token_matches[i-1];
                                auto found_prev = prev_token_matches.find(item_key);
                                is_streak_ending = (found_prev != prev_token_matches.end());
                            }

                            if ( also_full_token ) // Partial-token match which is a full-match for the same search item
                                match_score = match.partial_match_length*match.partial_match_length * 100 * match.partial_match_length / match.item_token_length;
                            else if ( is_streak_ending )
                                match_score = match.partial_match_length*match.partial_match_length * 100000 * match.partial_match_length / match.item_token_length;
                            else // Base partial-match
                                match_score = match.partial_match_length*match.partial_match_length * 1000 * match.partial_match_length / match.item_token_length;
                        }
                        else if ( match.streak_count > 0 ) // Full-token match chain
                        {
                            std::size_t total_streak_text_length = 0;
                            for ( std::size_t j=i-match.streak_count; j<=i; ++j )
                                total_streak_text_length += search_tokens[j].lowercase.size();

                            match_score = total_streak_text_length*total_streak_text_length * (match.streak_count+1) * 1000000;
                        }
                        else // Non-chain full-token match
                            match_score = search_token.lowercase.size()*search_token.lowercase.size() * 10000;

                        if ( match_score > 0 )
                        {
                            auto found = search_result_map.find(item_key);
                            if ( found != search_result_map.end() )
                            {
                                found->second.score += match_score;
                                found->second.contributors.push_back({{match}, match_score});
                            }
                            else
                            {
                                search_result_map.emplace(item_key, search_result {
                                    .score = match_score,
                                    .contributors = {{{match}, match_score}}
                                });
                            }
                        }

                        /*if ( first )
                            first = false;
                        else
                            std::cout << ", ";
                        
                        std::cout << "(" << item_key << ":" << match.item_token_index;
                        if ( match.partial_match_length > 0 )
                            std::cout << "/" << match.partial_match_length;

                        std::cout << ")";
                        if ( match.streak_count > 0 )
                            std::cout << "^" << (match.streak_count+1) << "";

                        std::cout << "+" << match_score;*/
                    }
                }
                //std::cout << " }\n";
            }
            //std::cout << "}\n";

            // TODO: remove debug prints
            std::cout << "\nsearching for \"" << text << "\" ";
            for ( auto & search_token : search_tokens )
                std::cout << "[" << search_token.lowercase << "]";
            std::cout << "\n";

            struct scored_result_index
            {
                std::uint64_t score = 0;
                std::size_t index = 0;
                const item_type* item = nullptr;
                decltype(search_result_map)::value_type* result = nullptr;
            };
            
            std::vector<scored_result_index> results {};
            for ( auto & search_result : search_result_map )
            {
                const item_type* item = &items[search_result.first];
                results.emplace_back(search_result.second.score, item->index, item, &search_result);
            }

            std::sort(results.begin(), results.end(), [](const scored_result_index & lhs, const scored_result_index & rhs) {
                return lhs.score > rhs.score || (lhs.score == rhs.score && lhs.index < rhs.index);
            });

            std::vector<std::size_t> result_indexes {};
            std::cout << "\nsearch_scores: [\n";
            std::size_t index = 0;
            for ( const auto & result : results )
            {
                ++index;
                result_indexes.push_back(result.index);
                if ( index == 15 )
                {
                    std::cout << "  ...\n";
                    break;
                }
                if ( result.score > 0 || text.empty() )
                {
                    const item_type & item = *result.item;
                    auto & contributors = result.result->second.contributors;
                    std::cout << "  " << result.index << ": \"" << item.base_text << "\" --> " << result.score << "  // ";
                    auto tokens = tokenize(item.base_text);
                    std::sort(contributors.begin(), contributors.end(),
                        [](const score_contributor & lhs, const score_contributor & rhs) { return lhs.score > rhs.score; });
                    for ( const auto & match : contributors )
                    {
                        auto token_it = std::ranges::next(std::ranges::begin(tokens), match.item_token_index);
                        auto token_text = std::string_view(std::begin(*token_it), std::end(*token_it));
                        if ( match.is_partial() )
                        {
                            std::cout << "\"" << std::string_view(&token_text[0], match.partial_match_length) << "\"/\"" << token_text << "\"";
                        }
                        else
                        {
                            if ( match.streak_count == 0 )
                                std::cout << "\"" << token_text << "\"";
                            else
                                std::cout << "\"" << token_text << "\"^" << match.streak_count+1;
                        }
                        std::cout << "+" << match.score << " ";
                    }
                    std::cout << "\n";
                }
            }

            std::cout << "]\n\n";

            std::cout << '\n';
            return result_indexes;
        }

        void item_text_changed(std::size_t index, const std::string & new_text)
        {
            if ( auto found_item_key = get_item_key(index) )
            {
                icux::case_converter case_conv {};
                item_key_type item_key = *found_item_key;
                remove_item_tokens_by_key(item_key);
                auto & item = items[item_key];
                item.base_text = new_text;
                upsert_item_tokens<true>(new_text, item_key, item, case_conv);
            }
        }

        // if Auto_move is set (not set by default), item indexes after the insertion index are incremented
        // if Auto_move is not set then you are expected to separately call item_moved to update the indexes
        template <bool Auto_move = false>
        void item_added(std::size_t index, const std::string & new_text)
        {
            if constexpr ( Auto_move )
            {
                for ( auto & item : items )
                {
                    if ( item.index >= index )
                        ++item.index;
                }
            }

            auto item_key = items.push_back(item_type{.base_text = new_text, .token_count = 0, .index = index});
            auto & item = items[item_key];

            icux::case_converter case_conv {};
            upsert_item_tokens<true>(new_text, item_key, item, case_conv);
        }

        // if Auto_move is set (not set by default), item indexes after the insertion index are decremented
        // if Auto_move is not set then you are expected to separately call item_moved to update the indexes
        template <bool Auto_move = false>
        void item_removed(std::size_t index)
        {
            if ( auto found_item_key = get_item_key(index) )
                remove_item_by_key(*found_item_key);
            else
                throw std::out_of_range("Item index given to item_removed was not present in the search cache!");

            if constexpr ( Auto_move )
            {
                for ( auto & item : items )
                {
                    if ( item.index > index )
                        --item.index;
                }
            }
        }

        // Call to indicate that the item previously at old_index has been moved to new_index
        // This will *only* update the item that presently has index "old_index" to have index "new_index", items will not be otherwise moved or changed
        void item_moved(std::size_t old_index, std::size_t new_index)
        {
            for ( auto & item : items )
            {
                if ( item.index = old_index )
                {
                    item.index = new_index;
                    break;
                }
            }
        }

    };

}
