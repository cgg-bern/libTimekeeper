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

TEST(Duration, SubtractionOfZeroIsNoOp) {
    Duration d{Duration::Dur(100), Duration::Dur(50), Duration::Dur(25)};
    Duration zero = Duration::zero();
    d -= zero;

    EXPECT_EQ(d.wallclock.count(), 100);
    EXPECT_EQ(d.user.count(), 50);
    EXPECT_EQ(d.system.count(), 25);
}

TEST(Duration, AdditionIsCommutative) {
    Duration a{Duration::Dur(100), Duration::Dur(50), Duration::Dur(25)};
    Duration b{Duration::Dur(200), Duration::Dur(75), Duration::Dur(50)};

    Duration ab = a;
    ab += b;

    Duration ba = b;
    ba += a;

    EXPECT_EQ(ab.wallclock.count(), ba.wallclock.count());
    EXPECT_EQ(ab.user.count(), ba.user.count());
    EXPECT_EQ(ab.system.count(), ba.system.count());
}

TEST(Duration, AdditionIsAssociative) {
    Duration a{Duration::Dur(10), Duration::Dur(20), Duration::Dur(30)};
    Duration b{Duration::Dur(40), Duration::Dur(50), Duration::Dur(60)};
    Duration c{Duration::Dur(70), Duration::Dur(80), Duration::Dur(90)};

    Duration ab_c = a;
    ab_c += b;
    ab_c += c;

    Duration a_bc = b;
    a_bc += c;
    a += a_bc;

    EXPECT_EQ(ab_c.wallclock.count(), a.wallclock.count());
    EXPECT_EQ(ab_c.user.count(), a.user.count());
    EXPECT_EQ(ab_c.system.count(), a.system.count());
}

TEST(Duration, SubtractionFromZeroClampsToZero) {
    Duration d = Duration::zero();
    d -= Duration{Duration::Dur(100), Duration::Dur(50), Duration::Dur(25)};

    EXPECT_EQ(d.wallclock.count(), 0);
    EXPECT_EQ(d.user.count(), 0);
    EXPECT_EQ(d.system.count(), 0);
}
