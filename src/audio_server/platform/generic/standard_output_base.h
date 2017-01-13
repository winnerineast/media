// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "apps/media/lib/timeline/timeline.h"
#include "apps/media/lib/timeline/timeline_function.h"
#include "apps/media/services/media_result.fidl.h"
#include "apps/media/services/media_types.fidl.h"
#include "apps/media/src/audio_server/audio_output.h"
#include "apps/media/src/audio_server/audio_renderer_to_output_link.h"
#include "apps/media/src/audio_server/gain.h"
#include "apps/media/src/audio_server/platform/generic/mixer.h"
#include "apps/media/src/audio_server/platform/generic/output_formatter.h"
#include "lib/ftl/time/time_delta.h"

namespace media {
namespace audio {

class StandardOutputBase : public AudioOutput {
 public:
  ~StandardOutputBase() override;

 protected:
  struct MixJob {
    static constexpr uint32_t kInvalidGeneration = 0;

    // State for the job set up once by the output implementation and then used
    // by all renderers.
    void* buf;
    uint32_t buf_frames;
    int64_t start_pts_of;  // start PTS, expressed in output frames.
    uint32_t local_to_output_gen;
    bool accumulate;
    const TimelineFunction* local_to_output;

    // State for the job which is set up for each renderer during SetupMix
    uint32_t frames_produced;
  };

  struct RendererBookkeeping : public AudioRendererToOutputLink::Bookkeeping {
    RendererBookkeeping();
    ~RendererBookkeeping() override;

    TimelineFunction lt_to_renderer_frames;
    TimelineFunction out_frames_to_renderer_frames;
    uint32_t lt_to_renderer_frames_gen = 0;
    uint32_t out_frames_to_renderer_frames_gen = MixJob::kInvalidGeneration;
    uint32_t step_size;
    Gain::AScale amplitude_scale;
    MixerPtr mixer;

    void UpdateRendererTrans(const AudioRendererImplPtr& renderer);
    void UpdateOutputTrans(const MixJob& job);
  };

  explicit StandardOutputBase(AudioOutputManager* manager);

  void Process() FTL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) final;
  MediaResult InitializeLink(const AudioRendererToOutputLinkPtr& link) final;

  void SetNextSchedTime(ftl::TimePoint next_sched_time) {
    next_sched_time_ = next_sched_time;
    next_sched_time_known_ = true;
  }

  void SetNextSchedDelay(const ftl::TimeDelta& next_sched_delay) {
    SetNextSchedTime(ftl::TimePoint::Now() + next_sched_delay);
  }

  virtual bool StartMixJob(MixJob* job, ftl::TimePoint process_start) = 0;
  virtual bool FinishMixJob(const MixJob& job) = 0;
  virtual RendererBookkeeping* AllocBookkeeping();
  void SetupMixBuffer(uint32_t max_mix_frames);

  // Details about the final output format
  OutputFormatterPtr output_formatter_;

 private:
  using RendererSetupTask =
      std::function<bool(const AudioRendererImplPtr& renderer,
                         RendererBookkeeping* info)>;
  using RendererProcessTask =
      std::function<bool(const AudioRendererImplPtr& renderer,
                         RendererBookkeeping* info,
                         const AudioPipe::AudioPacketRefPtr& pkt_ref)>;

  void ForeachRenderer(const RendererSetupTask& setup,
                       const RendererProcessTask& process)
      FTL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool SetupMix(const AudioRendererImplPtr& renderer,
                RendererBookkeeping* info);
  bool ProcessMix(const AudioRendererImplPtr& renderer,
                  RendererBookkeeping* info,
                  const AudioPipe::AudioPacketRefPtr& pkt_ref);

  bool SetupTrim(const AudioRendererImplPtr& renderer,
                 RendererBookkeeping* info);
  bool ProcessTrim(const AudioRendererImplPtr& renderer,
                   RendererBookkeeping* info,
                   const AudioPipe::AudioPacketRefPtr& pkt_ref);

  ftl::TimePoint next_sched_time_;
  bool next_sched_time_known_;

  // State for the internal buffer which holds intermediate mix results.
  //
  // TODO(johngro): Right now, the cannonical intermediate format is signed 32
  // bit ints.  As time goes on, we may need to reconsider this.  This will
  // become more important when...
  //
  // 1) We support 24 bit audio.  Right now, with a 16 bit max, we can
  //    accumulate for up to a maximum of 2^16-1 renderers without needing to do
  //    anything special about about clipping.  With 24 bit audio, this number
  //    will drop to only 255 simultanious renderers.  It is unclear if this is
  //    a reasonable system-wide limitation or not.
  // 2) We support floating point audio.
  std::unique_ptr<int32_t[]> mix_buf_;
  uint32_t mix_buf_frames_ = 0;

  // State used by the mix task.
  RendererSetupTask setup_mix_;
  RendererProcessTask process_mix_;
  MixJob cur_mix_job_;

  // State used by the trim task.
  RendererSetupTask setup_trim_;
  RendererProcessTask process_trim_;
  int64_t trim_threshold_;
};

}  // namespace audio
}  // namespace media
