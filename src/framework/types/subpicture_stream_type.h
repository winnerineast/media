// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

#include "apps/media/src/framework/types/stream_type.h"

namespace media {

// Describes the type of a subpicture stream.
class SubpictureStreamType : public StreamType {
 public:
  static std::unique_ptr<StreamType> Create(
      const std::string& encoding,
      std::unique_ptr<Bytes> encoding_parameters) {
    return std::unique_ptr<StreamType>(
        new SubpictureStreamType(encoding, std::move(encoding_parameters)));
  }

  SubpictureStreamType(const std::string& encoding,
                       std::unique_ptr<Bytes> encoding_parameters);

  ~SubpictureStreamType() override;

  const SubpictureStreamType* subpicture() const override;

  std::unique_ptr<StreamType> Clone() const override;
};

// Describes a set of subpicture stream types.
class SubpictureStreamTypeSet : public StreamTypeSet {
 public:
  static std::unique_ptr<StreamTypeSet> Create(
      const std::vector<std::string>& encodings) {
    return std::unique_ptr<StreamTypeSet>(
        new SubpictureStreamTypeSet(encodings));
  }

  SubpictureStreamTypeSet(const std::vector<std::string>& encodings);

  ~SubpictureStreamTypeSet() override;

  const SubpictureStreamTypeSet* subpicture() const override;

  std::unique_ptr<StreamTypeSet> Clone() const override;
};

}  // namespace media
