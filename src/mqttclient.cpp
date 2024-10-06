/*
 * Copyright 2022 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include "mqttclient.h"

#include <chrono>
#include <functional>


#include <util/logstream.h>
#include <util/utilfactory.h>


#include "mqtttopic.h"
#include "sparkplughelper.h"

using namespace std::chrono_literals;
using namespace util::log;


namespace {

} // end namespace

namespace pub_sub {
MqttClient::MqttClient()
: listen_(std::move(util::UtilFactory::CreateListen("ListenProxy", "LISMQTT"))) {
  ResetConnectionLost();

}
MqttClient::~MqttClient() {
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Stopping client");
  }
  MqttClient::Stop();
  for (auto& topic : topic_list_ ) {
    if (!topic) {
      continue;
    }
    topic->SetAllMetricsInvalid();
  }
  listen_.reset();
}

ITopic *MqttClient::CreateTopic() {

  auto topic = std::make_unique<MqttTopic>(*this);
  std::scoped_lock list_lock(topic_mutex_);

  topic_list_.emplace_back(std::move(topic));
  return topic_list_.back().get();
}


ITopic *MqttClient::AddMetric(const std::shared_ptr<Metric>& metric) {
  if (!metric || metric->Name().empty()) {
    LOG_ERROR() << "Cannot add a metric with no name.";
    return nullptr;
  }

  auto* topic = GetTopic(metric->Name()); // Note that this call adds the topic to its list.
  if ( topic == nullptr) {
    topic = CreateTopic();
    if (topic == nullptr) {
      LOG_ERROR() << "Failed to create a topic. Topic: " <<  metric->Name();
      return nullptr;
    }
    topic->Topic(metric->Name());

    topic->Publish(true);
  }

  // Set default value
  auto& payload = topic->GetPayload();
  const auto text = metric->GetMqttString();
  if (metric->IsNull()) {
    payload.StringToBody("");
  } else {
    payload.StringToBody(text);
  }

  payload.AddMetric(metric);

  return topic;
}

bool MqttClient::IsConnected() const {
  return handle_ != nullptr && MQTTAsync_isConnected(handle_);
}

bool MqttClient::Start() {
  InitMqtt();
  // Create the worker task
  stop_client_task_ = true;
  if (work_thread_.joinable()) {
    work_thread_.join();
  }

  ResetConnectionLost();
  client_timer_ = 0;
  stop_client_task_ = false;
  work_thread_ = std::thread(&MqttClient::ClientTask, this);

  client_event_.notify_one();
  return true;
}

bool MqttClient::CreateClient() {
  if (listen_ && !Name().empty()) {
    listen_->PreText(Name());
  }

  std::ostringstream connect_string;
  switch (Transport()) {
    case TransportLayer::MqttWebSocket:
      connect_string << "ws://";
      break;

    case TransportLayer::MqttTcpTls:
      connect_string << "ssl://";
      break;

    case TransportLayer::MqttWebSocketTls:
      connect_string << "wss://";
      break;

    default:
      connect_string << "tcp://";
      break;
  }
  connect_string << Broker() << ":" << Port();
  if (listen_->IsActive() && listen_) {
    listen_->ListenText("Creating client");
  }

  MQTTAsync_createOptions create_options = MQTTAsync_createOptions_initializer5;

  const auto create = MQTTAsync_createWithOptions(&handle_, connect_string.str().c_str(),
                                       Name().c_str(),
                                 MQTTCLIENT_PERSISTENCE_NONE, nullptr,
                                 Version() == ProtocolVersion::Mqtt5 ? &create_options : nullptr);
  if (create != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to create the MQTT handle.";
    const auto* cause = MQTTAsync_strerror(create);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }

    LOG_ERROR() << err.str();
    return false;
  }

  const auto callback = MQTTAsync_setCallbacks(handle_, this,
                                               OnConnectionLost,
                                               OnMessageArrived,
                                               OnDeliveryComplete);
  if (callback != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to set the MQTT callbacks.";
    const auto* cause = MQTTAsync_strerror(callback);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }

    LOG_ERROR() << err.str();
    return false;
  }
  if (listen_->IsActive() && listen_) {
    listen_->ListenText("Created client");
  }
  return true;
}

bool MqttClient::SendConnect() {

  // Reset the connection lost to detect any failing startup
  ResetConnectionLost();
  ResetDelivered();

  MQTTAsync_connectOptions connect_options = MQTTAsync_connectOptions_initializer;
  if (Version() == ProtocolVersion::Mqtt5) {
    connect_options = MQTTAsync_connectOptions_initializer5;
  }
  connect_options.keepAliveInterval = 10; // 10 seconds between keep alive messages
  // connect_options.cleansession = MQTTASYNC_TRUE;
  connect_options.connectTimeout = 5; // Wait max 5 seconds on connect.
  connect_options.onSuccess = OnConnect;
  connect_options.onFailure = OnConnectFailure;
  connect_options.context = this;
  if (Version() == ProtocolVersion::Mqtt5) {
    connect_options.MQTTVersion = MQTTVERSION_5;
    connect_options.onSuccess = nullptr;
    connect_options.onFailure = nullptr;
    connect_options.onSuccess5 = OnConnect5;
    connect_options.onFailure5 = OnConnectFailure5;
  }
  if (!username_.empty() && !password_.empty()) {
    connect_options.username = username_.c_str();
    connect_options.password = password_.c_str();
  }

  if (Transport() == TransportLayer::MqttTcpTls || Transport() == TransportLayer::MqttWebSocketTls) {
    InitSsl(); // Fill the ssl_options_ structure with values
    connect_options.ssl = &ssl_options_;
  }

  const auto connect = MQTTAsync_connect(handle_, &connect_options);
  if (connect != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to connect to the MQTT broker.";
    const auto* cause = MQTTAsync_strerror(connect);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }

    LOG_ERROR() << err.str();
    return false;
  }
  return true;
}

bool MqttClient::SendDisconnect() {

  ResetConnectionLost();
  ResetDelivered();
  MQTTAsync_disconnectOptions disconnect_options = MQTTAsync_disconnectOptions_initializer;
  if (Version() == ProtocolVersion::Mqtt5) {
    disconnect_options = MQTTAsync_disconnectOptions_initializer5;
    disconnect_options.onSuccess = nullptr;
    disconnect_options.onFailure = nullptr;
    disconnect_options.onSuccess5 = OnDisconnect5;
    disconnect_options.onFailure5 = OnDisconnectFailure5;
  } else {
    disconnect_options.onSuccess = OnDisconnect;
    disconnect_options.onFailure = OnDisconnectFailure;
  }
  disconnect_options.context = this;
  disconnect_options.timeout = 5000;

  const auto disconnect = MQTTAsync_disconnect(handle_, &disconnect_options);
  if (disconnect != MQTTASYNC_SUCCESS) {
    std::ostringstream err;
    err << "Failed to disconnect from the MQTT broker.";
    const auto* cause = MQTTAsync_strerror(disconnect);
    if (cause != nullptr && strlen(cause) > 0) {
      err << "Error: " << cause;
    }

    if (listen_ && listen_->IsActive()) {
      listen_->ListenText("%s", err.str().c_str());
    }

  }
  return disconnect == MQTTASYNC_SUCCESS;
}

bool MqttClient::Stop() {
  stop_client_task_ = true;
  client_event_.notify_one();
  if (work_thread_.joinable()) {
    work_thread_.join();
  }
  if (handle_ != nullptr) {
    MQTTAsync_destroy(&handle_);
    handle_ = nullptr;
  }

  return true;

}

void MqttClient::ConnectionLost(const std::string& cause) {
  std::ostringstream err;
  err << "Connection lost.";
  if (!cause.empty()) {
    err << " Error: " << cause;
  }

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str() );
  }
  SetConnectionLost();
}

void MqttClient::Message(const std::string& topic_name, const MQTTAsync_message& message) {
  if (topic_name.empty()) {
    return;
  }
  auto* topic = GetTopic(topic_name);
  if (topic == nullptr) {
    // The topic and its value do not exist. Create a topic and a metric for this topic

    topic = CreateTopic();
    if (topic != nullptr) {
      topic->Topic(topic_name);
      topic->Publish(false);
    }
  }
  if (topic == nullptr) {
    LOG_ERROR() << "Failed to create topic. Topic: " << topic_name;
    return;
  }

  auto& payload = topic->GetPayload();
  auto& body = payload.Body();
  try {
    body.resize(message.payloadlen, 0);
    if (message.payload != nullptr && message.payloadlen > 0) {
      std::memcpy(body.data(), message.payload, message.payloadlen);
    }
  } catch(const std::exception& err) {
    LOG_ERROR() << "Failed to parse payload. Topic: " << topic_name << ", Error: " << err.what();
    return;
  }

  auto metric = payload.GetMetric(topic_name);
  if (!metric) {
    metric = std::make_shared<Metric>(topic_name);
    metric->Timestamp(SparkplugHelper::NowMs());
    metric->Type(MetricType::String);
  }

  payload.Timestamp(SparkplugHelper::NowMs(), true);
  metric->Value(payload.BodyToString());
  metric->FireOnMessage();

  ResetConnectionLost();
  topic->Qos(static_cast<QualityOfService>(message.qos));
  topic->Retained(message.retained == 1);

  if (listen_ && listen_->IsActive() && listen_->LogLevel() != 1) {
    listen_->ListenText("Message: %s, Value: %s", topic_name.c_str(),
                                payload.BodyToString().c_str());
  }
}

void MqttClient::DeliveryComplete(MQTTAsync_token ) {
  ResetConnectionLost();
}

void MqttClient::Connect(const MQTTAsync_successData& response) {
  const auto& connect = response.alt.connect;
  const std::string server_url =  connect.serverURI != nullptr ? connect.serverURI : "";
  if (Name().empty()) {
    Name(server_url);
  }

  const int version = connect.MQTTVersion;
  switch (version) {
    case MQTTVERSION_3_1:
      Version(ProtocolVersion::Mqtt31);
      break;

    case MQTTVERSION_5:
      Version(ProtocolVersion::Mqtt5);
      break;

    case MQTTVERSION_3_1_1:
    case MQTTVERSION_DEFAULT:
    default:
      Version(ProtocolVersion::Mqtt311);
      break;

  }
  const int session_present = connect.sessionPresent;
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Connected: Server: %s, Version: %d, Session: %d",
                                server_url.c_str(), version, session_present);
  }
  ResetConnectionLost();
  SetDelivered();
  client_event_.notify_one();
}


void MqttClient::ConnectFailure(const  MQTTAsync_failureData* response) {
  std::ostringstream err;
  err << "Connect failure.";
  if (response != nullptr) {
    const auto code = response->code;
    const auto* cause = MQTTAsync_strerror(code);
    if (cause != nullptr && strlen(cause) > 0) {
      err << " Error: " << cause;
    }

  }
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str());
  }
  SetConnectionLost();
  SetDelivered();
  client_event_.notify_one();
}

void MqttClient::Connect5(const MQTTAsync_successData5& response) {
  const auto& connect = response.alt.connect;
  const std::string server_url =  connect.serverURI != nullptr ? connect.serverURI : "";
  if (Name().empty()) {
    Name(server_url);
  }

  const int version = connect.MQTTVersion;
  switch (version) {
    case MQTTVERSION_3_1:
      Version(ProtocolVersion::Mqtt31);
      break;

    case MQTTVERSION_5:
      Version(ProtocolVersion::Mqtt5);
      break;

    case MQTTVERSION_3_1_1:
    case MQTTVERSION_DEFAULT:
    default:
      Version(ProtocolVersion::Mqtt311);
      break;

  }
  const int session_present = connect.sessionPresent;
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("Connected: Server: %s, Version: %d, Session: %d",
                        server_url.c_str(), version, session_present);
  }
  ResetConnectionLost();
  SetDelivered();
  client_event_.notify_one();
}


void MqttClient::ConnectFailure5(const MQTTAsync_failureData5* response) {
  std::ostringstream err;
  err << "Connect failure.";
  if (response != nullptr) {
    const auto code = response->code;
    const auto* cause = MQTTAsync_strerror(code);
    if (cause != nullptr && strlen(cause) > 0) {
      err << " Error: " << cause;
    }

  }
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str());
  }
  SetConnectionLost();
  SetDelivered();
  client_event_.notify_one();
}

void MqttClient::SubscribeFailure(const MQTTAsync_failureData &response) {
  std::ostringstream err;
  err << "Subscribe Failure. Error: " << MQTTAsync_strerror(response.code);
  if (response.message != nullptr) {
    err << ". Message: " << response.message;
  }
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str() );
  }
  LOG_ERROR() << err.str();
}

void MqttClient::SubscribeFailure5(const MQTTAsync_failureData5 &response) {
  std::ostringstream err;
  err << "Subscribe Failure. Error: " << MQTTAsync_strerror(response.code);
  if (response.message != nullptr) {
    err << ". Message: " << response.message;
  }
  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str() );
  }
  LOG_ERROR() << err.str();
}

void MqttClient::Disconnect(const MQTTAsync_successData*) {
  SetDelivered();
  ResetConnectionLost();
  client_event_.notify_one();
}

void MqttClient::DisconnectFailure(const MQTTAsync_failureData* response) {
  std::ostringstream err;
  err << "Disconnect failure.";
  if (response != nullptr) {
    const int code = response->code;
    const auto* cause = MQTTAsync_strerror(code);
    if (cause != nullptr && strlen(cause) > 0) {
      err << " Error: " << cause;
    }
  }

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str());
  }
  SetConnectionLost();
  SetDelivered();
  client_event_.notify_one();
}

void MqttClient::Disconnect5(const MQTTAsync_successData5*) {
  SetDelivered();
  ResetConnectionLost();
  client_event_.notify_one(); // Speed up the disconnect
}

void MqttClient::DisconnectFailure5(const MQTTAsync_failureData5* response) {
  std::ostringstream err;
  err << "Disconnect failure.";
  if (response != nullptr) {
    const int code = response->code;
    const auto* cause = MQTTAsync_strerror(code);

    if (cause != nullptr && strlen(cause) > 0) {
      err << " Error: " << cause << ".";
    }
    if (response->message != nullptr) {
      err << " Message: " << response->message;
    }
  }

  if (listen_ && listen_->IsActive()) {
    listen_->ListenText("%s", err.str().c_str());
  }
  SetConnectionLost();
  SetDelivered();
  client_event_.notify_one();
}



void MqttClient::OnConnectionLost(void *context, char *cause) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  std::string reason = cause != nullptr ? cause : "";
  if (client != nullptr) {
    client->ConnectionLost(reason);
  }
  if (cause != nullptr) {
    MQTTAsync_free(cause);
  }
}

int MqttClient::OnMessageArrived(void* context, char* topic_name, int topicLen, MQTTAsync_message* message) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  const std::string topic_id = topic_name != nullptr && topicLen > 0 ? topic_name : "";

  if (client != nullptr && message != nullptr && !topic_id.empty()) {
    client->Message(topic_id, *message);
  }

  if (topic_name != nullptr) {
    MQTTAsync_free(topic_name);
  }
  if (message != nullptr) {
    MQTTAsync_freeMessage(&message);
  }
  return MQTTASYNC_TRUE;
}

void MqttClient::OnDeliveryComplete(void *context, MQTTAsync_token token) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr) {
    client->DeliveryComplete(token);
  }
}

void MqttClient::OnConnect(void* context, MQTTAsync_successData* response) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr && response != nullptr) {
    client->Connect(*response);
  }
}

void MqttClient::OnConnectFailure(void* context, MQTTAsync_failureData* response) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  if (client != nullptr) {
    client->ConnectFailure(response);
  }
}

void MqttClient::OnConnect5(void* context, MQTTAsync_successData5* response) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr && response != nullptr) {
    client->Connect5(*response);
  }
}

void MqttClient::OnConnectFailure5(void* context, MQTTAsync_failureData5* response) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr) {
    client->ConnectFailure5(response);
  }
}

void MqttClient::OnSubscribeFailure(void *context, MQTTAsync_failureData *response) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr && response != nullptr) {
    client->SubscribeFailure(*response);
  }
}

void MqttClient::OnSubscribeFailure5(void *context, MQTTAsync_failureData5 *response) {
  auto *client = reinterpret_cast<MqttClient *>(context);
  if (client != nullptr && response != nullptr) {
    client->SubscribeFailure5(*response);
  }
}

void MqttClient::OnDisconnect(void* context, MQTTAsync_successData* response) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  if (client != nullptr) {
    client->Disconnect(response);
  }
}

void MqttClient::OnDisconnectFailure(void* context, MQTTAsync_failureData* response) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  if (client != nullptr) {
    client->DisconnectFailure(response);
  }
}
void MqttClient::OnDisconnect5(void* context, MQTTAsync_successData5* response) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  if (client != nullptr) {
    client->Disconnect5(response);
  }
}

void MqttClient::OnDisconnectFailure5(void* context, MQTTAsync_failureData5* response) {
  auto *client = reinterpret_cast<pub_sub::MqttClient *>(context);
  if (client != nullptr) {
    client->DisconnectFailure5(response);
  }
}


bool MqttClient::IsOnline() const {
  return client_state_ == ClientState::Online;
}

bool MqttClient::IsOffline() const {
  return client_state_ == ClientState::Idle;
}

void MqttClient::ClientTask() {
  client_timer_ = 0;
  client_state_ = ClientState::Idle;
  if (handle_ != nullptr) {
    MQTTAsync_destroy(&handle_);
    handle_ = nullptr;
  }

  while (!stop_client_task_) {
    std::unique_lock client_lock(client_mutex_);
    client_event_.wait_for(client_lock, 100ms);

    switch (client_state_) {
      case ClientState::Idle: // Wait for in-service command
        DoIdle();
        break;

      case ClientState::WaitOnConnect: // Wait for in-service command
        DoWaitOnConnect();
        break;

      case ClientState::Online:
        DoOnline();
        break;

      case ClientState::WaitOnDisconnect:
        DoWaitOnDisconnect();
        break;

      default: // Invalid/Unknown state
        client_timer_ = SparkplugHelper::NowMs() + 10'000;
        client_state_ = ClientState::Idle;
        break;
    }
  }
  // Need to send disconnect or wait on the disconnect
  if (client_state_ != ClientState::Idle) {
    if (!IsConnected()) {
      if (listen_ && listen_->IsActive()) {
        listen_->ListenText("Stop ignored due to not connected to server");
      }
    } else {
      if (listen_ && listen_->IsActive()) {
        listen_->ListenText("Disconnecting");
      }
      if (client_state_ != ClientState::WaitOnDisconnect) {
        SendDisconnect();
      }
      // Wait for 5s for the disconnect to be delivered
      for (size_t timeout = 0;
           IsConnectionLost() && timeout < 50;
           ++timeout) {
        std::this_thread::sleep_for(100ms);
      }

      if (listen_ && listen_->IsActive()) {
        listen_->ListenText("Disconnected");
      }
    }
  }

  if (handle_ != nullptr) {
    MQTTAsync_destroy(&handle_);
    handle_ = nullptr;
  }

}

void MqttClient::StartSubscription() {
  for (const std::string& topic : subscription_list_ ) {

    MQTTAsync_responseOptions options = MQTTAsync_responseOptions_initializer;
    if (Version() == ProtocolVersion::Mqtt5) {
      options.onSuccess5 = nullptr; // No need of successful subscription
      options.onFailure5 = OnSubscribeFailure5;
    } else {
      options.onSuccess = nullptr;
      options.onFailure = OnSubscribeFailure;
    }
    options.context = this;


    if (listen_ && listen_->IsActive()) {
      listen_->ListenText("Subscribe: %s", topic.c_str());
    }
    const auto subscribe = MQTTAsync_subscribe(handle_, topic.c_str(),
                                               static_cast<int>(DefaultQualityOfService()), &options);
    if (subscribe != MQTTASYNC_SUCCESS) {
      LOG_ERROR() << "Subscription Failed. Topic: " << topic << ". Error: " << MQTTAsync_strerror(subscribe);
    }
  }

}

void MqttClient::DoIdle() {
  const auto now = SparkplugHelper::NowMs();
  const bool timeout = now >= client_timer_;

  // Destroy any previously created context/handle.
  if (handle_ != nullptr) {
    MQTTAsync_destroy(&handle_);
    handle_ = nullptr;
  }

  // Check the retry timeout first (10s)
  // Check if in service
  if (!InService() ) {
    client_timer_ = 0; // Fiz so it starts directly when on-line is requested
    return;
  }
  if (!timeout) { // Retry timer upon connect failure
    return;
  }

  // In-service create a communication context/handle and connect
  // to the MQTT server.
  const auto create = CreateClient();
  if (!create) {
    client_timer_ = now + 10'000; // 10 second to next create try
    return;
  }

  const auto connect = SendConnect();
  if (!connect) {
    client_timer_ = now + 10'000; // 10 second to next create try
    return;
  }

  // Switch state and wait for connection
  client_timer_ = now + 5'000; // Wait 5 second for connection
  client_state_ = ClientState::WaitOnConnect;
}

void MqttClient::DoWaitOnConnect() {
  const auto now = SparkplugHelper::NowMs();
  const bool timeout = now >= client_timer_;

  // Connection timeout
  if (timeout) {
    client_timer_ = now + 10'000; // Retry in 10 seconds
    client_state_ = ClientState::Idle;
    return;
  }

  // Check if connected and delivered.
  if (!IsConnected() || !IsDelivered()) {
    return;
  }

  // Start subscriptions and publish the topics for this client.
  // We will not check that it is delivered.
  StartSubscription();
  client_state_ = ClientState::Online;
}

void MqttClient::DoOnline() {
  const auto now = SparkplugHelper::NowMs();
  if (stop_client_task_ || !InService() ) {
    SendDisconnect();
    client_timer_ = now + 5'000;
    client_state_ = ClientState::WaitOnDisconnect;
  } else {
    PublishTopics();
  }
}

void MqttClient::DoWaitOnDisconnect() {
  const auto now = SparkplugHelper::NowMs();
  const bool timeout = now >= client_timer_;
  if (timeout || IsDelivered() ) {
    client_timer_ = now + 10'000; // Retry in 10s
    client_state_ = ClientState::Idle;
  }
}

void MqttClient::InitMqtt() const {
  static bool done_init = false;
  if (!done_init) {
    MQTTAsync_init_options init_options = MQTTAsync_init_options_initializer;
    if (Transport() == TransportLayer::MqttTcpTls || Transport() == TransportLayer::MqttWebSocketTls) {
      init_options.do_openssl_init = 1;
    }
    MQTTAsync_global_init(&init_options);
    done_init = true;
  }
}

void MqttClient::InitSsl() {
  ssl_options_ = MQTTAsync_SSLOptions_initializer;
  if (!trust_store_.empty()) {
    ssl_options_.trustStore = trust_store_.c_str();
  }
  if (!key_store_.empty()) {
    ssl_options_.keyStore = key_store_.c_str();
  }
  if (!private_key_.empty()) {
    ssl_options_.privateKey = private_key_.c_str();
  }
  if (!private_key_password_.empty()) {
    ssl_options_.privateKeyPassword = private_key_password_.c_str();
  }
  if (!enabled_cipher_suites_.empty()) {
    ssl_options_.enabledCipherSuites = enabled_cipher_suites_.c_str();
  }
  ssl_options_.enableServerCertAuth = enable_cert_auth_ ? 1 : 0;
  ssl_options_.sslVersion = ssl_version_;
  if (!ca_path_.empty()) {
    ssl_options_.CApath = ca_path_.c_str();
  }
  ssl_options_.ssl_error_cb = SslErrorCallback;
  ssl_options_.ssl_error_context = this;
  ssl_options_.disableDefaultTrustStore = disable_default_trust_store_ ? 1 : 0;
}

int MqttClient::SslErrorCallback(const char *error, size_t len, void *) {
  if (len == 0 || error == nullptr) {
    return 0;
  }
  std::string err(len,'\0');
  memcpy(err.data(), error, err.size());

  LOG_ERROR() << err;
  return 1;
}

} // end namespace