// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <unordered_map>

#include "apps/media/services/logs/media_packet_producer_channel.fidl.h"
#include "apps/media/tools/flog_viewer/accumulator.h"
#include "apps/media/tools/flog_viewer/binding.h"
#include "apps/media/tools/flog_viewer/channel_handler.h"
#include "apps/media/tools/flog_viewer/counted.h"
#include "apps/media/tools/flog_viewer/tracked.h"

namespace flog {
namespace handlers {

class MediaPacketProducerAccumulator;

// Handler for MediaPacketProducerChannel messages.
class MediaPacketProducer : public ChannelHandler,
                            public media::logs::MediaPacketProducerChannel {
 public:
  MediaPacketProducer(const std::string& format);

  ~MediaPacketProducer() override;

  std::shared_ptr<Accumulator> GetAccumulator() override;

 protected:
  // ChannelHandler overrides.
  void HandleMessage(fidl::Message* message) override;

 private:
  // MediaPacketProducerChannel implementation.
  void ConnectedTo(uint64_t related_koid) override;

  void Resetting() override;

  void RequestingFlush() override;

  void FlushCompleted() override;

  void PayloadBufferAllocationFailure(uint32_t index, uint64_t size) override;

  void DemandUpdated(media::MediaPacketDemandPtr demand) override;

  void ProducingPacket(uint64_t label,
                       media::MediaPacketPtr packet,
                       uint64_t payload_address,
                       uint32_t packets_outstanding) override;

  void RetiringPacket(uint64_t label, uint32_t packets_outstanding) override;

 private:
  media::logs::MediaPacketProducerChannelStub stub_;
  std::shared_ptr<MediaPacketProducerAccumulator> accumulator_;
};

// Status of a media packet producer as understood by MediaPacketProducer.
class MediaPacketProducerAccumulator : public Accumulator {
 public:
  MediaPacketProducerAccumulator();
  ~MediaPacketProducerAccumulator() override;

  // Accumulator overrides.
  void Print(std::ostream& os) override;

 private:
  struct Packet {
    static std::shared_ptr<Packet> Create(uint64_t label,
                                          media::MediaPacketPtr packet,
                                          uint64_t payload_address,
                                          uint32_t packets_outstanding) {
      return std::make_shared<Packet>(label, std::move(packet), payload_address,
                                      packets_outstanding);
    }

    Packet(uint64_t label,
           media::MediaPacketPtr packet,
           uint64_t payload_address,
           uint32_t packets_outstanding)
        : label_(label),
          packet_(std::move(packet)),
          payload_address_(payload_address),
          packets_outstanding_(packets_outstanding) {}

    uint64_t label_;
    media::MediaPacketPtr packet_;
    uint64_t payload_address_;
    uint32_t packets_outstanding_;
  };

  struct Allocation {
    Allocation(uint32_t index, uint64_t size, uint64_t buffer)
        : index_(index), size_(size), buffer_(buffer) {}
    uint32_t index_;
    uint64_t size_;
    uint64_t buffer_;
  };

  PeerBinding consumer_;
  Counted flush_requests_;
  media::MediaPacketDemandPtr current_demand_;
  uint32_t min_packets_outstanding_highest_ = 0;

  std::unordered_map<uint64_t, std::shared_ptr<Packet>> outstanding_packets_;
  Tracked packets_;

  friend class MediaPacketProducer;
};

}  // namespace handlers
}  // namespace flog
