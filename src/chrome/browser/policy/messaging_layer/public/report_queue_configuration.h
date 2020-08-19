// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_CONFIGURATION_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_CONFIGURATION_H_

#include <memory>

#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/proto/record_constants.pb.h"

namespace reporting {

// ReportQueueConfiguration configures a report queue.
// |dm_token| will be attached to all records generated with this queue.
// |destination| indicates what server side handler will be handling
// the records that are generated by the ReportQueue.
// |priority| indicates the priority of the ReportQueue.
class ReportQueueConfiguration {
 public:
  ~ReportQueueConfiguration() = default;
  ReportQueueConfiguration(const ReportQueueConfiguration& other) = delete;
  ReportQueueConfiguration& operator=(const ReportQueueConfiguration& other) =
      delete;

  // Factory for generating a ReportQueueConfiguration.
  // If any of the parameters are invalid, will return error::INVALID_ARGUMENT.
  // |dm_token| is valid when dm_token.is_valid() is true.
  // |destination| is valid when it is any value other than
  // Destination::UNDEFINED_DESTINATION.
  // |priority| is valid when it is any value other than
  // Priority::UNDEFINED_PRIORITY.
  static StatusOr<std::unique_ptr<ReportQueueConfiguration>> Create(
      const policy::DMToken& dm_token,
      reporting_messaging_layer::Destination destination,
      reporting_messaging_layer::Priority priority);

  reporting_messaging_layer::Destination destination() const {
    return destination_;
  }

  reporting_messaging_layer::Priority priority() const { return priority_; }

  policy::DMToken dm_token() const { return dm_token_; }

 private:
  ReportQueueConfiguration() = default;

  Status SetDMToken(const policy::DMToken& dm_token);
  Status SetDestination(reporting_messaging_layer::Destination destination);
  Status SetPriority(reporting_messaging_layer::Priority priority);

  policy::DMToken dm_token_;
  reporting_messaging_layer::Destination destination_;
  reporting_messaging_layer::Priority priority_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_QUEUE_CONFIGURATION_H_