// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MEDIA_SERVICES_FRAMEWORK_STAGES_TRANSFORM_STAGE_H_
#define APPS_MEDIA_SERVICES_FRAMEWORK_STAGES_TRANSFORM_STAGE_H_

#include "apps/media/services/framework/models/transform.h"
#include "apps/media/services/framework/stages/stage.h"

namespace mojo {
namespace media {

// A stage that hosts a Transform.
class TransformStage : public Stage {
 public:
  TransformStage(std::shared_ptr<Transform> transform);

  ~TransformStage() override;

  // Stage implementation.
  size_t input_count() const override;

  Input& input(size_t index) override;

  size_t output_count() const override;

  Output& output(size_t index) override;

  PayloadAllocator* PrepareInput(size_t index) override;

  void PrepareOutput(size_t index,
                     PayloadAllocator* allocator,
                     const UpstreamCallback& callback) override;

  void UnprepareOutput(size_t index, const UpstreamCallback& callback) override;

  void Update(Engine* engine) override;

  void FlushInput(size_t index, const DownstreamCallback& callback) override;

  void FlushOutput(size_t index) override;

 private:
  Input input_;
  Output output_;
  std::shared_ptr<Transform> transform_;
  PayloadAllocator* allocator_;
  bool input_packet_is_new_;
};

}  // namespace media
}  // namespace mojo

#endif  // APPS_MEDIA_SERVICES_FRAMEWORK_STAGES_TRANSFORM_STAGE_H_
