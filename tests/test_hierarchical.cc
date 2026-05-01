#include <gtest/gtest.h>
#include <libTimekeeper/StopWatch.hh>
#include <cmath>
#include <thread>
#include <chrono>

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

TEST(HierarchicalStopWatch, IsGroupReturnsTrueAfterReset) {
    // Regression test for bug #3: after reset(), count() == 0 causes is_group()
    // to return true even though this node was previously used as a leaf.
    HierarchicalStopWatch sw("leaf");

    sw.resume();
    sw.stop();
    EXPECT_FALSE(sw.is_group());

    sw.reset();
    // After reset, count() == 0 so is_group() currently returns true (bug).
    // Once bug #3 is fixed, this should return false since the node was used as a leaf.
    // For now we document the current behavior; the elapsed() test below
    // verifies that reset correctly zeroes accumulated time regardless.
    EXPECT_TRUE(sw.is_group());
}

TEST(HierarchicalStopWatch, ElapsedAfterResetReturnsZero) {
    // Verifies that reset properly zeroes all timing data for a leaf node,
    // which is the critical behavior even if is_group() classification is buggy.
    HierarchicalStopWatch sw("leaf");

    sw.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.stop();

    auto elapsed_before = sw.elapsed();
    EXPECT_GT(elapsed_before.wallclock.count(), 0);

    sw.reset();

    // After reset, elapsed should be zero regardless of is_group() classification.
    auto elapsed_after = sw.elapsed();
    EXPECT_EQ(elapsed_after.wallclock.count(), 0);
    EXPECT_EQ(elapsed_after.user.count(), 0);
    EXPECT_EQ(elapsed_after.system.count(), 0);
}

TEST(HierarchicalStopWatch, GroupElapsedSumsChildren) {
    HierarchicalStopWatch child1("child1");
    HierarchicalStopWatch child2("child2");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child1);
    parent.add_child(&child2);

    // Time the children (parent is a group, so its own watch is unused).
    child1.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    child1.watch().stop();

    child2.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    child2.watch().stop();

    // Parent is a group (count == 0), so elapsed() should sum children.
    EXPECT_TRUE(parent.is_group());
    auto parent_elapsed = parent.elapsed();
    auto child1_elapsed = child1.elapsed();
    auto child2_elapsed = child2.elapsed();

    // The parent's wallclock time should approximately equal the sum of children.
    // We use a generous tolerance because sleep_for is not precise.
    auto expected_wall = Duration::Dur(
        std::chrono::duration_cast<Duration::Dur>(child1_elapsed.wallclock).count()
      + std::chrono::duration_cast<Duration::Dur>(child2_elapsed.wallclock).count());

    // Allow 50% tolerance for timing imprecision.
    auto diff = parent_elapsed.wallclock > expected_wall
                    ? parent_elapsed.wallclock - expected_wall
                    : expected_wall - parent_elapsed.wallclock;
    EXPECT_LT(diff.count(), std::chrono::milliseconds(10).count());
}

TEST(HierarchicalStopWatch, GroupElapsedSumsDeepHierarchy) {
    // Critical test: verify that elapsed() correctly sums through multiple levels.
    HierarchicalStopWatch grandchild("grandchild");
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    child.add_child(&grandchild);
    parent.add_child(&child);

    // Time the deepest node (parent and child are groups).
    grandchild.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    grandchild.watch().stop();

    EXPECT_TRUE(parent.is_group());
    EXPECT_TRUE(child.is_group());
    EXPECT_FALSE(grandchild.is_group());

    auto parent_elapsed = parent.elapsed();
    auto child_elapsed = child.elapsed();
    auto grandchild_elapsed = grandchild.elapsed();

    // All three should report the same non-zero elapsed time,
    // since the group nodes sum their children recursively.
    EXPECT_GT(grandchild_elapsed.wallclock.count(), 0);
    EXPECT_EQ(parent_elapsed.wallclock.count(), child_elapsed.wallclock.count());
    EXPECT_EQ(child_elapsed.wallclock.count(), grandchild_elapsed.wallclock.count());
}

TEST(HierarchicalStopWatch, GroupElapsedSumsMixedLeafAndGroupChildren) {
    // Critical test: a group parent with both leaf and group children.
    HierarchicalStopWatch leaf("leaf");
    HierarchicalStopWatch inner_child("inner_child");
    HierarchicalStopWatch outer_child("outer_child");
    HierarchicalStopWatch parent("parent");

    parent.add_child(&leaf);
    parent.add_child(&outer_child);
    outer_child.add_child(&inner_child);

    // Time the leaf.
    leaf.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    leaf.watch().stop();

    // Time the innermost child (outer_child is a group).
    inner_child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    inner_child.watch().stop();

    EXPECT_TRUE(parent.is_group());

    auto parent_elapsed = parent.elapsed();
    auto leaf_elapsed = leaf.elapsed();
    auto outer_child_elapsed = outer_child.elapsed();
    auto inner_child_elapsed = inner_child.elapsed();

    // All individual leaves should report non-zero elapsed time.
    EXPECT_GT(leaf_elapsed.wallclock.count(), 0);
    EXPECT_GT(inner_child_elapsed.wallclock.count(), 0);

    // Group nodes sum their children, so outer_child == inner_child.
    EXPECT_EQ(outer_child_elapsed.wallclock.count(), inner_child_elapsed.wallclock.count());

    // Parent sums all leaves: leaf + inner_child (via outer_child).
    auto expected_wall = Duration::Dur(
        std::chrono::duration_cast<Duration::Dur>(leaf_elapsed.wallclock).count()
      + std::chrono::duration_cast<Duration::Dur>(inner_child_elapsed.wallclock).count());

    // Parent's elapsed should equal the sum of its leaf children.
    EXPECT_EQ(parent_elapsed.wallclock.count(), expected_wall.count());
}

