#include <gtest/gtest.h>
#include <libTimekeeper/Duration.hh>

using namespace Timekeeper;

TEST(Duration, ZeroReturnsAllZero) {
    Duration d = Duration::zero();
    EXPECT_EQ(d.wallclock.count(), 0);
    EXPECT_EQ(d.user.count(), 0);
    EXPECT_EQ(d.system.count(), 0);
}

TEST(Duration, AdditionWorksCorrectly) {
    Duration a{Duration::Dur(100), Duration::Dur(50), Duration::Dur(25)};
    Duration b{Duration::Dur(200), Duration::Dur(75), Duration::Dur(50)};

    Duration c = a;
    c += b;

    EXPECT_EQ(c.wallclock.count(), 300);
    EXPECT_EQ(c.user.count(), 125);
    EXPECT_EQ(c.system.count(), 75);
}

TEST(Duration, SubtractionClampsToZeroOnUnderflow) {
    // This test exposes the unsigned-underflow bug.
    // If child durations exceed the parent, subtracting should clamp to zero,
    // not silently wrap to a huge number.
    Duration parent{Duration::Dur(100), Duration::Dur(50), Duration::Dur(25)};

    // Subtract a larger duration from each component
    Duration child{Duration::Dur(200), Duration::Dur(100), Duration::Dur(50)};

    parent -= child;

    // BUG: Without the fix, these wrap around to huge unsigned values.
    // With the fix (clamp-to-zero), they should all be zero.
    EXPECT_EQ(parent.wallclock.count(), 0);
    EXPECT_EQ(parent.user.count(), 0);
    EXPECT_EQ(parent.system.count(), 0);
}

TEST(Duration, SubtractionClampsEachComponentIndependently) {
    Duration d{Duration::Dur(100), Duration::Dur(50), Duration::Dur(25)};
    d -= Duration{Duration::Dur(50), Duration::Dur(75), Duration::Dur(10)};

    // wallclock: 100 - 50 = 50 (ok)
    EXPECT_EQ(d.wallclock.count(), 50);
    // user: 50 < 75, should clamp to 0 (not wrap!)
    EXPECT_EQ(d.user.count(), 0);
    // system: 25 - 10 = 15 (ok)
    EXPECT_EQ(d.system.count(), 15);
}

TEST(Duration, SubtractionWithEqualValuesYieldsZero) {
    Duration a{Duration::Dur(100), Duration::Dur(50), Duration::Dur(25)};
    Duration b = a;
    b -= a;

    EXPECT_EQ(b.wallclock.count(), 0);
    EXPECT_EQ(b.user.count(), 0);
    EXPECT_EQ(b.system.count(), 0);
}
