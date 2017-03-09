// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/media/src/media_service/media_player_impl.h"

#include "application/lib/app/connect.h"
#include "apps/media/lib/timeline/timeline.h"
#include "apps/media/src/fidl/fidl_formatting.h"
#include "apps/media/src/util/callback_joiner.h"
#include "lib/ftl/functional/make_copyable.h"
#include "lib/ftl/logging.h"

namespace media {

// static
std::shared_ptr<MediaPlayerImpl> MediaPlayerImpl::Create(
    fidl::InterfaceHandle<SeekingReader> reader_handle,
    fidl::InterfaceHandle<MediaRenderer> audio_renderer_handle,
    fidl::InterfaceHandle<MediaRenderer> video_renderer_handle,
    fidl::InterfaceRequest<MediaPlayer> request,
    MediaServiceImpl* owner) {
  return std::shared_ptr<MediaPlayerImpl>(new MediaPlayerImpl(
      std::move(reader_handle), std::move(audio_renderer_handle),
      std::move(video_renderer_handle), std::move(request), owner));
}

MediaPlayerImpl::MediaPlayerImpl(
    fidl::InterfaceHandle<SeekingReader> reader_handle,
    fidl::InterfaceHandle<MediaRenderer> audio_renderer_handle,
    fidl::InterfaceHandle<MediaRenderer> video_renderer_handle,
    fidl::InterfaceRequest<MediaPlayer> request,
    MediaServiceImpl* owner)
    : MediaServiceImpl::Product<MediaPlayer>(this, std::move(request), owner),
      reader_handle_(std::move(reader_handle)) {
  RCHECK(audio_renderer_handle || video_renderer_handle);
  FTL_DCHECK(owner);

  if (audio_renderer_handle) {
    streams_by_medium_[MediaTypeMedium::AUDIO].renderer_handle_ =
        std::move(audio_renderer_handle);
  }

  if (video_renderer_handle) {
    streams_by_medium_[MediaTypeMedium::VIDEO].renderer_handle_ =
        std::move(video_renderer_handle);
  }

  FLOG(log_channel_, BoundAs(FLOG_BINDING_KOID(binding())));

  status_publisher_.SetCallbackRunner([this](const GetStatusCallback& callback,
                                             uint64_t version) {
    MediaPlayerStatusPtr status = MediaPlayerStatus::New();
    status->timeline_transform = TimelineTransform::From(timeline_function_);
    status->has_video = has_video_;
    status->end_of_stream = end_of_stream_;
    status->metadata = metadata_.Clone();
    status->problem = problem_.Clone();
    callback(version, std::move(status));
  });

  state_ = State::kInactive;

  media_service_ = owner->ConnectToEnvironmentService<MediaService>();

  // Create a timeline controller.
  media_service_->CreateTimelineController(timeline_controller_.NewRequest());
  timeline_controller_->GetControlPoint(timeline_control_point_.NewRequest());
  timeline_control_point_->GetTimelineConsumer(timeline_consumer_.NewRequest());
  HandleTimelineControlPointStatusUpdates();

  MaybeCreateSource();
}

MediaPlayerImpl::~MediaPlayerImpl() {}

void MediaPlayerImpl::MaybeCreateSource() {
  if (!reader_handle_) {
    return;
  }

  state_ = State::kWaiting;

  media_service_->CreateSource(std::move(reader_handle_), nullptr,
                               source_.NewRequest());
  FLOG(log_channel_, CreatedSource(FLOG_PTR_KOID(source_)));
  HandleSourceStatusUpdates();

  source_->Describe(
      ftl::MakeCopyable([this](fidl::Array<MediaTypePtr> stream_types) mutable {
        FLOG(log_channel_, ReceivedSourceDescription(stream_types.Clone()));
        ConnectSinks(std::move(stream_types));
      }));
}

void MediaPlayerImpl::ConnectSinks(fidl::Array<MediaTypePtr> stream_types) {
  std::shared_ptr<CallbackJoiner> callback_joiner = CallbackJoiner::Create();

  size_t stream_index = 0;

  for (MediaTypePtr& stream_type : stream_types) {
    auto iter = streams_by_medium_.find(stream_type->medium);
    if (iter != streams_by_medium_.end()) {
      PrepareStream(&iter->second, stream_index, stream_type,
                    callback_joiner->NewCallback());
    }

    if (stream_type->medium == MediaTypeMedium::VIDEO) {
      has_video_ = true;
    }

    ++stream_index;
  }

  callback_joiner->WhenJoined([this]() {
    FLOG(log_channel_, StreamsPrepared());
    state_ = State::kFlushed;
    FLOG(log_channel_, Flushed());
    Update();
  });
}

void MediaPlayerImpl::PrepareStream(Stream* stream,
                                    size_t index,
                                    const MediaTypePtr& input_media_type,
                                    const std::function<void()>& callback) {
  FTL_DCHECK(media_service_);

  MediaPacketProducerPtr producer;
  source_->GetPacketProducer(index, producer.NewRequest());

  MediaPacketConsumerPtr consumer;

  if (stream->sink_) {
    stream->sink_->ChangeMediaType(input_media_type.Clone(),
                                   consumer.NewRequest());
  } else {
    FTL_DCHECK(stream->renderer_handle_);
    media_service_->CreateSink(
        std::move(stream->renderer_handle_), input_media_type.Clone(),
        stream->sink_.NewRequest(), consumer.NewRequest());
    FLOG(log_channel_, CreatedSink(index, FLOG_PTR_KOID(stream->sink_)));

    MediaTimelineControlPointPtr timeline_control_point;
    stream->sink_->GetTimelineControlPoint(timeline_control_point.NewRequest());

    timeline_controller_->AddControlPoint(std::move(timeline_control_point));
  }

  // Capture producer so it survives through the callback.
  producer->Connect(std::move(consumer), ftl::MakeCopyable([
                      this, callback, producer = std::move(producer)
                    ]() { callback(); }));
}

void MediaPlayerImpl::Update() {
  // This method is called whenever we might want to take action based on the
  // current state and recent events. The current state is in |state_|. Recent
  // events are recorded in |target_state_|, which indicates what state we'd
  // like to transition to, |target_position_|, which can indicate a position
  // we'd like to stream to, and |end_of_stream_| which tells us we've reached
  // end of stream.
  //
  // Also relevant is |reader_transition_pending_|, which, when true, is treated
  // pretty much like a |target_state_| of kFlushed. It indicates that we have
  // a new reader we want to use, so the graph needs to be flushed and rebuilt.
  // We use it instead of |target_state_| so that |target_state_| is preserved
  // for when the new graph is built, at which point we'll work to transition
  // to |target_state_|.
  //
  // The states are as follows:
  //
  // |kInactive|- Indicates that we have no reader.
  // |kWaiting| - Indicates that we've done something asynchronous, and no
  //              further action should be taken by the state machine until that
  //              something completes (at which point the callback will change
  //              the state and call |Update|).
  // |kFlushed| - Indicates that presentation time is not progressing and that
  //              the pipeline is not primed with packets. This is the initial
  //              state and the state we transition to in preparation for
  //              seeking. A seek is currently only done when when the pipeline
  //              is clear of packets.
  // |kPrimed| -  Indicates that presentation time is not progressing and that
  //              the pipeline is primed with packets. We transition to this
  //              state when the client calls |Pause|, either from |kFlushed| or
  //              |kPlaying| state.
  // |kPlaying| - Indicates that presentation time is progressing and there are
  //              packets in the pipeline. We transition to this state when the
  //              client calls |Play|. If we're in |kFlushed| when |Play| is
  //              called, we transition through |kPrimed| state.
  //
  // The while loop that surrounds all the logic below is there because, after
  // taking some action and transitioning to a new state, we may want to check
  // to see if there's more to do in the new state. You'll also notice that
  // the callback lambdas generally call |Update|.
  while (true) {
    switch (state_) {
      case State::kInactive:
        if (!reader_transition_pending_) {
          return;
        }
      // Falls through.

      case State::kFlushed:
        // Presentation time is not progressing, and the pipeline is clear of
        // packets.
        if (reader_transition_pending_) {
          // We need to switch to a new reader. Destroy the current source.
          reader_transition_pending_ = false;
          state_ = State::kInactive;
          source_.reset();
          metadata_.reset();
          problem_.reset();
          has_video_ = false;
          // Setting |transform_subject_time_| ensures that we tell the
          // renderers to start with PTS 0 when we transition to |kPlay|. It
          // doesn't cause a seek on the source, which should already be at
          // 0 anyway.
          transform_subject_time_ = 0;
          status_publisher_.SendUpdates();
          MaybeCreateSource();
          return;
        }

        if (target_position_ != kUnspecifiedTime) {
          // We want to seek. Enter |kWaiting| state until the operation is
          // complete.
          state_ = State::kWaiting;
          // Make sure the renderers have the right timeline so post-seek
          // packets don't get discarded.
          transform_subject_time_ = target_position_;
          SetTimelineTransform(0.0f, Timeline::local_now(),
                               [this](bool completed) {
                                 // Seek to the new position.
                                 FLOG(log_channel_, Seeking(target_position_));
                                 source_->Seek(target_position_, [this]() {
                                   target_position_ = kUnspecifiedTime;
                                   state_ = State::kFlushed;
                                   FLOG(log_channel_, Flushed());
                                   // Back in |kFlushed|. Call |Update| to see
                                   // if there's further action to be taken.
                                   Update();
                                 });
                               });

          // Done for now. We're in kWaiting, and the callback will call Update
          // when the Seek call is complete.
          return;
        }

        if (target_state_ == State::kPlaying ||
            target_state_ == State::kPrimed) {
          // We want to transition to |kPrimed| or to |kPlaying|, for which
          // |kPrimed| is a prerequisite. We enter |kWaiting| state, issue the
          // |Prime| request and transition to |kPrimed| when the operation is
          // complete.
          state_ = State::kWaiting;
          FLOG(log_channel_, Priming());
          timeline_control_point_->Prime([this]() {
            state_ = State::kPrimed;
            FLOG(log_channel_, Primed());
            // Now we're in |kPrimed|. Call |Update| to see if there's further
            // action to be taken.
            Update();
          });

          // Done for now. We're in |kWaiting|, and the callback will call
          // |Update| when the prime is complete.
          return;
        }

        // No interesting events to respond to. Done for now.
        return;

      case State::kPrimed:
        // Presentation time is not progressing, and the pipeline is primed with
        // packets.
        if (target_position_ != kUnspecifiedTime ||
            target_state_ == State::kFlushed || reader_transition_pending_) {
          // Either we want to seek or just want to transition to |kFlushed|,
          // possibly because a reader transition is pending. We transition to
          // |kWaiting|, issue the |Flush| request and transition to |kFlushed|
          // when the operation is complete.
          state_ = State::kWaiting;
          FLOG(log_channel_, Flushing());
          source_->Flush([this]() {
            state_ = State::kFlushed;
            FLOG(log_channel_, Flushed());
            // Now we're in |kFlushed|. Call |Update| to see if there's further
            // action to be taken.
            Update();
          });

          // Done for now. We're in |kWaiting|, and the callback will call
          // |Update| when the flush is complete.
          return;
        }

        if (target_state_ == State::kPlaying) {
          // We want to transition to |kPlaying|. Enter |kWaiting|, start the
          // presentation timeline and transition to |kPlaying| when the
          // operation completes.
          state_ = State::kWaiting;
          SetTimelineTransform(1.0f, Timeline::local_now() + kMinimumLeadTime,
                               [this](bool completed) {
                                 state_ = State::kPlaying;
                                 FLOG(log_channel_, Playing());
                                 // Now we're in |kPlaying|. Call |Update| to
                                 // see if there's further action to be taken.
                                 Update();
                               });

          // Done for now. We're in |kWaiting|, and the callback will call
          // |Update| when the flush is complete.
          return;
        }

        // No interesting events to respond to. Done for now.
        return;

      case State::kPlaying:
        // Presentation time is progressing, and packets are moving through
        // the pipeline.
        if (target_position_ != kUnspecifiedTime ||
            target_state_ == State::kFlushed ||
            target_state_ == State::kPrimed || reader_transition_pending_) {
          // Either we want to seek or we want to stop playback, possibly
          // because a reader transition is pending. In either case, we need
          // to enter |kWaiting|, stop the presentation timeline and transition
          // to |kPrimed| when the operation completes.
          state_ = State::kWaiting;
          SetTimelineTransform(0.0f, Timeline::local_now() + kMinimumLeadTime,
                               [this](bool completed) {
                                 state_ = State::kPrimed;
                                 FLOG(log_channel_, Primed());
                                 // Now we're in |kPrimed|. Call |Update| to see
                                 // if there's further action to be taken.
                                 Update();
                               });

          // Done for now. We're in |kWaiting|, and the callback will call
          // |Update| when the flush is complete.
          return;
        }

        if (end_of_stream_) {
          // We've reached end of stream. The presentation timeline stops by
          // itself, so we just need to transition to |kPrimed|.
          target_state_ = State::kPrimed;
          state_ = State::kPrimed;
          FLOG(log_channel_, EndOfStream());
          // Loop around to check if there's more work to do.
          break;
        }

        // No interesting events to respond to. Done for now.
        return;

      case State::kWaiting:
        // Waiting for some async operation. Nothing to do until it completes.
        return;
    }
  }
}

void MediaPlayerImpl::SetTimelineTransform(
    float rate,
    int64_t reference_time,
    const TimelineConsumer::SetTimelineTransformCallback callback) {
  TimelineTransformPtr timeline_transform =
      CreateTimelineTransform(rate, reference_time);
  FLOG(log_channel_, SettingTimelineTransform(timeline_transform.Clone()));
  timeline_consumer_->SetTimelineTransform(std::move(timeline_transform),
                                           callback);
}

TimelineTransformPtr MediaPlayerImpl::CreateTimelineTransform(
    float rate,
    int64_t reference_time) {
  TimelineTransformPtr result = TimelineTransform::New();
  result->reference_time = reference_time;
  result->subject_time = transform_subject_time_;

  TimelineRate timeline_rate(rate);
  result->reference_delta = timeline_rate.reference_delta();
  result->subject_delta = timeline_rate.subject_delta();

  transform_subject_time_ = kUnspecifiedTime;

  return result;
}

void MediaPlayerImpl::GetStatus(uint64_t version_last_seen,
                                const GetStatusCallback& callback) {
  status_publisher_.Get(version_last_seen, callback);
}

void MediaPlayerImpl::Play() {
  FLOG(log_channel_, PlayRequested());
  target_state_ = State::kPlaying;
  Update();
}

void MediaPlayerImpl::Pause() {
  FLOG(log_channel_, PauseRequested());
  target_state_ = State::kPrimed;
  Update();
}

void MediaPlayerImpl::Seek(int64_t position) {
  FLOG(log_channel_, SeekRequested(position));
  target_position_ = position;
  Update();
}

void MediaPlayerImpl::SetReader(
    fidl::InterfaceHandle<SeekingReader> reader_handle) {
  if (!reader_handle && !source_) {
    // There was already no reader. Nothing to do.
    return;
  }

  // Setting reader_transition_pending_ has a similar effect to setting
  // target_state_ to State::kFlushed. We don't change target_state_ so the
  // player will respect the client's desires once the reader transition is
  // complete.
  reader_transition_pending_ = true;
  reader_handle_ = std::move(reader_handle);

  // We clear |target_position_| so that a previously-requested seek that's
  // still pending will not be applied to the new reader. The client can call
  // |Seek| between this point and when the new graph is set up, and it will
  // work.
  target_position_ = kUnspecifiedTime;

  Update();
}

void MediaPlayerImpl::HandleSourceStatusUpdates(uint64_t version,
                                                MediaSourceStatusPtr status) {
  if (status) {
    metadata_ = std::move(status->metadata);
    problem_ = std::move(status->problem);
    status_publisher_.SendUpdates();
  }

  source_->GetStatus(version,
                     [this](uint64_t version, MediaSourceStatusPtr status) {
                       HandleSourceStatusUpdates(version, std::move(status));
                     });
}

void MediaPlayerImpl::HandleTimelineControlPointStatusUpdates(
    uint64_t version,
    MediaTimelineControlPointStatusPtr status) {
  if (status) {
    timeline_function_ = status->timeline_transform.To<TimelineFunction>();
    end_of_stream_ = status->end_of_stream;
    status_publisher_.SendUpdates();
    Update();
  }

  timeline_control_point_->GetStatus(
      version,
      [this](uint64_t version, MediaTimelineControlPointStatusPtr status) {
        HandleTimelineControlPointStatusUpdates(version, std::move(status));
      });
}

}  // namespace media
