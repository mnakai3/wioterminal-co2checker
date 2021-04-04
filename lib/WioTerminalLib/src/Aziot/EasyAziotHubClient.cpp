#include "Aziot/EasyAziotConfig.h"
#include "Aziot/EasyAziotHubClient.h"
#include <azure/core/az_span.h>

static inline const az_span az_span_create_from_string(const std::string& str)
{
    return az_span_create(reinterpret_cast<uint8_t*>(const_cast<char*>(str.c_str())), str.size());
}

EasyAziotHubClient::EasyAziotHubClient()
{
}

int EasyAziotHubClient::Init(const char* host, const char* deviceId, const char* modelId)
{
    Host_ = host;
    DeviceId_ = deviceId;
    ModelId_ = modelId;

    {
        const az_span hostSpan = az_span_create_from_string(Host_);
        const az_span deviceIdSpan = az_span_create_from_string(DeviceId_);
        az_iot_hub_client_options options = az_iot_hub_client_options_default();
        options.model_id = az_span_create_from_string(ModelId_);
        if (az_result_failed(az_iot_hub_client_init(&HubClient_, hostSpan, deviceIdSpan, &options))) return -1;                                 // SDK_API
    }

    {
        char mqttUsername[MQTT_USERNAME_MAX_SIZE];
        if (az_result_failed(az_iot_hub_client_get_user_name(&HubClient_, mqttUsername, sizeof(mqttUsername), nullptr))) return -2;             // SDK_API
        MqttUsername_ = mqttUsername;
    }

    {
        char mqttClientId[MQTT_CLIENT_ID_MAX_SIZE];
        size_t client_id_length;
        if (az_result_failed(az_iot_hub_client_get_client_id(&HubClient_, mqttClientId, sizeof(mqttClientId), &client_id_length))) return -3;   // SDK_API
        MqttClientId_ = mqttClientId;
    }

    MqttPassword_.clear();

    return 0;
}

int EasyAziotHubClient::SetSAS(const char* symmetricKey, const uint64_t& expirationEpochTime, std::function<std::string(const std::string& symmetricKey, const std::vector<uint8_t>& signature)> generateEncryptedSignature)
{
    ////////////////////
    // SAS auth

    std::string encryptedSignature;
    {
        std::vector<uint8_t> signature;
        {
            uint8_t signatureBuf[SIGNATURE_MAX_SIZE];
            az_span signatureSpan = AZ_SPAN_FROM_BUFFER(signatureBuf);
            az_span signatureValidSpan;
            if (az_result_failed(az_iot_hub_client_sas_get_signature(&HubClient_, expirationEpochTime, signatureSpan, &signatureValidSpan))) return -4; // SDK_API
            signature.assign(az_span_ptr(signatureValidSpan), az_span_ptr(signatureValidSpan) + az_span_size(signatureValidSpan));
        }
        encryptedSignature = generateEncryptedSignature(symmetricKey, signature);
    }

    {
        char mqttPassword[MQTT_PASSWORD_MAX_SIZE];
        const az_span encryptedSignatureSpan = az_span_create_from_string(encryptedSignature);
        if (az_result_failed(az_iot_hub_client_sas_get_password(&HubClient_, expirationEpochTime, encryptedSignatureSpan, AZ_SPAN_EMPTY, mqttPassword, sizeof(mqttPassword), nullptr))) return -5;  // SDK_API
        MqttPassword_ = mqttPassword;
    }

    return 0;
}

const std::string& EasyAziotHubClient::GetMqttUsername() const
{
    return MqttUsername_;
}

const std::string& EasyAziotHubClient::GetMqttClientId() const
{
    return MqttClientId_;
}

const std::string& EasyAziotHubClient::GetMqttPassword() const
{
    return MqttPassword_;
}

std::string EasyAziotHubClient::GetTelemetryPublishTopic()
{
    char telemetryPublishTopic[TELEMETRY_PUBLISH_TOPIC_MAX_SIZE];
    if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(&HubClient_, nullptr, telemetryPublishTopic, sizeof(telemetryPublishTopic), nullptr))) return std::string(); // SDK_API

    return telemetryPublishTopic;
}
