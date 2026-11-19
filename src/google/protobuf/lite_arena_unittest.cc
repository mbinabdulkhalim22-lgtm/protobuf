// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "absl/log/absl_check.h"
#include "google/protobuf/arena_test_util.h"
#include "google/protobuf/map_lite_unittest.pb.h"
#include "google/protobuf/map_test_util.h"
#include "google/protobuf/unittest_lite.pb.h"

#include <gtest/gtest.h>


namespace google {
namespace protobuf {
namespace {

class LiteArenaTest : public testing::Test {
 protected:
  LiteArenaTest() {
    ArenaOptions options;
    options.start_block_size = 128 * 1024;
    options.max_block_size = 128 * 1024;
    arena_.reset(new Arena(options));
    // Trigger the allocation of the first arena block, so that further use of
    // the arena will not require any heap allocations.
    (void)Arena::CreateArray<char>(arena_.get(), 1);
  }

  std::unique_ptr<Arena> arena_;
};

TEST_F(LiteArenaTest, MapNoHeapAllocation) {
  std::string data;
  data.reserve(128 * 1024);

  {
    // TODO: Enable no heap check when ArenaStringPtr is used in
    // Map.
    // internal::NoHeapChecker no_heap;

    proto2_unittest::TestArenaMapLite* from =
        Arena::Create<proto2_unittest::TestArenaMapLite>(arena_.get());
    MapTestUtil::SetArenaMapFields(from);
    ABSL_CHECK(from->SerializeToString(&data));

    proto2_unittest::TestArenaMapLite* to =
        Arena::Create<proto2_unittest::TestArenaMapLite>(arena_.get());
    ABSL_CHECK(to->ParseFromString(data));
    MapTestUtil::ExpectArenaMapFieldsSet(*to);
  }
}

TEST_F(LiteArenaTest, UnknownFieldMemLeak) {
  proto2_unittest::ForeignMessageArenaLite* message =
      Arena::Create<proto2_unittest::ForeignMessageArenaLite>(arena_.get());
  std::string data = "\012\000";
  int original_capacity = data.capacity();
  while (data.capacity() <= original_capacity) {
    data.append("a");
  }
  data[1] = data.size() - 2;
  ABSL_CHECK(message->ParseFromString(data));
}

TEST_F(LiteArenaTest, UnknownFieldsWithArenaDestructor) {
  // TestAllTypesLite has Cord fields, so it registers a destructor with the
  // arena when it is created. Its unknown fields are allocated later, so the
  // arena cleans them up before it runs that destructor. The destructor must
  // still find the arena of the message: destroying a non-empty repeated Cord
  // field looks it up.
  proto2_unittest::TestAllTypesLite* message =
      Arena::Create<proto2_unittest::TestAllTypesLite>(arena_.get());
  // Field 99999 (varint) is not declared in TestAllTypesLite.
  EXPECT_TRUE(message->ParseFromString("\xf8\xe9\x30\x01"));
  EXPECT_FALSE(message->unknown_fields().empty());
  message->add_repeated_cord("cord");
  arena_.reset();
}

TEST_F(LiteArenaTest, ArenaDestroysOnlyUnknownFieldsString) {
  proto2_unittest::ForeignMessageArenaLite* message =
      Arena::Create<proto2_unittest::ForeignMessageArenaLite>(arena_.get());
  std::string* unknown_fields = nullptr;
  internal::CleanupGrowthInfo growth = internal::CleanupGrowth(
      *arena_, [&] { unknown_fields = message->mutable_unknown_fields(); });
  // The arena must destroy the unknown fields, and nothing else: the container
  // around them also holds the arena pointer of the message, which must stay
  // readable while the arena runs its cleanups.
  EXPECT_EQ(growth.cleanups.size(), 1u);
  EXPECT_TRUE(growth.cleanups.contains(unknown_fields));
}

}  // namespace
}  // namespace protobuf
}  // namespace google

