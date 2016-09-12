// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MEDIA_SERVICES_FRAMEWORK_MOJO_MOJO_PACKET_PRODUCER_H_
#define APPS_MEDIA_SERVICES_FRAMEWORK_MOJO_MOJO_PACKET_PRODUCER_H_

#include "apps/media/cpp/media_packet_producer_base.h"
#include "apps/media/interfaces/media_transport.mojom.h"
#include "apps/media/services/framework/models/active_sink.h"
#include "apps/media/services/framework/payload_allocator.h"
#include "lib/ftl/tasks/task_runner.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mojo {
namespace media {

// Implements MediaPacketProducer to forward a stream across mojo.
class MojoPacketProducer : private MediaPacketProducerBase,
                           public MediaPacketProducer,
                           public ActiveSink,
                           public PayloadAllocator {
 public:
  using FlushConnectionCallback = mojo::Callback<void()>;

  static std::shared_ptr<MojoPacketProducer> Create() {
    return std::shared_ptr<MojoPacketProducer>(new MojoPacketProducer());
  }

  ~MojoPacketProducer() override;

  // Binds.
  void Bind(InterfaceRequest<MediaPacketProducer> request);

  // Flushes and tells the connected consumer to flush.
  void FlushConnection(const FlushConnectionCallback& callback);

  // ActiveSink implementation.
  PayloadAllocator* allocator() override;

  void SetDemandCallback(const DemandCallback& demand_callback) override;

  Demand SupplyPacket(PacketPtr packet) override;

  // MediaPacketProducer implementation.
  void Connect(InterfaceHandle<MediaPacketConsumer> consumer,
               const ConnectCallback& callback) override;

  void Disconnect() override;

  // PayloadAllocator implementation:
  void* AllocatePayloadBuffer(size_t size) override;

  // Releases a buffer previously allocated via AllocatePayloadBuffer.
  void ReleasePayloadBuffer(void* buffer) override;

 protected:
  // MediaPacketProducerBase overrides.
  void OnDemandUpdated(uint32_t min_packets_outstanding,
                       int64_t min_pts) override;

 private:
  MojoPacketProducer();

  // Sends a packet to the consumer.
  void SendPacket(PacketPtr packet);

  // Shuts down the producer.
  void Reset();

  // Determines the current demand. The |additional_packets_outstanding|
  // parameter indicates the number of packets that should be added to the
  // current outstanding packet count when determining demand. For example, a
  // value of 1 means that the function should determine demand as if one
  // additional packet was outstanding.
  Demand CurrentDemand(uint32_t additional_packets_outstanding = 0);

  Binding<MediaPacketProducer> binding_;

  DemandCallback demand_callback_;
  ftl::TaskRunner* task_runner_;
};

}  // namespace media
}  // namespace mojo

#endif  // APPS_MEDIA_SERVICES_FRAMEWORK_MOJO_MOJO_PACKET_PRODUCER_H_
