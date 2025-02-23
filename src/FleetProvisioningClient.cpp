//
// Created by yunarta on 2/20/25.
//

#include "AwsIoTCore.h"

#include <LittleFS.h>

FleetProvisioningClient::FleetProvisioningClient(PubSubClient *client, String provisioningName, String thingName) {
    this->client = client;
    this->provisioningName = provisioningName;
    this->thingName = thingName;
    this->isRunning = false;
}

void FleetProvisioningClient::begin() {
    this->isRunning = true;
    Serial.println("Starting provisioning process...");
    this->client->publish("$aws/certificates/create/json", "{}");
}

void FleetProvisioningClient::end() {
    this->isRunning = false;
    Serial.println("Ending provisioning process...");
}

void FleetProvisioningClient::setCallback(FleetProvisioningClientCallback callback) {
    Serial.println("Setting callback...");
    this->callback = callback;
}

void FleetProvisioningClient::saveCertificate(JsonDocument payload) {
    Serial.println("Saving certificate...");
    JsonDocument doc;

    File keystore;
    String buffer;
    const char *certificate, *privateKey;

    Serial.println("Saving certificate... 1");

    certificate = payload["certificatePem"];
    doc["certificate"] = payload["certificatePem"].as<const char*>();
    Serial.println("Saving certificate... 2");


    privateKey = payload["privateKey"];
    doc["privateKey"] = payload["privateKey"].as<const char*>();
    Serial.println("Saving certificate... 3");

    keystore = LittleFS.open("/tmp/aws-iot-provisioning-keystore.json", "w", true);
    if (!keystore) {
        Serial.println("Failed to open keystore file for writing.");
        return;
    }

    serializeJson(doc, keystore);
    Serial.println("Saving certificate... 5");

    keystore.close();
    Serial.println("Certificate saved successfully.");
}

void FleetProvisioningClient::requestProvisioning(JsonDocument payload) {
    Serial.println("Requesting provisioning...");
    JsonDocument doc;

    char provisioningTopic[256];
    String buffer;

    size_t size;

    doc["certificateOwnershipToken"] = payload["certificateOwnershipToken"].as<const char*>();
    doc["parameters"]["ThingName"] = this->thingName;
    doc["parameters"]["AWS::IoT::Certificate::Id"] = payload["certificateId"].as<const char*>();

    snprintf(provisioningTopic, sizeof(provisioningTopic),
             "$aws/provisioning-templates/%s/provision/json", this->provisioningName.c_str());

    serializeJson(doc, buffer);
    this->client->publish(provisioningTopic, buffer.c_str());
    Serial.println("Provisioning request sent.");
}

bool FleetProvisioningClient::acceptProvisioning(JsonDocument payload) {
    Serial.println("Provisioning accepted.");
    File keystore;
    String content;
    JsonDocument reply;
    DeserializationError error;

    keystore = LittleFS.open("/tmp/aws-iot-provisioning-keystore.json", "r");
    if (!keystore) {
        Serial.println("Failed to open keystore file for reading.");
        return false;
    }

    error = deserializeJson(reply, keystore);
    keystore.close();

    if (error) {
        Serial.println("Failed to deserialize keystore content.");
        LittleFS.remove("/tmp/aws-iot-provisioning-keystore.json");
        return false;
    }

    Serial.println("Provisioning success callback triggered.");
    if (this->callback(F("provisioning/success"), reply)) {
        LittleFS.remove("/tmp/aws-iot-provisioning-keystore.json");
        return true;
    }

    return false;
}

bool FleetProvisioningClient::onMessage(String topic, JsonDocument payload) {
    if (!this->isRunning) {
        return false;
    }

    Serial.print("Received message on topic: ");
    Serial.println(topic);
    char provisioningTopicAccepted[256], provisioningTopicRejected[256];
    snprintf(provisioningTopicAccepted, sizeof(provisioningTopicAccepted),
             "$aws/provisioning-templates/%s/provision/json/accepted", this->provisioningName.c_str());
    snprintf(provisioningTopicRejected, sizeof(provisioningTopicRejected),
             "$aws/provisioning-templates/%s/provision/json/rejected", this->provisioningName.c_str());

    if (topic.equals("$aws/certificates/create/json/accepted")) {
        Serial.println("Certificate creation accepted.");
        saveCertificate(payload);
        requestProvisioning(payload);
        return true;
    }

    if (topic.equals(provisioningTopicAccepted)) {
        return acceptProvisioning(payload);
    }

    if (topic.equals(provisioningTopicRejected)) {
        Serial.println("Provisioning rejected.");
        LittleFS.remove("/tmp/aws-iot-provisioning-keystore.json");
        return true;
    }

    Serial.println("Unhandled topic.");
    return false;
}
