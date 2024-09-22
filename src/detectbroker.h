/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "mqttclient.h"

namespace pub_sub {
    /**
     * @class DetectBroker
     * @brief The class implements a simple client that detect if a broker exist.
     *
     * The class shall be used to detect an MQTT broker. The user should add the
     * address, optional the IP port and the transport layer.
     */
    class DetectBroker : public MqttClient {
      bool Start() override;

    };

} // pub_sub