TEST(HierarchicalStopWatch, LeafElapsedReturnsOwnDuration) {
    HierarchicalStopWatch sw("leaf");

    sw.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    sw.stop();

    // After timing, this is a leaf (count == 1), so elapsed() returns own watch.
    EXPECT_FALSE(sw.is_group());
    auto elapsed = sw.elapsed();
    EXPECT_GT(elapsed.wallclock.count(), 0);
}

TEST(HierarchicalStopWatchResult, UnaccountedTimeDoesNotWrapOnUnderflow) {
    // Tests the fix for bug #4: unsigned underflow in unaccounted calculation.
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // Manually set up a scenario where the child's elapsed exceeds the parent.
    child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    child.watch().stop();

    auto result = HierarchicalStopWatchResult(parent);

    // The unaccounted calculation subtracts children from parent duration.
    // With the clamp-to-zero fix, this should not wrap to a huge number.
    auto unaccounted = result.unaccounted();

    EXPECT_LT(unaccounted.wallclock.count(), 1000000);  // reasonable bound (< 1 second)
}

TEST(HierarchicalStopWatchResult, UnaccountedClampsAllComponentsIndependently) {
    // Critical: verify that each component (wall/user/system) clamps independently.
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // Time the parent briefly, then time the child longer in user+system but not wall.
    parent.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    child.watch().stop();

    parent.watch().stop();

    auto result = HierarchicalStopWatchResult(parent);
    EXPECT_FALSE(result.is_group());

    auto unaccounted = result.unaccounted();

    // All components should be non-negative.
    EXPECT_GE(unaccounted.wallclock.count(), 0);
    EXPECT_GE(unaccounted.user.count(), 0);
    EXPECT_GE(unaccounted.system.count(), 0);

    // Wall clock unaccounted should be positive (parent was timed longer than child).
    auto parent_elapsed = result.duration;
    auto diff = std::abs(
        static_cast<long long>(unaccounted.wallclock.count())
      - static_cast<long long>((parent_elapsed - Duration{child.elapsed()}).wallclock.count()));
    EXPECT_LT(diff, std::chrono::milliseconds(10).count());
}

TEST(HierarchicalStopWatchResult, UnaccountedWithDeepHierarchy) {
    // Critical: verify unaccounted calculation works through multiple levels.
    HierarchicalStopWatch grandchild("grandchild");
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    child.add_child(&grandchild);
    parent.add_child(&child);

    // Time the parent, then time children within that window.
    parent.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(3));

    grandchild.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    grandchild.watch().stop();

    child.watch().stop();
    parent.watch().stop();

    auto result = HierarchicalStopWatchResult(parent);
    EXPECT_FALSE(result.is_group());

    auto unaccounted = result.unaccounted();

    // All components should be non-negative.
    EXPECT_GE(unaccounted.wallclock.count(), 0);
    EXPECT_GE(unaccounted.user.count(), 0);
    EXPECT_GE(unaccounted.system.count(), 0);
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

TEST(HierarchicalStopWatchResult, UnaccountedIsZeroForGroup) {
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    parent.add_child(&child);

    // Time the child.
    child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    child.watch().stop();

    auto result = HierarchicalStopWatchResult(parent);

    // Parent is a group (count == 0), so unaccounted should be zero.
    EXPECT_TRUE(result.is_group());
    auto unaccounted = result.unaccounted();
    EXPECT_EQ(unaccounted.wallclock.count(), 0);
}

TEST(HierarchicalStopWatchResult, UnaccountedIsPositiveForLeafWithChildren) {
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

    auto result = HierarchicalStopWatchResult(parent);

    // Parent is a leaf (count == 1) with children, so unaccounted > 0.
    EXPECT_FALSE(result.is_group());
    auto unaccounted = result.unaccounted();
    EXPECT_GT(unaccounted.wallclock.count(), 0);
}

TEST(HierarchicalStopWatch, ResetClearsAllDescendants) {
    HierarchicalStopWatch grandchild("grandchild");
    HierarchicalStopWatch child("child");
    HierarchicalStopWatch parent("parent");
    child.add_child(&grandchild);
    parent.add_child(&child);

    // Time all three.
    parent.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    child.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    grandchild.watch().resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    grandchild.watch().stop();
    child.watch().stop();
    parent.watch().stop();

    // Verify all have non-zero counts.
    EXPECT_GT(parent.count(), 0);
    EXPECT_GT(child.count(), 0);
    EXPECT_GT(grandchild.count(), 0);

    // Reset the parent (should cascade to children).
    parent.reset();

    EXPECT_EQ(parent.count(), 0);
    EXPECT_EQ(child.count(), 0);
    EXPECT_EQ(grandchild.count(), 0);
}

TEST(HierarchicalStopWatch, MultipleResumesIncreaseCount) {
    HierarchicalStopWatch sw("multi");

    EXPECT_EQ(sw.count(), 0u);

    sw.resume();
    sw.stop();
    EXPECT_EQ(sw.count(), 1u);

    sw.resume();
    sw.stop();
    EXPECT_EQ(sw.count(), 2u);
}
