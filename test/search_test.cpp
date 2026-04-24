#include <gtest/gtest.h>
#include <typeahead/search.h>

TEST(Icux, ToLowerCase)
{
	auto str = [](const std::u8string & input) {
		return std::string(input.begin(), input.end());
	};
	icux::case_converter cc {};
	EXPECT_EQ("", cc.to_lower(""));
	EXPECT_EQ("a", cc.to_lower("A"));
	EXPECT_EQ("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz",
		cc.to_lower("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"));
	EXPECT_EQ("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz",
		cc.to_lower("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"));
	EXPECT_EQ(str(u8"\u00E4\u00F6\u015F"),
		cc.to_lower(str(u8"\u00C4\u00D6\u015E")));
}

TEST(SearchHelpers, PrefixValue)
{
	EXPECT_EQ(0x41, search::prefix_value("A"));
	EXPECT_EQ(0x4141, search::prefix_value("AA"));
	EXPECT_EQ(0x4142, search::prefix_value("AB"));
	EXPECT_EQ(0x4241, search::prefix_value("BA"));
	EXPECT_EQ(0x414243, search::prefix_value("ABC"));
	EXPECT_EQ(0x41424344, search::prefix_value("ABCD"));
	if constexpr ( sizeof(size_t) <= 4 )
	{
		EXPECT_EQ(0x41424344, search::prefix_value("ABCDE"));
		EXPECT_EQ(0x41424344, search::prefix_value("ABCDEF"));
	}
	else if constexpr ( sizeof(size_t) >= 8 )
	{
		EXPECT_EQ(0x4142434445, search::prefix_value("ABCDE"));
		EXPECT_EQ(0x414243444546, search::prefix_value("ABCDEF"));
		EXPECT_EQ(0x41424344454647, search::prefix_value("ABCDEFG"));
		EXPECT_EQ(0x4142434445464748, search::prefix_value("ABCDEFGH"));
		EXPECT_EQ(0x4142434445464748, search::prefix_value("ABCDEFGHI"));
		EXPECT_EQ(0x4142434445464748, search::prefix_value("ABCDEFGHIJ"));
	}
}

TEST(SearchHelpers, Tokenize)
{
	auto test_tokenize = [](std::string_view input, const std::vector<std::string> & expected_tokens) {
		auto range = search::tokenize(input);
		std::vector<std::string> tokens {};
		for ( auto token : search::tokenize(input) )
			tokens.push_back(std::string(token.begin(), token.end()));

		EXPECT_EQ(expected_tokens, tokens);
	};
	
	test_tokenize("",
		std::vector<std::string>{});
	test_tokenize("a",
		std::vector<std::string>{"a"});
	test_tokenize("word",
		std::vector<std::string>{"word"});
	test_tokenize("two words",
		std::vector<std::string>{"two", "words"});
	test_tokenize("a few\rdifferent\ntypes\r\nof separators",
		std::vector<std::string>{"a", "few", "different", "types", "of", "separators"});
}

TEST(Search, ResultRankings)
{
	// Returns number of permutations searched/tested
	auto permute_and_search = [](const std::string & search_text,
		std::vector<std::string> search_set,
		const std::vector<std::string> & expected_results) -> std::size_t
	{
		std::sort(search_set.begin(), search_set.end());
		std::size_t num_permutations = 0;
		do
		{
			search::strings test {};
			test.load(search_set);
			std::vector<std::size_t> result_indexes = test.search_for(search_text);
			std::vector<std::string> results {};
			for ( auto index : result_indexes )
				results.push_back(search_set[index]);

			EXPECT_EQ(results, expected_results);
			++num_permutations;
		} while ( std::next_permutation(search_set.begin(), search_set.end()) );
		return num_permutations;
	};

	auto perms = permute_and_search(
		"two THREE four one",
		{"One two three four", "four FIVE One six", "one", "none"},
		{"One two three four", "four FIVE One six", "one"}
	);
	EXPECT_EQ(24, perms);

	perms = permute_and_search(
		"zer",
		{"zerg larva", "zerg zergling"},
		{"zerg zergling", "zerg larva"}
	);
	EXPECT_EQ(2, perms);

	perms = permute_and_search(
		"zerg dr e",
		{"zerg drone", "zerg egg", "zerg larva", "zerg zergling"},
		{"zerg drone", "zerg zergling", "zerg egg", "zerg larva"}
	);
	EXPECT_EQ(24, perms);

	perms = permute_and_search(
		"one two f",
		{"one two six", "one two five"},
		{"one two five", "one two six"}
	);
	EXPECT_EQ(2, perms);
}
