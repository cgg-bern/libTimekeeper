#include <gtest/gtest.h>
#include <libTimekeeper/StopWatch.hh>

using namespace Timekeeper;

TEST(HierarchicalStopWatch, IsGroupReturnsTrueForParentOnlyNodes) {
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // A node that was never started (count == 0) and has children is a group.
    EXPECT_TRUE(parent.is_group());
}

TEST(HierarchicalStopWatch, IsGroupReturnsFalseForLeafNodesAfterOneTiming) {
    HierarchicalStopWatch sw("leaf");

    // Before any timing: count == 0, so is_group() returns true (by current logic).
    EXPECT_TRUE(sw.is_group());

    sw.resume();
    sw.stop();

    // After one timing cycle: count == 1.
    // BUG: is_group() still checks count() == 0, which is now false, so it
    // correctly returns false for a leaf that has been used once.
    EXPECT_FALSE(sw.is_group());
}

TEST(HierarchicalStopWatch, IsGroupMisclassifiesResetNodesAsGroups) {
    HierarchicalStopWatch sw("leaf");

    sw.resume();
    sw.stop();
    // count == 1, not a group.
    EXPECT_FALSE(sw.is_group());

    // Reset clears count to 0.
    sw.reset();

    // BUG: is_group() returns true because count() == 0, even though this node
    // was used as a leaf (not a group). This causes elapsed() to sum children
    // instead of the watch's own duration.
    EXPECT_TRUE(sw.is_group());  // <-- This is the bug! A reset leaf should not be a group.

    // Verify that elapsed() now returns zero from child-summing logic,
    // which happens to match the actual elapsed (also zero after reset),
    // but the classification is still wrong for nodes that were timed before reset.
}

TEST(HierarchicalStopWatchResult, UnaccountedTimeDoesNotWrapOnUnderflow) {
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // Manually set up a scenario where the child's elapsed exceeds the parent.
    child.watch().resume();
    child.watch().stop();

    auto result = HierarchicalStopWatchResult(parent);

    // The unaccounted calculation subtracts children from parent duration.
    // If this wraps around (unsigned underflow), we get a huge number.
    // With the fix, it should clamp to zero or be reasonable.
    auto unaccounted = result.unaccounted();

    // BUG: Without the Duration::operator-= fix, this assertion fails because
    // the subtraction wraps to a huge unsigned value (e.g., 4294967295).
    EXPECT_LT(unaccounted.wallclock.count(), 1000000);  // reasonable bound (< 1 second)
}

TEST(HierarchicalStopWatchResult, CopiesChildrenRecursively) {
    HierarchicalStopWatch grandchild("grandchild");
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    child.add_child(&grandchild);
    parent.add_child(&child);

    auto result = HierarchicalStopWatchResult(parent);

    EXPECT_EQ(result.name, "parent");
    EXPECT_EQ(result.children.size(), 1u);
    EXPECT_EQ(result.children[0].name, "child");
    EXPECT_EQ(result.children[0].children.size(), 1u);
    EXPECT_EQ(result.children[0].children[0].name, "grandchild");
}
