# Heuristic Search

Lightning fast type-ahead heuristic search for in-memory data-sets.

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

## TODO
- Make the search cache update-friendly (eliminate dependence on pointers that may be invalidated by updates)
- Document the cache data, cache-build logic, cache-update logic & search logic
- Create additional tokens around the caret-position (blinking cursor)
- Gather some example cases for special characters (e.g. \`\~\!\@\#\$\%\^\&\*\(\)\-\_\=\+\[\]\{\}\;\:\'\"\,\.\<\>\/\?), decide what search rankings should look like & how to achieve that
- Opt-in support for logic accounting for custom character codes (e.g. <02> which maps to a particular control character and partials thereof "<", "<0", "<02")
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
