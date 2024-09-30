/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include "pubsubworkflowfactory.h"

using namespace workflow;

namespace pub_sub {

PubSubWorkflowFactory::PubSubWorkflowFactory()
: IRunnerFactory() {
  name_ = "Pub/Sub Factory";
  description_ = "Public/Subscribe workflow task templates.";

  // Todo: Create a list of templates


}
std::unique_ptr<workflow::IRunner> PubSubWorkflowFactory::CreateRunner(const workflow::IRunner &source) const {
  std::unique_ptr<IRunner> task;
  const auto& template_name = source.Template();
  return task;
}

} // pub_sub