#include <gtest/gtest.h>
#include <libTimekeeper/StopWatch.hh>
#include <libTimekeeper/json.hh>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

using namespace Timekeeper;

TEST(JsonDuration, SerializesToMicroseconds) {
    Duration d{std::chrono::microseconds(100), std::chrono::microseconds(50), std::chrono::microseconds(25)};
    nlohmann::json j = d;

    EXPECT_EQ(j["wall_us"], 100);
    EXPECT_EQ(j["user_us"], 50);
    EXPECT_EQ(j["system_us"], 25);
}

TEST(JsonDuration, ZeroSerializesCorrectly) {
    Duration d = Duration::zero();
    nlohmann::json j = d;

    EXPECT_EQ(j["wall_us"], 0);
    EXPECT_EQ(j["user_us"], 0);
    EXPECT_EQ(j["system_us"], 0);
}

TEST(JsonDuration, ProducesValidJson) {
    Duration d{std::chrono::microseconds(1234), std::chrono::microseconds(567), std::chrono::microseconds(89)};
    nlohmann::json j = d;

    // Verify the JSON is well-formed.
    std::string json_str = j.dump();
    EXPECT_FALSE(json_str.empty());

    auto parsed = nlohmann::json::parse(json_str);
    EXPECT_EQ(parsed["wall_us"], 1234);
    EXPECT_EQ(parsed["user_us"], 567);
    EXPECT_EQ(parsed["system_us"], 89);
}

TEST(JsonDuration, LargeValuesSerializeCorrectly) {
    Duration d{std::chrono::microseconds(1'000'000), std::chrono::microseconds(500'000), std::chrono::microseconds(250'000)};
    nlohmann::json j = d;

    EXPECT_EQ(j["wall_us"], 1000000);
    EXPECT_EQ(j["user_us"], 500000);
    EXPECT_EQ(j["system_us"], 250000);
}

TEST(JsonDuration, FromJsonThrowsRuntimeError) {
    // Bug #6: from_json throws at runtime instead of compile-time error.
    nlohmann::json j = R"({"wall_us": 100, "user_us": 50, "system_us": 25})"_json;

    // This should throw a std::runtime_error.
    EXPECT_THROW(j.get<Duration>(), std::runtime_error);
}

TEST(JsonHierarchicalStopWatchResult, SerializesCount) {
    HierarchicalStopWatch sw("test");
    sw.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.stop();

    auto result = HierarchicalStopWatchResult(sw);
    nlohmann::json j = result;

    EXPECT_EQ(j["count"], 1u);
}

TEST(JsonHierarchicalStopWatchResult, SerializesDuration) {
    HierarchicalStopWatch sw("test");
    sw.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.stop();

    auto result = HierarchicalStopWatchResult(sw);
    nlohmann::json j = result;

    // Duration should be serialized as microseconds.
    EXPECT_TRUE(j.contains("duration"));
    EXPECT_EQ(j["duration"]["wall_us"], result.duration.wallclock.count());
}

TEST(JsonHierarchicalStopWatchResult, SerializesWithActualTimingData) {
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // Time the hierarchy.
    parent.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    child.watch().stop();

    parent.watch().stop();

    auto result = HierarchicalStopWatchResult(parent);
    nlohmann::json j = result;

    // Verify all expected keys exist.
    EXPECT_TRUE(j.contains("count"));
    EXPECT_TRUE(j.contains("duration"));
    EXPECT_TRUE(j["duration"].contains("wall_us"));
    EXPECT_TRUE(j["duration"].contains("user_us"));
    EXPECT_TRUE(j["duration"].contains("system_us"));

    // Verify the JSON is well-formed.
    std::string json_str = j.dump();
    EXPECT_FALSE(json_str.empty());

    auto parsed = nlohmann::json::parse(json_str);
    EXPECT_EQ(parsed["count"], 1u);
    EXPECT_GT(parsed["duration"]["wall_us"], 0);
}

TEST(JsonHierarchicalStopWatchResult, SerializesChildrenAsObject) {
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    auto result = HierarchicalStopWatchResult(parent);
    nlohmann::json j = result;

    // Children are serialized as an object keyed by name.
    EXPECT_TRUE(j.contains("children"));
    EXPECT_TRUE(j["children"].contains("child"));
}

TEST(JsonHierarchicalStopWatchResult, SerializesNestedChildren) {
    HierarchicalStopWatch grandchild("grandchild");
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    child.add_child(&grandchild);
    parent.add_child(&child);

    auto result = HierarchicalStopWatchResult(parent);
    nlohmann::json j = result;

    // Verify nested structure is preserved.
    EXPECT_TRUE(j.contains("children"));
    EXPECT_TRUE(j["children"].contains("child"));
    EXPECT_TRUE(j["children"]["child"].contains("children"));
    EXPECT_TRUE(j["children"]["child"]["children"].contains("grandchild"));
}

TEST(JsonHierarchicalStopWatchResult, ProducesValidJson) {
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // Time both.
    parent.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    child.watch().stop();
    parent.watch().stop();

    auto result = HierarchicalStopWatchResult(parent);
    nlohmann::json j = result;

    // Verify the JSON is well-formed by round-tripping through to_string.
    std::string json_str = j.dump();
    EXPECT_FALSE(json_str.empty());

    // Parse it back and verify structure.
    auto parsed = nlohmann::json::parse(json_str);
    EXPECT_TRUE(parsed.contains("count"));
    EXPECT_TRUE(parsed.contains("duration"));
    EXPECT_TRUE(parsed["duration"].contains("wall_us"));
    EXPECT_TRUE(parsed["duration"].contains("user_us"));
    EXPECT_TRUE(parsed["duration"].contains("system_us"));
}

TEST(JsonHierarchicalStopWatchResult, GroupSerializesWithChildrenOnly) {
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // Time only the child (parent remains a group).
    child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    child.watch().stop();

    auto result = HierarchicalStopWatchResult(parent);
    nlohmann::json j = result;

    // Groups have count == 0 (parent's own watch was never started).
    EXPECT_EQ(j["count"], 0u);
    // Duration for groups sums children, so it should be non-zero.
    EXPECT_GT(j["duration"]["wall_us"], 0);
    EXPECT_TRUE(j.contains("children"));
}

TEST(JsonHierarchicalStopWatchResult, DuplicateChildNamesOverwrite) {
    // Bug #5: duplicate child names silently overwrite in JSON serialization.
    HierarchicalStopWatch child1("dup");
    HierarchicalStopWatch child2("dup");  // same name!
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child1);
    parent.add_child(&child2);

    auto result = HierarchicalStopWatchResult(parent);
    nlohmann::json j = result;

    // Only one "dup" entry exists in the JSON object — the second overwrote the first.
    EXPECT_EQ(j["children"].size(), 1u)
        << "Bug #5: duplicate child names silently overwrite each other in JSON.";
}

TEST(JsonHierarchicalStopWatchResult, CountIsAccurate) {
    HierarchicalStopWatch sw("test");

    // Multiple resume/stop cycles should be reflected in count.
    for (int i = 0; i < 5; ++i) {
        sw.watch().resume();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        sw.watch().stop();
    }

    auto result = HierarchicalStopWatchResult(sw);
    nlohmann::json j = result;

    EXPECT_EQ(j["count"], 5u);
}

// Note: HierarchicalStopWatchResult has no default constructor,
// so from_json cannot be called on it at all — the type simply
// isn't deserializable. This is bug #6 in action.
// A compile-time static_assert would be preferable to a runtime throw.
