// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>

#include "lib/ftl/command_line.h"
#include "lib/ftl/macros.h"

namespace examples {

class VuMeterParams {
 public:
  VuMeterParams(const ftl::CommandLine& command_line);

  bool is_valid() const { return is_valid_; }

 private:
  bool is_valid_;

  FTL_DISALLOW_COPY_AND_ASSIGN(VuMeterParams);
};

}  // namespace examples
