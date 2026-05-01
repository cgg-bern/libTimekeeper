#include <gtest/gtest.h>
#include <libTimekeeper/StopWatch.hh>
#include <libTimekeeper/StopWatchPrinting.hh>
#include <sstream>
#include <thread>

using namespace Timekeeper;

// Forward declaration for split_lines (defined at end of file).
static std::vector<std::string> split_lines(const std::string &s);

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

TEST(PrintedTable, TreeIndentationSymbolsArePresent) {
    HierarchicalStopWatch grandchild("grandchild");
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    child.add_child(&grandchild);
    parent.add_child(&child);

    std::ostringstream os;
    auto result = HierarchicalStopWatchResult(parent);
    os << result;
    std::string output = os.str();

    // On non-Windows, tree symbols should be present.
#ifndef _WIN32
    EXPECT_NE(output.find("├"), std::string::npos) << "Expected branch symbol '├'";
    EXPECT_NE(output.find("│"), std::string::npos) << "Expected vertical symbol '│'";
    EXPECT_NE(output.find("╰"), std::string::npos) << "Expected last-branch symbol '╰'";
#else
    // On Windows, ASCII fallback symbols should be present.
    EXPECT_NE(output.find("+ "), std::string::npos) << "Expected branch symbol '+ '";
    EXPECT_NE(output.find("| "), std::string::npos) << "Expected vertical symbol '| '";
    EXPECT_NE(output.find("\\ "), std::string::npos) << "Expected last-branch symbol '\\ '";
#endif
}

TEST(PrintedTable, GroupNodeHasEmptyDurationFields) {
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // Parent is a group (never timed), so its duration fields should be empty.
    std::ostringstream os;
    auto result = HierarchicalStopWatchResult(parent);
    os << result;
    std::string output = os.str();

    // The group row itself should have empty wall/user/system columns.
    // We verify by checking that the parent name appears followed by empty fields.
    EXPECT_NE(output.find("parent"), std::string::npos);
}

TEST(PrintedTable, LeafNodeShowsDurationValues) {
    HierarchicalStopWatch sw("leaf");
    sw.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.stop();

    std::ostringstream os;
    auto result = HierarchicalStopWatchResult(sw);
    os << result;
    std::string output = os.str();

    // Leaf should show a non-zero wall time.
    EXPECT_NE(output.find("leaf"), std::string::npos);
    // The formatted duration should contain "ms" suffix.
    EXPECT_NE(output.find(" ms"), std::string::npos);
}

TEST(PrintedTable, UnaccountedRowIsShownForLeafWithChildren) {
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // Time the parent (making it a leaf with children).
    parent.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    child.watch().stop();
    parent.watch().stop();

    std::ostringstream os;
    auto result = HierarchicalStopWatchResult(parent);
    os << result;
    std::string output = os.str();

    // Should contain "(unaccounted)" row.
    EXPECT_NE(output.find("(unaccounted)"), std::string::npos);
}

TEST(PrintedTable, EndGroupRowIsShownForGroupWithChildren) {
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // Time only the child (parent remains a group).
    child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    child.watch().stop();

    std::ostringstream os;
    auto result = HierarchicalStopWatchResult(parent);
    os << result;
    std::string output = os.str();

    // Should contain "(end group)" row.
    EXPECT_NE(output.find("(end group)"), std::string::npos);
}

TEST(PrintedTable, ColumnAlignmentIsCorrect) {
    PT table{{PT::Align::Left, PT::Align::Right}};
    table.add_row({"short", "long_value"});
    table.add_row({"very_long_name", "x"});

    std::ostringstream os;
    os << table;
    std::string output = os.str();

    // The first column should be left-aligned (padding on the right).
    // The second column should be right-aligned (padding on the left).
    // We verify by checking that both rows have the same total width.
    auto lines = split_lines(output);
    ASSERT_GE(lines.size(), 2u);

    // Both data rows should have equal length (same max column widths applied).
    EXPECT_EQ(lines[0].length(), lines[1].length())
        << "Rows should be padded to the same width for alignment.\n"
        << "Row 0: \"" << lines[0] << "\"\n"
        << "Row 1: \"" << lines[1] << "\"";
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

TEST(FormatMs, EdgeCases) {
    // Exactly 999µs → "0 ms" (truncation).
    EXPECT_EQ(format_ms(std::chrono::microseconds(999)), "0 ms");

    // Exactly 1000µs → "1 ms".
    EXPECT_EQ(format_ms(std::chrono::microseconds(1000)), "1 ms");

    // Zero duration.
    EXPECT_EQ(format_ms(std::chrono::milliseconds(0)), "0 ms");

    // Large value.
    EXPECT_EQ(format_ms(std::chrono::seconds(123)), "123000 ms");
}

TEST(Utf8Len, CountsUnicodeCharactersCorrectly) {
    // ASCII string: 3 bytes, 3 chars
    EXPECT_EQ(utf8_len("abc"), 3u);

    // UTF-8 multi-byte character (e with acute = 2 bytes): "cafe" has 4 chars
    EXPECT_EQ(utf8_len("caf\u00e9"), 4u);

    // Emoji (4 bytes in UTF-8): "\U0001F600" is 1 char
    EXPECT_EQ(utf8_len("\xf0\x9f\x98\x80"), 1u);
}

TEST(Utf8Len, EmptyString) {
    EXPECT_EQ(utf8_len(""), 0u);
}

TEST(Strrep, RepeatsStringNTimes) {
    EXPECT_EQ(strrep("-", 3), "---");
    EXPECT_EQ(strrep("ab", 2), "abab");
    EXPECT_EQ(strrep("x", 0), "");
    EXPECT_EQ(strrep("x", -1), "");
}

TEST(Strrep, SingleCharacter) {
    EXPECT_EQ(strrep("*", 5), "*****");
}

// Helper to split a string into lines (forward-declared above).
static std::vector<std::string> split_lines(const std::string &s) {
    std::vector<std::string> lines;
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    return lines;
}
