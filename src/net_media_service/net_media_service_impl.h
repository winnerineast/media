// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "application/lib/app/application_context.h"
#include "apps/media/services/net_media_service.fidl.h"
#include "apps/media/src/util/factory_service_base.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/ftl/macros.h"

namespace media {

class NetMediaServiceImpl : public FactoryServiceBase, public NetMediaService {
 public:
  NetMediaServiceImpl();

  ~NetMediaServiceImpl() override;

  // NetMediaService implementation.
  void CreateNetMediaPlayer(
      const fidl::String& service_name,
      fidl::InterfaceHandle<MediaPlayer> media_player,
      fidl::InterfaceRequest<NetMediaPlayer> net_media_player_request) override;

  void CreateNetMediaPlayerProxy(
      const fidl::String& device_name,
      const fidl::String& service_name,
      fidl::InterfaceRequest<NetMediaPlayer> net_media_player_request) override;

 private:
  fidl::BindingSet<NetMediaService> bindings_;

  FTL_DISALLOW_COPY_AND_ASSIGN(NetMediaServiceImpl);
};

}  // namespace media
