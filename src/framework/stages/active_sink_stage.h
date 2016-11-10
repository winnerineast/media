// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <deque>

#include "apps/media/src/framework/models/active_sink.h"
#include "apps/media/src/framework/stages/stage.h"

namespace media {

// A stage that hosts an ActiveSink.
class ActiveSinkStage : public Stage {
 public:
  ActiveSinkStage(std::shared_ptr<ActiveSink> sink);

  ~ActiveSinkStage() override;

  // Stage implementation.
  size_t input_count() const override;

  Input& input(size_t index) override;

  size_t output_count() const override;

  Output& output(size_t index) override;

  PayloadAllocator* PrepareInput(size_t index) override;

  void PrepareOutput(size_t index,
                     PayloadAllocator* allocator,
                     const UpstreamCallback& callback) override;

  void Update(Engine* engine) override;

  void FlushInput(size_t index, const DownstreamCallback& callback) override;

  void FlushOutput(size_t index) override;

 private:
  Input input_;
  std::shared_ptr<ActiveSink> sink_;
  ActiveSink::DemandCallback demand_function_;
  Demand sink_demand_ = Demand::kNegative;
};

}  // namespace media
