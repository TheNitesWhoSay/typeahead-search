#include <gtest/gtest.h>
#include <typeahead/search.h>

TEST(SearchTest, Rankings)
{
	std::vector<std::string> search_set {"One two three four", "four FIVE One six", "one", "none"};
	std::sort(search_set.begin(), search_set.end());
	std::size_t num_permutations = 0;
	do
	{
		search::strings test {};
		test.load(search_set);
		std::vector<std::size_t> results = test.search_for("two THREE four one");
		EXPECT_EQ(3, results.size());
		EXPECT_EQ("One two three four", search_set[results[0]]);
		EXPECT_EQ("four FIVE One six", search_set[results[1]]);
		EXPECT_EQ("one", search_set[results[2]]);
		++num_permutations;
	} while ( std::next_permutation(search_set.begin(), search_set.end()) );
	EXPECT_EQ(24, num_permutations);

	search_set = {"zerg zergling", "zerg larva"};
	std::sort(search_set.begin(), search_set.end());
	num_permutations = 0;
	do
	{
		search::strings test {};
		test.load(search_set);
		std::vector<std::size_t> results = test.search_for("zer");
		EXPECT_EQ(2, results.size());
		EXPECT_EQ("zerg zergling", search_set[results[0]]);
		EXPECT_EQ("zerg larva", search_set[results[1]]);
		++num_permutations;
	} while ( std::next_permutation(search_set.begin(), search_set.end()) );
	EXPECT_EQ(2, num_permutations);
	
	search_set = {"zerg drone", "zerg zergling", "zerg egg", "zerg larva"};
	std::sort(search_set.begin(), search_set.end());
	num_permutations = 0;
	do
	{
		search::strings test {};
		test.load(search_set);
		std::vector<std::size_t> results = test.search_for("zerg dr e");
		EXPECT_EQ(4, results.size());
		EXPECT_EQ("zerg drone", search_set[results[0]]);
		EXPECT_EQ("zerg zergling", search_set[results[1]]);
		EXPECT_EQ("zerg egg", search_set[results[2]]);
		EXPECT_EQ("zerg larva", search_set[results[3]]);
		++num_permutations;
	} while ( std::next_permutation(search_set.begin(), search_set.end()) );
	EXPECT_EQ(24, num_permutations);
	
	search_set = {"one two five", "one two six"};
	std::sort(search_set.begin(), search_set.end());
	num_permutations = 0;
	do
	{
		search::strings test {};
		test.load(search_set);
		std::vector<std::size_t> results = test.search_for("one two f");
		EXPECT_EQ(2, results.size());
		EXPECT_EQ("one two five", search_set[results[0]]);
		EXPECT_EQ("one two six", search_set[results[1]]);
		++num_permutations;
	} while ( std::next_permutation(search_set.begin(), search_set.end()) );
	EXPECT_EQ(2, num_permutations);
}
