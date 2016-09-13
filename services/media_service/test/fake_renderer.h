// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MEDIA_SERVICES_MEDIA_SERVICE_TEST_FAKE_RENDERER_H_
#define APPS_MEDIA_SERVICES_MEDIA_SERVICE_TEST_FAKE_RENDERER_H_

#include <memory>
#include <queue>
#include <vector>

#include "apps/media/cpp/media_packet_consumer_base.h"
#include "apps/media/cpp/timeline_function.h"
#include "apps/media/interfaces/media_renderer.mojom.h"
#include "apps/media/interfaces/media_transport.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mojo {
namespace media {

// Implements MediaRenderer for testing.
class FakeRenderer : public MediaPacketConsumerBase,
                     public MediaRenderer,
                     public MediaTimelineControlPoint,
                     public TimelineConsumer {
 public:
  class PacketInfo {
   public:
    PacketInfo(int64_t pts, bool end_of_stream, uint64_t size, uint64_t hash)
        : pts_(pts), end_of_stream_(end_of_stream), size_(size), hash_(hash) {}

    int64_t pts() const { return pts_; }
    bool end_of_stream() const { return end_of_stream_; }
    uint64_t size() const { return size_; }
    uint64_t hash() const { return hash_; }

   private:
    int64_t pts_;
    bool end_of_stream_;
    uint64_t size_;
    uint64_t hash_;
  };

  FakeRenderer();

  ~FakeRenderer() override;

  // Binds the renderer.
  void Bind(InterfaceRequest<MediaRenderer> renderer_request);

  // Sets the demand min_packets_outstanding.
  void ConfigureDemand(uint32_t min_packets_outstanding) {
    demand_min_packets_outstanding_ = min_packets_outstanding;
  }

  // Indicates that the renderer should print out supplied packet info.
  void DumpPackets() { dump_packets_ = true; }

  // Indicates that the renderer should verify supplied packets against the
  // indicated PacketInfos.
  void ExpectPackets(const std::vector<PacketInfo>&& expected_packets_info) {
    expected_packets_info_ = std::move(expected_packets_info);
    expected_packets_info_iter_ = expected_packets_info_.begin();
  }

  // Returns true if everything has gone as expected so far.
  bool expected() { return expected_; }

 private:
  // MediaRenderer implementation.
  void GetSupportedMediaTypes(
      const GetSupportedMediaTypesCallback& callback) override;

  void SetMediaType(MediaTypePtr media_type) override;

  void GetPacketConsumer(
      InterfaceRequest<MediaPacketConsumer> packet_consumer_request) override;

  void GetTimelineControlPoint(InterfaceRequest<MediaTimelineControlPoint>
                                   control_point_request) override;

  // MediaPacketConsumerBase overrides.
  void OnPacketSupplied(
      std::unique_ptr<SuppliedPacket> supplied_packet) override;

  void OnFlushRequested(const FlushCallback& callback) override;

  void OnFailure() override;

  // MediaTimelineControlPoint implementation.
  void GetStatus(uint64_t version_last_seen,
                 const GetStatusCallback& callback) override;

  void GetTimelineConsumer(
      InterfaceRequest<TimelineConsumer> timeline_consumer_request) override;

  void Prime(const PrimeCallback& callback) override;

  // TimelineConsumer implementation.
  void SetTimelineTransform(
      TimelineTransformPtr timeline_transform,
      const SetTimelineTransformCallback& callback) override;

  // Clears the pending timeline function and calls its associated callback
  // with the indicated completed status.
  void ClearPendingTimelineFunction(bool completed);

  // Apply a pending timeline change if there is one an it's due.
  void MaybeApplyPendingTimelineChange(int64_t reference_time);

  // Sends status updates to waiting callers of GetStatus.
  void SendStatusUpdates();

  // Calls the callback with the current status.
  void CompleteGetStatus(const GetStatusCallback& callback);

  uint32_t demand_min_packets_outstanding_ = 1;
  bool dump_packets_ = false;
  std::vector<PacketInfo> expected_packets_info_;
  std::vector<PacketInfo>::iterator expected_packets_info_iter_;

  Binding<MediaRenderer> renderer_binding_;
  Binding<MediaTimelineControlPoint> control_point_binding_;
  Binding<TimelineConsumer> timeline_consumer_binding_;
  std::queue<std::unique_ptr<SuppliedPacket>> packet_queue_;
  TimelineFunction current_timeline_function_;
  TimelineFunction pending_timeline_function_;
  SetTimelineTransformCallback set_timeline_transform_callback_;
  bool end_of_stream_ = false;
  uint64_t status_version_ = 1u;
  std::vector<GetStatusCallback> pending_status_callbacks_;

  bool expected_ = true;
};

}  // namespace media
}  // namespace moj

#endif  // APPS_MEDIA_SERVICES_MEDIA_SERVICE_TEST_FAKE_RENDERER_H_
