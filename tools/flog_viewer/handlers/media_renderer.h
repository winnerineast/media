// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>

#include "apps/media/services/logs/media_renderer_channel.fidl.h"
#include "apps/media/tools/flog_viewer/accumulator.h"
#include "apps/media/tools/flog_viewer/channel_handler.h"
#include "apps/media/tools/flog_viewer/counted.h"
#include "apps/media/tools/flog_viewer/tracked.h"

namespace flog {
namespace handlers {

class MediaRendererAccumulator;

// Handler for MediaRendererChannel messages.
class MediaRenderer : public ChannelHandler,
                      public media::logs::MediaRendererChannel {
 public:
  MediaRenderer(const std::string& format);

  ~MediaRenderer() override;

  std::shared_ptr<Accumulator> GetAccumulator() override;

 protected:
  // ChannelHandler overrides.
  void HandleMessage(fidl::Message* message) override;

 private:
  // MediaRendererChannel implementation.
  void BoundAs(uint64_t koid) override;

  void Config(fidl::Array<media::MediaTypeSetPtr> supported_types,
              uint64_t consumer_address,
              uint64_t timeline_control_point_address) override;

  void SetMediaType(media::MediaTypePtr type) override;

  void EngagePacket(int64_t current_pts,
                    int64_t packet_pts,
                    uint64_t packet_label) override;

 private:
  media::logs::MediaRendererChannelStub stub_;
  std::shared_ptr<MediaRendererAccumulator> accumulator_;
};

// Status of a media renderer as understood by MediaRenderer.
class MediaRendererAccumulator : public Accumulator {
 public:
  MediaRendererAccumulator();
  ~MediaRendererAccumulator() override;

  // Accumulator overrides.
  void Print(std::ostream& os) override;

 private:
  fidl::Array<media::MediaTypeSetPtr> supported_types_;
  std::shared_ptr<Channel> consumer_channel_;
  std::shared_ptr<Channel> timeline_control_point_channel_;
  media::MediaTypePtr type_;
  Counted preroll_packets_;
  Tracked packet_earliness_ns_;
  Counted starved_no_packet_;
  Tracked starved_ns_;
  Counted missing_packets_;

  friend class MediaRenderer;
};

}  // namespace handlers
}  // namespace flog
