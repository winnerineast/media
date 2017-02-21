// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/media/tools/flog_viewer/binding.h"

#include "apps/media/tools/flog_viewer/channel.h"
#include "lib/ftl/logging.h"

namespace flog {

Binding::Binding() {}

Binding::~Binding() {}

void Binding::SetChannel(std::shared_ptr<Channel> channel) {
  FTL_DCHECK(channel);
  FTL_DCHECK(!channel_);

  channel_ = channel;
}

ChildBinding::ChildBinding() : Binding() {}

ChildBinding::~ChildBinding() {}

void ChildBinding::SetChannel(std::shared_ptr<Channel> channel) {
  Binding::SetChannel(channel);
  FTL_DCHECK(!channel->has_parent());
  channel->SetHasParent();
}

PeerBinding::PeerBinding() : Binding() {}

PeerBinding::~PeerBinding() {}

}  // namespace flog
