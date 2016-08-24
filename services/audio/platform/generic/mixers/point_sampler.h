// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MEDIA_SERVICES_AUDIO_PLATFORM_GENERIC_MIXERS_POINT_SAMPLER_H_
#define APPS_MEDIA_SERVICES_AUDIO_PLATFORM_GENERIC_MIXERS_POINT_SAMPLER_H_

#include "apps/media/interfaces/media_types.mojom.h"
#include "apps/media/services/audio/platform/generic/mixer.h"

namespace mojo {
namespace media {
namespace audio {
namespace mixers {

class PointSampler : public Mixer {
 public:
  static MixerPtr Select(const AudioMediaTypeDetailsPtr& src_format,
                         const AudioMediaTypeDetailsPtr& dst_format);

 protected:
  PointSampler(uint32_t pos_filter_width, uint32_t neg_filter_width)
      : Mixer(pos_filter_width, neg_filter_width) {}
};

}  // namespace mixers
}  // namespace audio
}  // namespace media
}  // namespace mojo

#endif  // APPS_MEDIA_SERVICES_AUDIO_PLATFORM_GENERIC_MIXERS_POINT_SAMPLER_H_
