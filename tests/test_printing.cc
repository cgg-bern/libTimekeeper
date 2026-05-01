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

TEST(PrintedTable, TreeStructureWithDepthIsPrintedCorrectly) {
    HierarchicalStopWatch grandchild("grandchild");
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    child.add_child(&grandchild);
    parent.add_child(&child);

    std::ostringstream os;
    auto result = HierarchicalStopWatchResult(parent);
    os << result;
    std::string output = os.str();

    // Verify all names appear in the output.
    EXPECT_NE(output.find("parent"), std::string::npos);
    EXPECT_NE(output.find("child"), std::string::npos);
    EXPECT_NE(output.find("grandchild"), std::string::npos);

    auto lines = split_lines(output);
    // Header + parent + child + grandchild + (end group) + (end group) = 6 lines minimum.
    ASSERT_GE(lines.size(), 5u);

#ifndef _WIN32
    // Find the line containing "child" — it should have a branch prefix.
    bool found_child_with_branch = false;
    bool found_grandchild_doubly_indented = false;
    for (const auto &line : lines) {
        if (line.find("child") != std::string::npos && line.find("grandchild") == std::string::npos) {
            EXPECT_NE(line.find("├"), std::string::npos)
                << "Child should be indented with branch symbol. Line: " << line;
            found_child_with_branch = true;
        }
        if (line.find("grandchild") != std::string::npos) {
            EXPECT_NE(line.find("│"), std::string::npos)
                << "Grandchild should have vertical prefix.";
            EXPECT_NE(line.find("├"), std::string::npos)
                << "Grandchild should be doubly-indented. Line: " << line;
            found_grandchild_doubly_indented = true;
        }
    }
    EXPECT_TRUE(found_child_with_branch);
    EXPECT_TRUE(found_grandchild_doubly_indented);
#else
    bool found_child_with_branch = false;
    bool found_grandchild_doubly_indented = false;
    for (const auto &line : lines) {
        if (line.find("child") != std::string::npos && line.find("grandchild") == std::string::npos) {
            EXPECT_NE(line.find("+ "), std::string::npos);
            found_child_with_branch = true;
        }
        if (line.find("grandchild") != std::string::npos) {
            EXPECT_NE(line.find("| "), std::string::npos);
            EXPECT_NE(line.find("+ "), std::string::npos);
            found_grandchild_doubly_indented = true;
        }
    }
    EXPECT_TRUE(found_child_with_branch);
    EXPECT_TRUE(found_grandchild_doubly_indented);
#endif
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

    auto lines = split_lines(output);
    ASSERT_GE(lines.size(), 2u);

    // Find the parent line (skip header).
    size_t parent_line_idx = 0;
    for (size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].find("parent") != std::string::npos &&
            lines[i].find("├") == std::string::npos &&
            lines[i].find("│") == std::string::npos &&
            lines[i].find("╰") == std::string::npos) {
            parent_line_idx = i;
            break;
        }
    }

    // The group row should not contain "ms" (no duration values).
    EXPECT_EQ(lines[parent_line_idx].find("ms"), std::string::npos)
        << "Group node should have empty duration fields.\nLine: " << lines[parent_line_idx];
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

TEST(FormatMs, VeryLargeValues) {
    // Hours of work should format correctly.
    auto h = std::chrono::hours(1);
    EXPECT_EQ(format_ms(h), "3600000 ms");

    // Days (edge case for very large numbers).
    auto d = std::chrono::hours(24);
    EXPECT_EQ(format_ms(d), "86400000 ms");
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
