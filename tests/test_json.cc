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

// Note: HierarchicalStopWatchResult has no default constructor,
// so from_json cannot be called on it at all — the type simply
// isn't deserializable. This is bug #6 in action.
// A compile-time static_assert would be preferable to a runtime throw.
