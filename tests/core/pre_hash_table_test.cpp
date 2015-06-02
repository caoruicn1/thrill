/*******************************************************************************
 * tests/core/pre_hash_table_test.cpp
 *
 * Part of Project c7a.
 *
 *
 * This file has no license. Only Chuck Norris can compile it.
 ******************************************************************************/

#include <c7a/core/reduce_pre_table.hpp>

#include "gtest/gtest.h"

using namespace c7a::data;
using namespace c7a::net;
using StringPair = std::pair<std::string, int>;

struct PreTable : public::testing::Test {
    PreTable()
        : dispatcher(),
          multiplexer(dispatcher),
          manager(multiplexer),
          id1(manager.AllocateDIA()),
          id2(manager.AllocateDIA()) {
        one_int_emitter.emplace_back(manager.GetLocalEmitter<int>(id1));
        one_pair_emitter.emplace_back(manager.GetLocalEmitter<StringPair>(id1));

        two_int_emitters.emplace_back(manager.GetLocalEmitter<int>(id1));
        two_int_emitters.emplace_back(manager.GetLocalEmitter<int>(id2));

        two_pair_emitters.emplace_back(manager.GetLocalEmitter<StringPair>(id1));
        two_pair_emitters.emplace_back(manager.GetLocalEmitter<StringPair>(id2));
    }

    Dispatcher                             dispatcher;
    ChannelMultiplexer                     multiplexer;
    Manager                                manager;
    DIAId                                  id1;
    DIAId                                  id2;
    // all emitters access the same dia id, which is bad if you use them both
    std::vector<BlockEmitter<int> >        one_int_emitter;
    std::vector<BlockEmitter<int> >        two_int_emitters;
    std::vector<BlockEmitter<StringPair> > one_pair_emitter;
    std::vector<BlockEmitter<StringPair> > two_pair_emitters;
};

struct MyStruct
{
    int key;
    int count;

    // only initializing constructor, no default construction possible.
    explicit MyStruct(int k, int c) : key(k), count(c)
    { }
};

namespace c7a {
namespace data {
namespace serializers {
template <>
struct Impl<MyStruct>{
    static std::string Serialize(const MyStruct& s) {
        std::size_t len = 2 * sizeof(int);
        char result[len];
        std::memcpy(result, &(s.key), sizeof(int));
        std::memcpy(result + sizeof(int), &(s.count), sizeof(int));
        return std::string(result, len);
    }

    static MyStruct Deserialize(const std::string& x) {
        int i, j;
        std::memcpy(&i, x.c_str(), sizeof(int));
        std::memcpy(&j, x.substr(sizeof(int), 2 * sizeof(int)).c_str(), sizeof(int));
        return MyStruct(i, j);
    }
};
}
}
}

