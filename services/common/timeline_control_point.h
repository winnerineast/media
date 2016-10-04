// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MEDIA_SERVICES_COMMON_TIMELINE_CONTROL_POINT_H_
#define APPS_MEDIA_SERVICES_COMMON_TIMELINE_CONTROL_POINT_H_

#include "apps/media/cpp/timeline_function.h"
#include "apps/media/interfaces/timeline_controller.mojom.h"
#include "apps/media/services/common/mojo_publisher.h"
#include "lib/ftl/synchronization/mutex.h"
#include "lib/ftl/synchronization/thread_annotations.h"
#include "lib/ftl/tasks/task_runner.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mojo {
namespace media {

// MediaTimelineControlPoint implementation.
class TimelineControlPoint : public MediaTimelineControlPoint,
                             public TimelineConsumer {
 public:
  using PrimeRequestedCallback = std::function<void(const PrimeCallback&)>;
  TimelineControlPoint();

  ~TimelineControlPoint() override;

  // Binds to the control point. If a binding exists already, it is closed.
  void Bind(InterfaceRequest<MediaTimelineControlPoint> request);

  // Determines whether the control point is currently bound.
  bool is_bound() { return control_point_binding_.is_bound(); }

  // Unbinds from clients and resets to initial state.
  void Reset();

  // Sets a callback to be called when priming is requested.
  void SetPrimeRequestedCallback(const PrimeRequestedCallback& callback) {
    prime_requested_callback_ = callback;
  }

  // Get the TimelineFunction for the reference_time (which should be 'now',
  // approximately).
  void SnapshotCurrentFunction(int64_t reference_time,
                               TimelineFunction* out,
                               uint32_t* generation = nullptr);

  // Sets the current end_of_stream status published by the control point.
  void SetEndOfStreamPts(int64_t end_of_stream_pts);

  // MediaTimelineControlPoint implementation.
  void GetStatus(uint64_t version_last_seen,
                 const GetStatusCallback& callback) override;

  void GetTimelineConsumer(
      InterfaceRequest<TimelineConsumer> timeline_consumer) override;

  void Prime(const PrimeCallback& callback) override;

  // TimelineConsumer implementation.
  void SetTimelineTransform(
      TimelineTransformPtr timeline_transform,
      const SetTimelineTransformCallback& callback) override;

 private:
  // Applies pending_timeline_function_ if it's time to do so based on the
  // given reference time.
  void ApplyPendingChanges(int64_t reference_time)
      FTL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Clears the pending timeline function and calls its associated callback
  // with the indicated completed status.
  void ClearPendingTimelineFunction(bool completed)
      FTL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Determines if an unrealized timeline function is currently pending.
  bool TimelineFunctionPending() FTL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return pending_timeline_function_.reference_time() != kUnspecifiedTime;
  }

  // Determines whether end-of-stream has been reached.
  bool ReachedEndOfStream() FTL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Unbinds from clients and resets to initial state.
  void PostReset() FTL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Binding<MediaTimelineControlPoint> control_point_binding_;
  Binding<TimelineConsumer> consumer_binding_;
  MojoPublisher<GetStatusCallback> status_publisher_;
  PrimeRequestedCallback prime_requested_callback_;

  ftl::Mutex mutex_;
  ftl::RefPtr<ftl::TaskRunner> task_runner_ FTL_GUARDED_BY(mutex_);
  TimelineFunction current_timeline_function_ FTL_GUARDED_BY(mutex_);
  TimelineFunction pending_timeline_function_ FTL_GUARDED_BY(mutex_);
  SetTimelineTransformCallback set_timeline_transform_callback_
      FTL_GUARDED_BY(mutex_);
  uint32_t generation_ FTL_GUARDED_BY(mutex_) = 1;
  int64_t end_of_stream_pts_ FTL_GUARDED_BY(mutex_) = kUnspecifiedTime;
  bool end_of_stream_published_ FTL_GUARDED_BY(mutex_) = false;
};

}  // namespace media
}  // namespace mojo

#endif  // APPS_MEDIA_SERVICES_COMMON_TIMELINE_CONTROL_POINT_H_
