// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/media/src/fidl/fidl_packet_producer.h"

#include "lib/ftl/functional/make_copyable.h"
#include "lib/ftl/logging.h"
#include "lib/mtl/tasks/message_loop.h"

namespace media {

FidlPacketProducer::FidlPacketProducer() : binding_(this) {
  task_runner_ = mtl::MessageLoop::GetCurrent()->task_runner();
  FTL_DCHECK(task_runner_);
}

FidlPacketProducer::~FidlPacketProducer() {}

void FidlPacketProducer::Bind(
    fidl::InterfaceRequest<MediaPacketProducer> request) {
  binding_.Bind(std::move(request));
}

void FidlPacketProducer::FlushConnection(
    const FlushConnectionCallback& callback) {
  if (is_connected()) {
    FlushConsumer(callback);
  } else {
    callback();
  }
}

PayloadAllocator* FidlPacketProducer::allocator() {
  return this;
}

void FidlPacketProducer::SetDemandCallback(
    const DemandCallback& demand_callback) {
  demand_callback_ = demand_callback;
}

Demand FidlPacketProducer::SupplyPacket(PacketPtr packet) {
  FTL_DCHECK(packet);

  bool end_of_stream = packet->end_of_stream();

  // If we're not connected, throw the packet away.
  if (!is_connected()) {
    return end_of_stream ? Demand::kNegative : CurrentDemand();
  }

  task_runner_->PostTask(ftl::MakeCopyable([
    weak_this = std::weak_ptr<FidlPacketProducer>(shared_from_this()),
    packet = std::move(packet)
  ]() mutable {
    auto shared_this = weak_this.lock();
    if (shared_this) {
      shared_this->SendPacket(std::move(packet));
    }
  }));

  return end_of_stream ? Demand::kNegative : CurrentDemand(1);
}

void FidlPacketProducer::Connect(
    fidl::InterfaceHandle<MediaPacketConsumer> consumer,
    const ConnectCallback& callback) {
  FTL_DCHECK(consumer);
  MediaPacketProducerBase::Connect(
      MediaPacketConsumerPtr::Create(std::move(consumer)), callback);
}

void FidlPacketProducer::Disconnect() {
  if (demand_callback_) {
    demand_callback_(Demand::kNegative);
  }

  MediaPacketProducerBase::Disconnect();
}

void* FidlPacketProducer::AllocatePayloadBuffer(size_t size) {
  return MediaPacketProducerBase::AllocatePayloadBuffer(size);
}

void FidlPacketProducer::ReleasePayloadBuffer(void* buffer) {
  MediaPacketProducerBase::ReleasePayloadBuffer(buffer);
}

void FidlPacketProducer::OnDemandUpdated(uint32_t min_packets_outstanding,
                                         int64_t min_pts) {
  FTL_DCHECK(demand_callback_);
  demand_callback_(CurrentDemand());
}

void FidlPacketProducer::SendPacket(PacketPtr packet) {
  FTL_DCHECK(packet);

  ProducePacket(packet->payload(), packet->size(), packet->pts(),
                packet->pts_rate(), packet->end_of_stream(),
                ftl::MakeCopyable([ this, packet = std::move(packet) ]() {
                  FTL_DCHECK(demand_callback_);
                  demand_callback_(CurrentDemand());
                }));
}

void FidlPacketProducer::Reset() {
  if (binding_.is_bound()) {
    binding_.Close();
  }

  MediaPacketProducerBase::Reset();
}

Demand FidlPacketProducer::CurrentDemand(
    uint32_t additional_packets_outstanding) {
  if (!is_connected()) {
    return Demand::kNeutral;
  }

  // ShouldProducePacket tells us whether we should produce a packet based on
  // demand the consumer has expressed using fidl packet transport demand
  // semantics (min_packets_outstanding, min_pts). We need to translate this
  // into the producer's demand using framework demand semantics
  // (positive/neutral/negative).
  //
  // If we should send a packet, the producer signals positive demand so that
  // upstream components will deliver the needed packet. If we shouldn't send a
  // packet, the producer signals negative demand to prevent new packets from
  // arriving at the producer.
  //
  // If we express neutral demand instead of negative demand, packets would flow
  // freely downstream even though they're not demanded by the consumer. In
  // multistream (e.g audio/video) scenarios, this would cause serious problems.
  // If the demux has to produce a bunch of undemanded video packets in order to
  // find a demanded audio packet, neutral demand here would cause those video
  // packets to flow downstream, get decoded and queue up at the video renderer.
  // This wastes memory, because the decoded frames are so large. We would
  // rather the demux keep the undemanded video packets until they're demanded
  // so we get only the decoded frames we need, hence negative demand here.
  return ShouldProducePacket(additional_packets_outstanding)
             ? Demand::kPositive
             : Demand::kNegative;
}

}  // namespace media
