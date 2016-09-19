// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_FLOG_VIEWER_FORMATTING_H_
#define EXAMPLES_FLOG_VIEWER_FORMATTING_H_

#include <ostream>

#include "examples/flog_viewer/channel.h"
#include "mojo/services/flog/interfaces/flog.mojom.h"

//
// This file declares a bunch of << operator overloads for formatting stuff.
// Unless you want to add new operators, it's sufficient to know that you can
// just use the operators as expected, except that some of the overloads can
// produce multiple lines and therefore provide their own newlines.
//
// These operators are intended to be called after a label has been added to
// the stream with a trailing space. If the text generated by an operator is
// sufficiently short, the operator may add that text with no preamble and
// terminate it with std::endl. If the text has to be multiline, the operator
// first adds std::endl, then the multiline text with std::endl termination.
// Each line starts with begl in order to apply the appropriate indentation.
// The Indenter class is provided to adjust the identation level. Operators
// that take pointers need to handle nullptr.
//

namespace mojo {
namespace flog {
namespace examples {

int ostream_indent_index();

std::ostream& begl(std::ostream& os);

inline std::ostream& indent(std::ostream& os) {
  ++os.iword(ostream_indent_index());
  return os;
}

inline std::ostream& outdent(std::ostream& os) {
  --os.iword(ostream_indent_index());
  return os;
}

struct AsAddress {
  explicit AsAddress(uint64_t address) : address_(address) {}
  uint64_t address_;
};

std::ostream& operator<<(std::ostream& os, AsAddress value);

struct AsNiceDateTime {
  explicit AsNiceDateTime(uint64_t time_us) : time_us_(time_us) {}
  uint64_t time_us_;
};

std::ostream& operator<<(std::ostream& os, AsNiceDateTime value);

struct AsMicroseconds {
  explicit AsMicroseconds(uint64_t time_us) : time_us_(time_us) {}
  uint64_t time_us_;
};

std::ostream& operator<<(std::ostream& os, AsMicroseconds value);

struct AsLogLevel {
  explicit AsLogLevel(uint32_t level) : level_(level) {}
  uint32_t level_;
};

std::ostream& operator<<(std::ostream& os, AsLogLevel value);

std::ostream& operator<<(std::ostream& os, const Channel& value);

}  // namespace examples

std::ostream& operator<<(std::ostream& os, const FlogEntryPtr& value);

}  // namespace flog
}  // namespace mojo

#endif  // EXAMPLES_FLOG_VIEWER_FORMATTING_H_
