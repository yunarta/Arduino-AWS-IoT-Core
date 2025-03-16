//
// Created by yunarta on 2/20/25.
//

// ReSharper disable CppMemberFunctionMayBeStatic
// ReSharper disable CppMemberFunctionMayBeConst
#include "AwsIoTCore.h"

#include <LittleFS.h>
#include <FS.h>

#define KEYSTORE_FILE "/tmp/aws-keystore.json"

FleetProvisioningClient::FleetProvisioningClient(PubSubClient *client, const String &provisioningName,
                                                 const String &thingName) {
    this->client = client;
    this->provisioningName = provisioningName;
    this->thingName = thingName;
    this->isRunning = false;
    this->callback = nullptr;

#ifdef LOG_INFO
    Serial.println(F("[INFO] FleetProvisioningClient initialized"));
#endif
}

void FleetProvisioningClient::begin() {
    this->isRunning = true;
    bool mkdir = LittleFS.mkdir("/tmp");
#ifdef LOG_DEBUG
    Serial.println(F("[DEBUG] FleetProvisioningClient starting"));
#endif
    if (!mkdir) {
#ifdef LOG_DEBUG
        Serial.println(F("[DEBUG] Failed to create temporary directory"));
#endif
        return;
    }
    this->client->publish("$aws/certificates/create/json", "{}");
#ifdef LOG_INFO
    Serial.println(F("[INFO] Certificate creation request sent"));
#endif
}

void FleetProvisioningClient::end() {
    this->isRunning = false;
#ifdef LOG_INFO
    Serial.println(F("[INFO] FleetProvisioningClient stopped"));
#endif
}

void FleetProvisioningClient::setCallback(FleetProvisioningClientCallback callback) {
    this->callback = callback;
#ifdef LOG_INFO
    Serial.println(F("[INFO] Callback set for FleetProvisioningClient"));
#endif
}

void FleetProvisioningClient::saveCertificate(JsonDocument &payload) {
    JsonDocument doc;
    String buffer;

    doc["certificate"] = payload["certificatePem"].as<const char *>();
    doc["privateKey"] = payload["privateKey"].as<const char *>();

    LittleFS.begin();
#ifdef LOG_DEBUG
    Serial.println(F("[DEBUG] Saving certificate to keystore"));
#endif
    File keystore = LittleFS.open(KEYSTORE_FILE, FILE_WRITE, true);
    if (!keystore) {
#ifdef LOG_DEBUG
        Serial.println(F("[DEBUG] Failed to open keystore file for writing"));
#endif
        return;
    }

    serializeJson(doc, keystore);
    keystore.close();
#ifdef LOG_INFO
    Serial.println(F("[INFO] Certificate and private key saved"));
#endif
}

void FleetProvisioningClient::requestProvisioning(JsonDocument &payload) {
    JsonDocument doc;
    char provisioningTopic[256];
    String buffer;

    doc["certificateOwnershipToken"] = payload["certificateOwnershipToken"].as<const char *>();
    doc["parameters"]["ThingName"] = this->thingName;
    doc["parameters"]["AWS::IoT::Certificate::Id"] = payload["certificateId"].as<const char *>();

    snprintf(provisioningTopic, sizeof(provisioningTopic),
             "$aws/provisioning-templates/%s/provision/json", this->provisioningName.c_str());

    serializeJson(doc, buffer);
    this->client->publish(provisioningTopic, buffer.c_str());
#ifdef LOG_INFO
    Serial.println(F("[INFO] Provisioning request sent"));
#endif
}

bool FleetProvisioningClient::acceptProvisioning(JsonDocument &payload) {
    String content;
    JsonDocument reply;

    File keystore = LittleFS.open(KEYSTORE_FILE, "r");
#ifdef LOG_DEBUG
    Serial.println(F("[DEBUG] Reading keystore file"));
#endif
    if (!keystore) {
#ifdef LOG_DEBUG
        Serial.println(F("[DEBUG] Keystore file not found"));
#endif
        return false;
    }

    DeserializationError error = deserializeJson(reply, keystore);
    keystore.close();

    if (error) {
#ifdef LOG_DEBUG
        Serial.println(F("[DEBUG] Failed to deserialize keystore file"));
#endif
        LittleFS.remove(KEYSTORE_FILE);
        return false;
    }

    if (this->callback != nullptr && this->callback(F("provisioning/success"), reply)) {
        LittleFS.remove(KEYSTORE_FILE);
#ifdef LOG_INFO
        Serial.println(F("[INFO] Provisioning accepted"));
#endif
        return true;
    }

    return false;
}

bool FleetProvisioningClient::onMessage(const String &topic, JsonDocument &payload) {
    if (!this->isRunning) {
#ifdef LOG_DEBUG
        Serial.println(F("[DEBUG] Received message while client is not running"));
#endif
        return false;
    }

    char provisioningTopicAccepted[256], provisioningTopicRejected[256];
    snprintf(provisioningTopicAccepted, sizeof(provisioningTopicAccepted),
             "$aws/provisioning-templates/%s/provision/json/accepted", this->provisioningName.c_str());
    snprintf(provisioningTopicRejected, sizeof(provisioningTopicRejected),
             "$aws/provisioning-templates/%s/provision/json/rejected", this->provisioningName.c_str());

    if (topic.equals("$aws/certificates/create/json/accepted")) {
#ifdef LOG_INFO
        Serial.println(F("[INFO] Certificate creation accepted"));
#endif
        saveCertificate(payload);
        requestProvisioning(payload);
        return true;
    }

    if (topic.equals(provisioningTopicAccepted)) {
#ifdef LOG_INFO
        Serial.println(F("[INFO] Provisioning accepted message received"));
#endif
        return acceptProvisioning(payload);
    }

    if (topic.equals(provisioningTopicRejected)) {
#ifdef LOG_DEBUG
        Serial.println(F("[DEBUG] Provisioning rejected"));
#endif
        LittleFS.remove(KEYSTORE_FILE);
        return true;
    }

    return false;
}