TEST_F(PreTable, AddIntegers) {
    auto key_ex = [](int in) {
                      return in;
                  };

    auto red_fn = [](int in1, int in2) {
                      return in1 + in2;
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<int> >
    table(1, key_ex, red_fn, one_int_emitter);

    table.Insert(1);
    table.Insert(2);
    table.Insert(3);

    ASSERT_EQ(3u, table.Size());

    table.Insert(2);

    ASSERT_EQ(3u, table.Size());
}

TEST_F(PreTable, CreateEmptyTable) {
    auto key_ex = [](int in) { return in; };

    auto red_fn = [](int in1, int in2) {
                      return in1 + in2;
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<int> >
    table(1, key_ex, red_fn, one_int_emitter);

    table.Insert(1);
    table.Insert(2);
    table.Insert(3);

    ASSERT_EQ(3u, table.Size());

    table.Insert(2);

    ASSERT_EQ(3u, table.Size());
}

TEST_F(PreTable, PopIntegers) {
    auto red_fn = [](int in1, int in2) {
                      return in1 + in2;
                  };

    auto key_ex = [](int in) { return in; };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<int> >
    table(1, key_ex, red_fn, one_int_emitter);

    table.SetMaxSize(3);

    table.Insert(1);
    table.Insert(2);
    table.Insert(3);
    table.Insert(4);

    ASSERT_EQ(0u, table.Size());

    table.Insert(1);

    ASSERT_EQ(1u, table.Size());
}

// Manually flush all items in table,
// no size constraint, one partition
TEST_F(PreTable, FlushIntegersManuallyOnePartition) {
    auto key_ex = [](int in) {
                      return in;
                  };

    auto red_fn = [](int in1, int in2) {
                      return in1 + in2;
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<int> >
    table(1, 10, 2, 10, 10, key_ex, red_fn, one_int_emitter);

    table.Insert(0);
    table.Insert(1);
    table.Insert(2);
    table.Insert(3);
    table.Insert(4);

    ASSERT_EQ(5u, table.Size());

    table.Flush();

    auto it = manager.GetIterator<int>(id1);
    int c = 0;
    while (it.HasNext()) {
        std::cout << "test" << std::endl;
        it.Next();
        c++;
    }

    std::cout << "test: " << c << std::endl;

    ASSERT_EQ(5, c);
    ASSERT_EQ(0u, table.Size());
}

// Manually flush all items in table,
// no size constraint, two partitions
TEST_F(PreTable, FlushIntegersManuallyTwoPartitions) {
    auto key_ex = [](int in) {
                      return in;
                  };

    auto red_fn = [](int in1, int in2) {
                      return in1 + in2;
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<int> >
    table(2, 5, 2, 10, 10, key_ex, red_fn, two_int_emitters);

    table.Insert(0);
    table.Insert(1);
    table.Insert(2);
    table.Insert(3);
    table.Insert(4);

    ASSERT_EQ(5u, table.Size());

    table.Flush();

    auto it1 = manager.GetIterator<int>(id1);
    int c1 = 0;
    while (it1.HasNext()) {
        it1.Next();
        c1++;
    }

    ASSERT_EQ(3, c1);

    auto it2 = manager.GetIterator<int>(id2);
    int c2 = 0;
    while (it2.HasNext()) {
        it2.Next();
        c2++;
    }

    ASSERT_EQ(2, c2);
    ASSERT_EQ(0u, table.Size());
}

// Partial flush of items in table due to
// max table size constraint, one partition
TEST_F(PreTable, FlushIntegersPartiallyOnePartition) {
    auto key_ex = [](int in) {
                      return in;
                  };

    auto red_fn = [](int in1, int in2) {
                      return in1 + in2;
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<int> >
    table(1, 10, 2, 10, 4, key_ex, red_fn, one_int_emitter);

    table.Insert(0);
    table.Insert(1);
    table.Insert(2);
    table.Insert(3);

    ASSERT_EQ(4u, table.Size());

    table.Insert(4);

    auto it = manager.GetIterator<int>(id1);
    int c = 0;
    while (it.HasNext()) {
        it.Next();
        c++;
    }

    ASSERT_EQ(5, c);
    ASSERT_EQ(0u, table.Size());
}

//// Partial flush of items in table due to
//// max table size constraint, two partitions
TEST_F(PreTable, FlushIntegersPartiallyTwoPartitions) {
    auto key_ex = [](int in) {
                      return in;
                  };

    auto red_fn = [](int in1, int in2) {
                      return in1 + in2;
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<int> >
    table(2, 5, 2, 10, 4, key_ex, red_fn, two_int_emitters);

    table.Insert(0);
    table.Insert(1);
    table.Insert(2);
    table.Insert(3);

    ASSERT_EQ(4u, table.Size());

    table.Insert(4);
    table.Flush();

    auto it1 = manager.GetIterator<int>(id1);
    int c1 = 0;
    while (it1.HasNext()) {
        it1.Next();
        c1++;
    }

    ASSERT_EQ(3, c1);
    table.Flush();

    auto it2 = manager.GetIterator<int>(id2);
    int c2 = 0;
    while (it2.HasNext()) {
        it2.Next();
        c2++;
    }

    ASSERT_EQ(2, c2);
    ASSERT_EQ(0u, table.Size());
}

TEST_F(PreTable, ComplexType) {

    auto key_ex = [](StringPair in) {
                      return in.first;
                  };

    auto red_fn = [](StringPair in1, StringPair in2) {
                      return std::make_pair(in1.first, in1.second + in2.second);
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<StringPair> >
    table(1, 2, 2, 10, 3, key_ex, red_fn, one_pair_emitter);

    table.Insert(std::make_pair("hallo", 1));
    table.Insert(std::make_pair("hello", 2));
    table.Insert(std::make_pair("bonjour", 3));

    ASSERT_EQ(3u, table.Size());

    table.Insert(std::make_pair("hello", 5));

    ASSERT_EQ(3u, table.Size());

    table.Insert(std::make_pair("baguette", 42));

    ASSERT_EQ(0u, table.Size());
}

TEST_F(PreTable, MultipleWorkers) {
    auto key_ex = [](int in) {
                      return in;
                  };

    auto red_fn = [](int in1, int in2) {
                      return in1 + in2;
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<int> >
    table(2, key_ex, red_fn, one_int_emitter);

    ASSERT_EQ(0u, table.Size());
    table.SetMaxSize(5);

    for (int i = 0; i < 6; i++) {
        table.Insert(i * 35001);
    }

    ASSERT_LE(table.Size(), 3u);
    ASSERT_GT(table.Size(), 0u);
}

// Resize due to max bucket size reached. Set max items per bucket to 1,
// then add 2 items with different key, but having same hash value, one partition
TEST_F(PreTable, ResizeOnePartition) {
    auto key_ex = [](int in) {
                      return in;
                  };

    auto red_fn = [](int in1, int in2) {
                      return in1 + in2;
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<int> >
    table(1, 1, 10, 1, 10, key_ex, red_fn, one_int_emitter);

    table.Insert(1);

    ASSERT_EQ(1u, table.NumBuckets());
    ASSERT_EQ(1u, table.PartitionSize(0));
    ASSERT_EQ(1u, table.Size());

    table.Insert(2); // Resize happens here

    ASSERT_EQ(10u, table.NumBuckets());
    ASSERT_EQ(2u, table.PartitionSize(0));
    ASSERT_EQ(2u, table.Size());

    table.Flush();

    auto it1 = manager.GetIterator<int>(id1);
    int c = 0;
    while (it1.HasNext()) {
        it1.Next();
        c++;
    }

    ASSERT_EQ(2, c);
}

// Resize due to max bucket size reached. Set max items per bucket to 1,
// then add 2 items with different key, but having same hash value, two partitions
// Check that same items are in same partition after resize
TEST_F(PreTable, ResizeTwoPartitions) {
    auto key_ex = [](int in) {
                      return in;
                  };

    auto red_fn = [](int in1, int in2) {
                      return in1 + in2;
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<int> >
    table(2, 1, 10, 1, 10, key_ex, red_fn, two_int_emitters);

    ASSERT_EQ(0u, table.Size());
    ASSERT_EQ(2u, table.NumBuckets());
    ASSERT_EQ(0u, table.PartitionSize(0));
    ASSERT_EQ(0u, table.PartitionSize(1));

    table.Insert(1);
    table.Insert(2);

    ASSERT_EQ(2u, table.Size());
    ASSERT_EQ(2u, table.NumBuckets());
    ASSERT_EQ(1u, table.PartitionSize(0));
    ASSERT_EQ(1u, table.PartitionSize(1));

    table.Insert(3); // Resize happens here

    ASSERT_EQ(3u, table.Size());
    ASSERT_EQ(20u, table.NumBuckets());
    ASSERT_EQ(3u, table.PartitionSize(0) + table.PartitionSize(1));
}

// Insert several items with same key and test application of local reduce
TEST_F(PreTable, BigReduce) {
    auto key_ex = [](const MyStruct& in) {
                      return in.key % 500;
                  };

    auto red_fn = [](const MyStruct& in1, const MyStruct& in2) {
                      return MyStruct(in1.key, in1.count + in2.count);
                  };

    size_t total_sum = 0, total_count = 0;

    auto id1 = manager.AllocateDIA();
    std::vector<BlockEmitter<MyStruct> > emitters;
    emitters.emplace_back(manager.GetLocalEmitter<MyStruct>(id1));

    // Hashtable with smaller block size for testing.
    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn),
                              BlockEmitter<MyStruct>, 16*1024>
    table(1, 2, 2, 128 * 1024, 1024 * 1024,
          key_ex, red_fn, { emitters });

    // insert lots of items
    size_t nitems = 1 * 1024 * 1024;
    for (size_t i = 0; i != nitems; ++i) {
        table.Insert(MyStruct(i, 1));
    }

    table.Flush();

    auto it1 = manager.GetIterator<MyStruct>(id1);
    while (it1.HasNext()) {
        auto n = it1.Next();
        total_count++;
        total_sum += n.count;
    }

    // actually check that the reduction worked
    ASSERT_EQ(500u, total_count);
    ASSERT_EQ(nitems, total_sum);
}

// Insert several items with same key and test application of local reduce
TEST_F(PreTable, SmallReduce) {
    auto key_ex = [](StringPair in) { return in.first; };

    auto red_fn = [](StringPair in1, StringPair in2) {
                      return std::make_pair(in1.first, in1.second + in2.second);
                  };

    c7a::core::ReducePreTable<decltype(key_ex), decltype(red_fn), BlockEmitter<StringPair> >
    table(1, 10, 2, 10, 10, key_ex, red_fn, one_pair_emitter);

    table.Insert(std::make_pair("hallo", 1));
    table.Insert(std::make_pair("hello", 22));
    table.Insert(std::make_pair("bonjour", 3));

    ASSERT_EQ(3u, table.Size());

    table.Insert(std::make_pair("hallo", 2));
    table.Insert(std::make_pair("hello", 33));
    table.Insert(std::make_pair("bonjour", 44));

    ASSERT_EQ(3u, table.Size());

    table.Flush();

    auto it1 = manager.GetIterator<StringPair>(id1);
    int c1 = 0;
    while (it1.HasNext()) {
        StringPair p = it1.Next();
        if (p.first == "hallo") {
            ASSERT_EQ(3, p.second);
            c1++;
        }
        else if (p.first == "hello") {
            ASSERT_EQ(55, p.second);
            c1++;
        }
        else if (p.first == "bonjour") {
            ASSERT_EQ(47, p.second);
            c1++;
        }
    }
    ASSERT_EQ(3, c1);
}

/******************************************************************************/
