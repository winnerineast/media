// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

#include "apps/media/cpp/flog.h"
#include "apps/media/interfaces/logs/media_decoder_channel.mojom.h"
#include "apps/media/interfaces/media_type_converter.mojom.h"
#include "apps/media/src/decode/decoder.h"
#include "apps/media/src/framework/graph.h"
#include "apps/media/src/media_service/media_service_impl.h"
#include "apps/media/src/mojo/mojo_packet_consumer.h"
#include "apps/media/src/mojo/mojo_packet_producer.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mojo {
namespace media {

// Mojo agent that decodes a stream.
class MediaDecoderImpl : public MediaServiceImpl::Product<MediaTypeConverter>,
                         public MediaTypeConverter {
 public:
  static std::shared_ptr<MediaDecoderImpl> Create(
      MediaTypePtr input_media_type,
      InterfaceRequest<MediaTypeConverter> request,
      MediaServiceImpl* owner);

  ~MediaDecoderImpl() override;

  // MediaTypeConverter implementation.
  void GetOutputType(const GetOutputTypeCallback& callback) override;

  void GetPacketConsumer(
      InterfaceRequest<MediaPacketConsumer> consumer) override;

  void GetPacketProducer(
      InterfaceRequest<MediaPacketProducer> producer) override;

 private:
  MediaDecoderImpl(MediaTypePtr input_media_type,
                   InterfaceRequest<MediaTypeConverter> request,
                   MediaServiceImpl* owner);

  Graph graph_;
  std::shared_ptr<MojoPacketConsumer> consumer_;
  std::shared_ptr<Decoder> decoder_;
  std::shared_ptr<MojoPacketProducer> producer_;

  FLOG_INSTANCE_CHANNEL(logs::MediaDecoderChannel, log_channel_);
};

}  // namespace media
}  // namespace mojo
