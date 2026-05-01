#include <gtest/gtest.h>
#include <libTimekeeper/StopWatch.hh>
#include <libTimekeeper/StopWatchPrinting.hh>
#include <sstream>

using namespace Timekeeper;

TEST(PrintedTable, HeaderRowIsPrinted) {
    std::ostringstream os;
    PT table{{PT::Align::Left, PT::Align::Right, PT::Align::Right, PT::Align::Right, PT::Align::Right}};
    table.add_row({"", "Wall", "User", "System", "count"});

    os << table;
    std::string output = os.str();

    EXPECT_NE(output.find("Wall"), std::string::npos);
    EXPECT_NE(output.find("User"), std::string::npos);
    EXPECT_NE(output.find("System"), std::string::npos);
    EXPECT_NE(output.find("count"), std::string::npos);
}

TEST(PrintedTable, TreeStructureIsPrinted) {
    HierarchicalStopWatch child1("child1");
    HierarchicalStopWatch child2("child2");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child1);
    parent.add_child(&child2);

    std::ostringstream os;
    auto result = HierarchicalStopWatchResult(parent);
    os << result;
    std::string output = os.str();

    EXPECT_NE(output.find("parent"), std::string::npos);
    EXPECT_NE(output.find("child1"), std::string::npos);
    EXPECT_NE(output.find("child2"), std::string::npos);
}

TEST(FormatMs, ProducesStringWithSuffix) {
    auto ms = format_ms(std::chrono::milliseconds(42));
    EXPECT_EQ(ms, "42 ms");
}

TEST(FormatMs, TruncatesSubMillisecondToZero) {
    // This test documents the precision loss in format_ms.
    // A 500us duration prints as "0 ms".
    auto result = format_ms(std::chrono::microseconds(500));
    EXPECT_EQ(result, "0 ms");

    // And a 1500us duration truncates (not rounds) to "1 ms".
    auto result2 = format_ms(std::chrono::microseconds(1500));
    EXPECT_EQ(result2, "1 ms");
}

TEST(Utf8Len, CountsUnicodeCharactersCorrectly) {
    // ASCII string: 3 bytes, 3 chars
    EXPECT_EQ(utf8_len("abc"), 3u);

    // UTF-8 multi-byte character (e with acute = 2 bytes): "cafe" has 4 chars
    EXPECT_EQ(utf8_len("caf\u00e9"), 4u);

    // Emoji (4 bytes in UTF-8): "\U0001F600" is 1 char
    EXPECT_EQ(utf8_len("\xf0\x9f\x98\x80"), 1u);
}

TEST(Strrep, RepeatsStringNTimes) {
    EXPECT_EQ(strrep("-", 3), "---");
    EXPECT_EQ(strrep("ab", 2), "abab");
    EXPECT_EQ(strrep("x", 0), "");
    EXPECT_EQ(strrep("x", -1), "");
}
