// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "apps/media/src/audio_server/platform/generic/standard_output_base.h"

namespace mojo {
namespace media {
namespace audio {

class ThrottleOutput : public StandardOutputBase {
 public:
  static AudioOutputPtr New(AudioOutputManager* manager);
  ~ThrottleOutput() override;

 protected:
  explicit ThrottleOutput(AudioOutputManager* manager);

  // AudioOutput Implementation
  MediaResult Init() override;

  // StandardOutputBase Implementation
  bool StartMixJob(MixJob* job, ftl::TimePoint process_start) override;
  bool FinishMixJob(const MixJob& job) override;

 private:
  ftl::TimePoint last_sched_time_;
};

}  // namespace audio
}  // namespace media
}  // namespace mojo
