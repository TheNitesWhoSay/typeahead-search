#pragma once
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
            std::int32_t op_buffer_size = utf8_input_size*2+1;
            std::string utf8_lcase(' ', static_cast<std::size_t>(op_buffer_size));
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

    struct strings
    {
        struct item_type
        {
            std::string regular = "";
            std::string lowercase = "";
            std::size_t token_count = 0;
            std::size_t index = 0;
        };
        std::vector<item_type> items;

        struct token_type
        {
            struct owner
            {
                std::uint32_t item_index = 0; // Index of searchable items
                std::uint32_t item_token_index = 0; // Index of this token within the searchable item
            };

            std::string_view text = "";
            std::vector<owner> owners {};
        };

        // TODO: pull tokens out of the map and into a list or vec<ptr> for stability when updating the map
        std::unordered_multimap<std::size_t, token_type> token_map {}; // item_token_hash -> token

        static constexpr std::hash<std::string_view> get_hash {};

        std::array<std::unordered_map<std::size_t, std::vector<const token_type*>>, max_prefix_size> prefix_lookup {}; // [i] = prefixes of length i+1

        void load_prefixes()
        {
            for ( const auto & entry : token_map )
            {
                const token_type* token = &entry.second;
                const std::string_view & token_text = token->text;
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
                            found->second.push_back(token);
                        else
                            prefix_map.emplace(prefix, std::vector<const token_type*>{token});
                    }
                }
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
                    const token_type* first_token = tokens.front();
                    std::string_view prefix(&first_token->text[0], std::min(prefix_size, first_token->text.size()));
                    std::cout << "  \"" << prefix << "\": {\n";
                    for ( const token_type* token : tokens )
                    {
                        const auto & owners = token->owners;
                        std::cout << "    \"" << token->text << "\": { ";
                        bool first = true;
                        for ( const auto & owner : owners )
                        {
                            if ( first )
                            {
                                std::cout << "(" << owner.item_index << ":" << owner.item_token_index << ")";
                                first = false;
                            }
                            else
                                std::cout << ", (" << owner.item_index << ":" << owner.item_token_index << ")";
                        }
                        std::cout << " },\n";
                    }
                    std::cout << "  },\n";
                }
                std::cout << "}\n";
            }*/
        }

        void load(const std::vector<std::string> & new_items)
        {
            std::size_t item_count = static_cast<std::size_t>(new_items.size());
            this->items.assign(item_count, {});
            icux::case_converter case_conv {};
            for ( std::size_t i=0; i<item_count; ++i )
            {
                this->items[i].regular = new_items[i];
                this->items[i].lowercase = case_conv.to_lower(new_items[i]);
                this->items[i].index = i;
            }

            this->token_map = {};

            for ( std::uint32_t i=0; i<item_count; ++i )
            {
                std::uint32_t token_index = 0;
                std::string_view item = this->items[i].lowercase;
                this->items[i].token_count = 0;
                for ( auto token : tokenize(item) )
                {
                    ++(this->items[i].token_count);
                    auto text = std::string_view(token.begin(), token.end());
                    std::size_t hash = strings::get_hash(text);

                    bool found_matching_token = false;
                    auto found = this->token_map.equal_range(hash);
                    if ( found.first != this->token_map.end() )
                    {
                        for ( auto it = found.first; it != found.second; ++it )
                        {
                            if ( it->second.text == text )
                            {
                                it->second.owners.emplace_back(i, token_index);
                                found_matching_token = true;
                            }
                        }
                    }

                    if ( !found_matching_token )
                        this->token_map.emplace(hash, token_type {.text = text, .owners = {{ .item_index = i, .item_token_index = token_index }} });

                    ++token_index;
                }
            }

            // TODO: remove debug prints
            /*std::cout << "search_set: [\n";
            for ( std::size_t i=0; i<item_count; ++ i)
                std::cout << "  " << i << ": \"" << this->items[i].regular << "\"\n";
            std::cout << "]\n";

            std::cout << "\ntoken_map: { // (item_index:item_token_index)\n";
            for ( const auto & entry : token_map )
            {
                std::cout << "  \"" << entry.second.text << "\": { ";
                bool first = true;
                for ( const auto & owner : entry.second.owners )
                {
                    if ( first )
                    {
                        std::cout << "(" << owner.item_index << ":" << owner.item_token_index << ")";
                        first = false;
                    }
                    else
                        std::cout << ", (" << owner.item_index << ":" << owner.item_token_index << ")";
                }
                std::cout << " }\n";
            }
            std::cout << "}\n";*/

            load_prefixes();
        }

        void search_for(std::string_view text)
        {
            std::vector<item_type> search_tokens {};
            icux::case_converter case_conv {};
            for ( auto token : tokenize(text) )
            {
                auto & new_item = search_tokens.emplace_back();
                new_item.regular = std::string(token.begin(), token.end());
                new_item.lowercase = case_conv.to_lower(new_item.regular);
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
            std::vector<std::unordered_map<std::size_t, std::vector<search_match>>> search_token_matches {}; // [search_token_index](item_index->search_match)
            search_token_matches.assign(search_tokens.size(), {});
            std::size_t search_token_count = search_tokens.size();
            for ( std::size_t i=0; i<search_token_count; ++i )
            {
                const auto & item = search_tokens[i];
                auto found = this->token_map.equal_range(get_hash(item.lowercase));
                for ( auto it = found.first; it != found.second; ++it )
                {
                    if ( item.lowercase != it->second.text )
                        continue;

                    for ( const auto & owner : it->second.owners )
                    {
                        std::size_t streak_count = 0;
                        if ( i > 0 && owner.item_token_index > 0 )
                        {
                            std::size_t prev_item_token_index = owner.item_token_index-1;
                            for ( std::size_t j=i; j>0; --j )
                            {
                                auto & prev_matches = search_token_matches[j-1];
                                auto prev_match = prev_matches.find(owner.item_index);
                                if ( prev_match != prev_matches.end() && prev_match->second[0].is_full() && prev_match->second[0].item_token_index == prev_item_token_index )
                                {
                                    ++streak_count;
                                    --prev_item_token_index;
                                }
                                else
                                    break;
                            }
                        }

                        search_token_matches[i].emplace(owner.item_index, std::vector<search_match> {
                            search_match{.item_token_index = owner.item_token_index, .streak_count = streak_count, .partial_match_length = 0, .item_token_length = it->second.text.size() }});
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
                    for ( const token_type* matching_token : matching_tokens )
                    {
                        if ( matching_token->text.starts_with(search_token_text) )
                        {
                            const auto & owners = matching_token->owners;
                            for ( const auto & owner : owners )
                            {
                                auto existing_match = search_token_matches[i].find(owner.item_index);
                                if ( existing_match != search_token_matches[i].end() )
                                {
                                    existing_match->second.emplace_back(
                                        search_match{.item_token_index = owner.item_token_index, .streak_count = 0, .partial_match_length = prefix_size, .item_token_length = matching_token->text.size()});
                                }
                                else
                                {
                                    search_token_matches[i].emplace(owner.item_index, std::vector<search_match> {
                                        search_match{.item_token_index = owner.item_token_index, .streak_count = 0, .partial_match_length = prefix_size, .item_token_length = matching_token->text.size()}});
                                }
                            }
                        }
                    }
                }
            }

            struct score_contributor : search_match {
                std::size_t score = 0;
            };

            std::vector<std::size_t> search_scores {};
            std::vector<std::vector<score_contributor>> search_score_contributors {};
            search_scores.assign(this->items.size(), 0);
            search_score_contributors.assign(this->items.size(), {});
            //std::cout << "\nmatches_found: { // (item_index:item_token_index/partial_match_len) ^ streak_count\n";
            for ( std::size_t i=0; i<search_token_count; ++i )
            {
                auto & search_token = search_tokens[i];
                auto & matches = search_token_matches[i]; // For a particular search token, these are the matches
                //std::cout << "  \"" << search_token.lowercase << "\": { ";
                bool first = true;
                for ( const auto & match_set : matches )
                {
                    std::size_t item_index = match_set.first;
                    for ( const auto & match : match_set.second )
                    {
                        std::size_t match_score = 0;
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
                            if ( i > 0 && item_index > 0 )
                            {
                                auto & prev_token_matches = search_token_matches[i-1];
                                auto found_prev = prev_token_matches.find(item_index-1);
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

                        search_scores[item_index] += match_score;
                        search_score_contributors[item_index].push_back({{match}, match_score});

                        /*if ( first )
                            first = false;
                        else
                            std::cout << ", ";
                        
                        std::cout << "(" << item_index << ":" << match.item_token_index;
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
            for ( auto & item : search_tokens )
                std::cout << "[" << item.lowercase << "]";
            std::cout << "\n";

            struct scored_result_index
            {
                std::size_t score = 0;
                std::size_t index = 0;
            };
            
            std::vector<scored_result_index> results {};
            results.assign(items.size(), {});
            for ( std::size_t i=0; i<items.size(); ++i )
            {
                results[i].index = i;
                results[i].score = search_scores[i];
            }
            std::sort(results.begin(), results.end(), [](const scored_result_index & lhs, const scored_result_index & rhs) {
                return lhs.score > rhs.score || (lhs.score == rhs.score && lhs.index < rhs.index);
            });

            std::cout << "\nsearch_scores: [\n";
            std::size_t index = 0;
            for ( const auto & result : results )
            {
                ++index;
                if ( index == 15 )
                {
                    std::cout << "  ...\n";
                    break;
                }
                if ( result.score > 0 || text.empty() )
                {
                    std::size_t i = result.index;
                    std::cout << "  " << i << ": \"" << this->items[i].regular << "\" --> " << search_scores[i] << "  // ";
                    auto tokens = tokenize(this->items[i].lowercase);
                    std::sort(search_score_contributors[i].begin(), search_score_contributors[i].end(),
                        [](const score_contributor & lhs, const score_contributor & rhs) { return lhs.score > rhs.score; });
                    for ( const auto & match : search_score_contributors[i] )
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
                else
                    break;
            }

            std::cout << "]\n\n";

            std::cout << '\n';
        }
    };

}
