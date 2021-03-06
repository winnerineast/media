// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "apps/media/src/framework/models/demand.h"
#include "apps/media/src/framework/models/part.h"
#include "apps/media/src/framework/packet.h"
#include "apps/media/src/framework/payload_allocator.h"

namespace media {

// Source that produces packets asynchronously.
class ActiveSource : public Part {
 public:
  using SupplyCallback = std::function<void(PacketPtr packet)>;

  ~ActiveSource() override {}

  // Whether the source can accept an allocator.
  virtual bool can_accept_allocator() const = 0;

  // Sets the allocator for the source.
  virtual void set_allocator(PayloadAllocator* allocator) = 0;

  // Sets the callback that supplies a packet asynchronously.
  virtual void SetSupplyCallback(const SupplyCallback& supply_callback) = 0;

  // Sets the demand signalled from downstream.
  virtual void SetDownstreamDemand(Demand demand) = 0;
};

}  // namespace media
