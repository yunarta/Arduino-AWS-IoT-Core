//
// Created by yunarta on 2/20/25.
//

// ReSharper disable CppMemberFunctionMayBeStatic
// ReSharper disable CppMemberFunctionMayBeConst
#include "AwsIoTCore.h"
#include "aws_utils.h"

#include <LittleFS.h>

ThingClient::ThingClient(PubSubClient *client, const String &thingName) {
    this->client = client;
    this->thingName = thingName;
    this->isRunning = false;
    this->isClassicReceived = false;
    this->callback = nullptr;
    this->shadowCallback = nullptr;

#ifdef LOG_INFO
    Serial.printf("[INFO] ThingClient initialized for thing: %s\n", thingName.c_str());
#endif
}

void ThingClient::begin() {
    this->isRunning = true;

#ifdef LOG_INFO
    Serial.println("[INFO] ThingClient started.");
#endif
}

void ThingClient::end() {
    this->isRunning = false;

#ifdef LOG_INFO
    Serial.println("[INFO] ThingClient stopped.");
#endif
}

void ThingClient::setCallback(ThingClientCallback callback) {
    this->callback = callback;

#ifdef LOG_DEBUG
    Serial.println("[DEBUG] Main callback set.");
#endif
}

void ThingClient::setShadowCallback(ThingClientShadowCallback shadowCallback) {
    this->shadowCallback = shadowCallback;

#ifdef LOG_DEBUG
    Serial.println("[DEBUG] Shadow callback set.");
#endif
}

void ThingClient::registerShadow(const String &shadowName) {
    String shadowTopic = StringPrintF("$aws/things/%s/shadow/name/%s",
                                      this->thingName.c_str(),
                                      shadowName.c_str());

    this->shadows[shadowName]["timestamp"] = millis();;

    this->client->subscribe((shadowTopic + "/get/accepted").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/get/rejected").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/delta").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/accepted").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/rejected").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/documents").c_str(), 1.0);

#ifdef LOG_INFO
    Serial.printf("[INFO] Shadow '%s' registered.\n", shadowName.c_str());
#endif
}

void ThingClient::updateShadow(const String &shadowName, JsonObject &payload) {
    String updateTopic = StringPrintF("$aws/things/%s/shadow/name/%s/update",
                                      this->thingName.c_str(),
                                      shadowName.c_str());

    JsonDocument reply_payload;
    String jsonString;

    reply_payload["state"]["reported"] = payload;
    serializeJson(reply_payload, jsonString);

    this->client->publish(updateTopic.c_str(), jsonString.c_str());
    this->shadows[shadowName]["state"] = payload;

#ifdef LOG_INFO
    Serial.printf("[INFO] Shadow '%s' updated with reported state.\n", shadowName.c_str());
#endif
#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Published payload: %s\n", jsonString.c_str());
#endif
}

JsonObject ThingClient::getShadow(const String &shadowName) {
    auto state = this->shadows[shadowName]["state"].as<JsonObject>();
#ifdef LOG_DEBUG
    if (state.isNull()) {
        Serial.printf("[DEBUG] Shadow '%s' state is null.\n", shadowName.c_str());
    } else {
        Serial.printf("[DEBUG] Shadow '%s' state retrieved.\n", shadowName.c_str());
    }
#endif
    return state.isNull() ? JsonObject() : state;
}

bool ThingClient::onMessage(const String &topic, JsonDocument &payload) {
    if (!this->isRunning) {
#ifdef LOG_DEBUG
        Serial.println("[DEBUG] Received message but ThingClient is not running.");
#endif
        return false;
    }

    char shadowChars[1024];
    String shadowTopic;
    String shadowName;
    JsonDocument reply_payload;
    String jsonString;

    snprintf(shadowChars, sizeof(shadowChars), "$aws/things/%s/shadow/name/", this->thingName.c_str());
    size_t nameOffset = strlen(shadowChars);

    if (topic.endsWith("/get/accepted")) {
        JsonObject desired = payload["state"]["desired"];
        if (!desired.isNull()) {
            shadowName = topic.substring(nameOffset, topic.length() - 13);

            String publishTopic = topic.substring(0, topic.length() - 13) + "/update";
            if (this->shadowCallback != nullptr) {
                this->shadowCallback(shadowName, desired);
            }
#ifdef LOG_INFO
            Serial.printf("[INFO] Shadow '%s' GET accepted received.\n", shadowName.c_str());
#endif
            return true;
        }
    }

    if (topic.endsWith("/update/documents")) {
        JsonObject desired = payload["current"]["state"]["desired"];
        if (!desired.isNull()) {
            shadowName = topic.substring(nameOffset, topic.length() - 17);

            if (this->shadowCallback != nullptr) {
                this->shadowCallback(shadowName, desired);
            }
#ifdef LOG_INFO
            Serial.printf("[INFO] Shadow '%s' UPDATE documents received.\n", shadowName.c_str());
#endif
            return true;
        }
        return true;
    }

#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Received topic: %s but no handler matched.\n", topic.c_str());
#endif
    return false;
}

void ThingClient::loop() {
    if (this->isRunning) {
        unsigned long now = millis();

        for (JsonPair kv: this->shadows.as<JsonObject>()) {
            auto key = kv.key();
            auto shadow = this->shadows[key].as<JsonObject>();
            if (shadow["state"].isNull()) {
                auto time = shadow["timestamp"].as<unsigned long>();
                if (now - time > 10 * 1000) {
                    shadow["timestamp"] = millis();
                    String shadowTopic = StringPrintF("$aws/things/%s/shadow/name/%s/get",
                                                      this->thingName.c_str(),
                                                      key.c_str());
                    this->client->publish(shadowTopic.c_str(), "{}");

#ifdef LOG_DEBUG
                    Serial.printf("[DEBUG] Requested state for shadow '%s'.\n", key.c_str());
#endif
                }
            }
        }
    }
}
