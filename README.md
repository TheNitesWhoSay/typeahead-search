# Typeahead Search

Lightning fast type-ahead search for in-memory data-sets.

## Mission

1. Take a text data-set (up to 2^16 items/strings, 2gb characters) and make it searchable (cache built <1000ms)
2. Perform fast smart searches and make small update to the data-set in real-time (search or update <25ms)

## Search Logic

- Break the searchable data-set text and search string into lowercase tokens (by spaces, special characters, caret-position, and other criteria)
- Locate full & partial token-matches, identify token chains, match lengths & more and assign scores

## Search Scoring
| Description | Scoring |
| --- | --- |
| Full-token match chains | (chainTextLen)^2\*1,000,000 |
| Partial-token match chain-terminator | (chainTextLen)^2\*100,000\*searchTokenLen/itemTokenLen |
| Other full-token matches | (tokenLen)^2\*10,000 |
| Regular partial-token matches | (tokenLen)^2\*1,000\*searchTokenLen/itemTokenLen |
| Partial-token matches that separately matched as full tokens | (tokenLen)^2\*100\*searchTokenLen/itemTokenLen |

## Search Cache Data

The search cache consists of four variables...
1. The searchable items
2. The tokens for the searchable items (not to be confused with the search tokens that search strings are broken into)
3. The token map (maps full-token hashes to token keys)
4. The token prefix map (maps token prefixes to token keys)

The search cache is designed to maximize the speed of both searching and incremental updates to search items.

Fast incremental updates means references to both tokens (to get to tokens from the token map & token prefix map) and items (to get from tokens to the owning items) must be stable.

To that end, both items and tokens are slot maps which provide stable keys, fast iteration, constant-time insertion/deletion, and reasonably efficient memory usage. 

The token map is a simple unordered_map of full-token hashes to token keys, and the token prefix map is an array of length 8 (representing prefix string byte lengths 1-8), and whose entries are a map from prefix values (leading string data converted to an unsigned integer) to tokens that have said prefix.

## Search Cache Build & Update

The search cache is built by calling .load with items (or incrementally with .item_added), .load will take in a vector of search-item strings and with each of them it will add an entry to items and tokenize the search-item string and add to tokens & token map. Prefixes are then loaded in bulk after all items have been loaded.

The search cache can be added to with .item_added which will take in a search-item string and the index at which it will be added, it will then add it to items, tokenize the search-item string and add to tokens, token-map and prefixes.

The search cache can be removed from with .item_removed which will remove the item and any tokens, token-map entries, and prefixes that are now not owned by any item.

The search cache can have an individual items text updated with .item_text_changed, which is approximately the same as .item_removed combined with .item_added (insofar as tokens are concerned) just the items entry isn't removed or added, only its base text changes.

Finally items can be moved to new indexes with .item_moved, this only changes the index you get for your results and would be appropriate if you performed an operation that changed search-item indexes (like a sort), or can be called manually if you don't specify auto-move to update indexes when calling .item_removed or .item_added, either specifying the auto-move flag or manually calling .item_moved may be appropriate depending on your use-case.

## TODO

- [Done] Make the search cache update-friendly (eliminate dependence on pointers that may be invalidated by updates)
- [Done] Make the actual cache update logic
- [Done] Document the cache data, cache-build logic, cache-update logic & search logic
- [Done] Create additional tokens around the caret-position (blinking cursor)
- Gather some example cases for special characters (e.g. \`\~\!\@\#\$\%\^\&\*\(\)\-\_\=\+\[\]\{\}\;\:\'\"\,\.\<\>\/\?), decide what search rankings should look like & how to achieve that
- Opt-in support for logic accounting for custom character codes (e.g. <02> which maps to a particular control character and partials thereof "<", "<0", "<02")
- Check that scoring for duplicated full-tokens in items is sensible
- Unit tests (correct search rankings)
- Performance tests (cache built <1000ms, search/update ops <25ms)
- Use performance tests to help guide investigation (e.g. how big of a data set can linear/regex approaches handle while still coming in under-time)
- Perhaps add a preference for rarer search tokens (e.g. if "the" occurs 100 times in the search set and "fae" occurs twice, a match on "fae" should be worth more than a match on "the") 
- Perhaps add a preference for matching tokens earlier in the item
- Perhaps support for different logic for small and large search sets (e.g. linear/regex search is doable for 200 strings, not necessarily 50,000 strings, so maybe pickup all partial matches in smaller cases not just prefix-partials)
- Perhaps recognize capital-letters following lower-case letters as a split for tokens

## Example cases

```
search: "two THREE four one"
rank: "One two three four" > "four FIVE One six" > "one" > "none"

search: "zerg dr e"
rank: "zerg drone" > "zerg zergling" > "zerg egg" > "zerg larva"

search: "zer"
rank: "zerg zergling" > "zerg larva"

search: "one two f"
rank: "one two five" > "one two six"
```
