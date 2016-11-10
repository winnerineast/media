// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/media/cpp/media_packet_consumer_base.h"

#include "lib/ftl/logging.h"

namespace media {

#if defined(FLOG_ENABLED)

namespace {

// Gets the size of a shared buffer.
uint64_t SizeOf(const mx::vmo& vmo) {
  uint64_t size;
  mx_status_t status = vmo.get_size(&size);

  if (status != NO_ERROR) {
    FTL_LOG(ERROR) << "mx::vmo::get_size failed, status " << status;
    return 0;
  }

  return size;
}

}  // namespace

#endif  // !defined(FLOG_ENABLED)

// For checking preconditions when handling fidl requests.
// Checks the condition, and, if it's false, calls Fail and returns.
#define RCHECK(condition, message) \
  if (!(condition)) {              \
    FTL_DLOG(ERROR) << message;    \
    Fail();                        \
    return;                        \
  }

MediaPacketConsumerBase::MediaPacketConsumerBase() : binding_(this) {
  Reset();
  FTL_DCHECK(counter_);
}

MediaPacketConsumerBase::~MediaPacketConsumerBase() {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif

  // Prevent the counter from calling us back.
  counter_->Detach();

  if (binding_.is_bound()) {
    binding_.Close();
  }
}

void MediaPacketConsumerBase::Bind(
    fidl::InterfaceRequest<MediaPacketConsumer> request) {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler([this]() { Reset(); });
}

bool MediaPacketConsumerBase::is_bound() {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  return binding_.is_bound();
}

void MediaPacketConsumerBase::SetDemand(uint32_t min_packets_outstanding,
                                        int64_t min_pts) {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  if (min_packets_outstanding == demand_.min_packets_outstanding &&
      min_pts == demand_.min_pts) {
    // Demand hasn't changed. Nothing to do.
    return;
  }

  demand_.min_packets_outstanding = min_packets_outstanding;
  demand_.min_pts = min_pts;

  FLOG(log_channel_, DemandSet(demand_.Clone()));
  demand_update_required_ = true;

  MaybeCompletePullDemandUpdate();
}

void MediaPacketConsumerBase::Reset() {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  FLOG(log_channel_, Reset());
  if (binding_.is_bound()) {
    binding_.Close();
  }

  demand_.min_packets_outstanding = 0;
  demand_.min_pts = MediaPacket::kNoTimestamp;

  get_demand_update_callback_ = nullptr;

  if (counter_) {
    counter_->Detach();
  }
  counter_ = std::make_shared<SuppliedPacketCounter>(this);
}

void MediaPacketConsumerBase::Fail() {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  FLOG(log_channel_, Failed());
  Reset();
  OnFailure();
}

void MediaPacketConsumerBase::OnPacketReturning() {}

void MediaPacketConsumerBase::OnFlushRequested(const FlushCallback& callback) {
  callback();
}

void MediaPacketConsumerBase::OnFailure() {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
}

void MediaPacketConsumerBase::PullDemandUpdate(
    const PullDemandUpdateCallback& callback) {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  if (get_demand_update_callback_) {
    // There's already a pending request. This isn't harmful, but it indicates
    // that the client doesn't know what it's doing.
    FTL_DLOG(WARNING) << "PullDemandUpdate was called when another "
                         "PullDemandUpdate call was pending";
    FLOG(log_channel_, RespondingToGetDemandUpdate(demand_.Clone()));
    get_demand_update_callback_(demand_.Clone());
  }

  get_demand_update_callback_ = callback;

  MaybeCompletePullDemandUpdate();
}

void MediaPacketConsumerBase::AddPayloadBuffer(uint32_t payload_buffer_id,
                                               mx::vmo payload_buffer) {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  FTL_DCHECK(payload_buffer);
  FLOG(log_channel_,
       AddPayloadBufferRequested(payload_buffer_id, SizeOf(payload_buffer)));
  mx_status_t status = counter_->buffer_set().AddBuffer(
      payload_buffer_id, std::move(payload_buffer));
  RCHECK(status == NO_ERROR, "failed to map buffer");
}

void MediaPacketConsumerBase::RemovePayloadBuffer(uint32_t payload_buffer_id) {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  FLOG(log_channel_, RemovePayloadBufferRequested(payload_buffer_id));
  counter_->buffer_set().RemoveBuffer(payload_buffer_id);
}

void MediaPacketConsumerBase::SupplyPacket(
    MediaPacketPtr media_packet,
    const SupplyPacketCallback& callback) {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  FTL_DCHECK(media_packet);

  void* payload;
  if (media_packet->payload_size == 0) {
    payload = nullptr;
  } else {
    RCHECK(counter_->buffer_set().Validate(
               SharedBufferSet::Locator(media_packet->payload_buffer_id,
                                        media_packet->payload_offset),
               media_packet->payload_size),
           "invalid buffer region");
    payload = counter_->buffer_set().PtrFromLocator(SharedBufferSet::Locator(
        media_packet->payload_buffer_id, media_packet->payload_offset));
  }

  uint64_t label = ++prev_packet_label_;
  FLOG(log_channel_,
       PacketSupplied(label, media_packet.Clone(), FLOG_ADDRESS(payload),
                      counter_->packets_outstanding() + 1));

  SetPacketPtsRate(media_packet);

  OnPacketSupplied(std::unique_ptr<SuppliedPacket>(new SuppliedPacket(
      label, std::move(media_packet), payload, callback, counter_)));
}

void MediaPacketConsumerBase::Flush(const FlushCallback& callback) {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  FLOG(log_channel_, FlushRequested());

  demand_.min_packets_outstanding = 0;
  demand_.min_pts = MediaPacket::kNoTimestamp;

  OnFlushRequested([this, callback]() {
    FLOG(log_channel_, CompletingFlush());
    callback();
  });
}

void MediaPacketConsumerBase::MaybeCompletePullDemandUpdate() {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  // If we're in the middle of returning a packet, we want to use the
  // SupplyPacket callback for demand updates rather than the PullDemandUpdate
  // callback.
  if (!demand_update_required_ || returning_packet_ ||
      !get_demand_update_callback_) {
    return;
  }

  FLOG(log_channel_, RespondingToGetDemandUpdate(demand_.Clone()));
  demand_update_required_ = false;
  get_demand_update_callback_(demand_.Clone());
  get_demand_update_callback_ = nullptr;
}

MediaPacketDemandPtr MediaPacketConsumerBase::GetDemandForPacketDeparture(
    uint64_t label) {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif

  FLOG(log_channel_, ReturningPacket(label, counter_->packets_outstanding()));

  // Note that we're returning a packet so that MaybeCompletePullDemandUpdate
  // won't try to send a packet update via a PullDemandUpdate callback.
  returning_packet_ = true;
  // This is the subclass's chance to SetDemand.
  OnPacketReturning();
  returning_packet_ = false;

  if (!demand_update_required_) {
    return nullptr;
  }

  demand_update_required_ = false;
  return demand_.Clone();
}

void MediaPacketConsumerBase::SetPacketPtsRate(const MediaPacketPtr& packet) {
  if (pts_rate_ == TimelineRate::Zero) {
    return;
  }

  TimelineRate original_rate(packet->pts_rate_ticks, packet->pts_rate_seconds);

  if (original_rate != pts_rate_) {
    // We're asking for an inexact product here, because, in some cases,
    // pts_rate_ / original_rate can't be represented exactly as a TimelineRate.
    // Using this approach produces small errors in the resulting pts in those
    // cases.
    // TODO(dalesat): Do the 128-bit calculation required to do this exactly.
    packet->pts = packet->pts * TimelineRate::Product(
                                    pts_rate_, original_rate.Inverse(), false);
    packet->pts_rate_ticks = pts_rate_.subject_delta();
    packet->pts_rate_seconds = pts_rate_.reference_delta();
  }
}

MediaPacketConsumerBase::SuppliedPacket::SuppliedPacket(
    uint64_t label,
    MediaPacketPtr packet,
    void* payload,
    const SupplyPacketCallback& callback,
    std::shared_ptr<SuppliedPacketCounter> counter)
    : label_(label),
      packet_(std::move(packet)),
      payload_(payload),
      callback_(callback),
      counter_(counter) {
  FTL_DCHECK(packet_);
  FTL_DCHECK(callback);
  FTL_DCHECK(counter_);
  counter_->OnPacketArrival();
}

MediaPacketConsumerBase::SuppliedPacket::~SuppliedPacket() {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
  callback_(counter_->OnPacketDeparture(label_));
}

MediaPacketConsumerBase::SuppliedPacketCounter::SuppliedPacketCounter(
    MediaPacketConsumerBase* owner)
    : owner_(owner) {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
}

MediaPacketConsumerBase::SuppliedPacketCounter::~SuppliedPacketCounter() {
#ifndef NDEBUG
  FTL_DCHECK(thread_checker_.IsCreationThreadCurrent());
#endif
}

}  // namespace media
