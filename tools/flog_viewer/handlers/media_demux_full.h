// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MEDIA_TOOLS_FLOG_VIEWER_HANDLERS_MEDIA_DEMUX_FULL_H_
#define APPS_MEDIA_TOOLS_FLOG_VIEWER_HANDLERS_MEDIA_DEMUX_FULL_H_

#include "apps/media/interfaces/logs/media_demux_channel.mojom.h"
#include "apps/media/tools/flog_viewer/channel_handler.h"

namespace mojo {
namespace flog {
namespace handlers {

// Handler for MediaDemuxChannel messages.
class MediaDemuxFull : public ChannelHandler,
                       public mojo::media::logs::MediaDemuxChannel {
 public:
  MediaDemuxFull(const std::string& format);

  ~MediaDemuxFull() override;

  // ChannelHandler implementation.
  void HandleMessage(Message* message) override;

  // MediaDemuxChannel implementation.
  void NewStream(uint32_t index,
                 mojo::media::MediaTypePtr type,
                 uint64_t producer_address) override;

 private:
  mojo::media::logs::MediaDemuxChannelStub stub_;
  bool terse_;
};

}  // namespace handlers
}  // namespace flog
}  // namespace mojo

#endif  // APPS_MEDIA_TOOLS_FLOG_VIEWER_HANDLERS_MEDIA_DEMUX_FULL_H_
