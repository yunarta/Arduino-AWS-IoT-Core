//
// Created by yunarta on 2/20/25.
//

#include "AwsIoTCore.h"

#include <LittleFS.h>

ThingClient::ThingClient(PubSubClient *client, String thingName) {
    this->client = client;
    this->thingName = thingName;
    this->isRunning = false;
    this->isClassicReceived = false;
    Serial.println("ThingClient initialized");
}

void ThingClient::begin() {
    this->isRunning = true;

    char classicShadow[1024];
    snprintf(classicShadow, sizeof(classicShadow), "$aws/things/%s/shadow", this->thingName.c_str());
    String classicShadowTopic = classicShadow;


    this->shadows["classic"]["state"] = millis();

    this->client->subscribe((classicShadowTopic + "/get/accepted").c_str(), 1.0);
    this->client->subscribe((classicShadowTopic + "/get/rejected").c_str(), 1.0);
    // this->client->subscribe((classicShadowTopic + "/update/delta").c_str(), 1.0);
    this->client->subscribe((classicShadowTopic + "/update/accepted").c_str(), 1.0);
    this->client->subscribe((classicShadowTopic + "/update/rejected").c_str(), 1.0);
    this->client->publish((classicShadowTopic + "/get").c_str(), "{}");

    Serial.println("ThingClient begin complete");
}

void ThingClient::end() {
    this->isRunning = false;
    Serial.println("ThingClient stopped");
}

void ThingClient::setCallback(ThingClientCallback callback) {
    this->callback = callback;
    Serial.println("Callback function set");
}

void ThingClient::registerShadow(String shadowName) {
    char shadowChars[1024];
    String shadowTopic;

    snprintf(shadowChars, sizeof(shadowChars), "$aws/things/%s/shadow/name/%s",
             this->thingName.c_str(),
             shadowName.c_str());
    shadowTopic = shadowChars;

    this->shadows[shadowName]["state"] = millis();
    this->client->subscribe((shadowTopic + "/get/accepted").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/get/rejected").c_str(), 1.0);
    // this->client->subscribe((shadowTopic + "/update/delta").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/accepted").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/rejected").c_str(), 1.0);
    this->client->publish((shadowTopic + "/get").c_str(), "{}");

    Serial.printf("Shadow registered: %s\n", shadowName.c_str());
}

void ThingClient::replyShadow(String shadowName, String replyTopic, JsonDocument payload) {
    JsonDocument reply_payload;
    String jsonString;

    JsonObject desired = payload["state"]["desired"];
    if (!desired.isNull()) {
        reply_payload["state"]["reported"] = desired;
        serializeJson(reply_payload, jsonString);

        this->client->publish(replyTopic.c_str(), jsonString.c_str());
    }
}

bool ThingClient::onMessage(String topic, JsonDocument payload) {
    char shadowChars[1024];
    String shadowTopic;
    String shadowName;
    String publishTopic;
    JsonDocument reply_payload;
    String jsonString;
    size_t nameOffset;

    snprintf(shadowChars, sizeof(shadowChars), "$aws/things/%s/shadow/name/", this->thingName.c_str());
    nameOffset = strlen(shadowChars);


    if (topic.endsWith("shadow/get/accepted")) {
        // classic shadow contains additional shadow name that we wanted to subscribe to
        if (!payload["state"]["desired"]["shadows"].isNull()) {
            for (JsonVariant shadow: payload["state"]["desired"]["shadows"].as<JsonArray>()) {
                shadowName = shadow.as<String>();
                snprintf(shadowChars, sizeof(shadowChars), "$aws/things/%s/shadow/name/%s",
                         this->thingName.c_str(),
                         shadowName.c_str());
                shadowTopic = shadowChars;

                this->shadows[shadowName]["state"] = millis();
                this->client->subscribe((shadowTopic + "/get/accepted").c_str(), 1.0);
                this->client->subscribe((shadowTopic + "/get/rejected").c_str(), 1.0);
                // this->client->subscribe((shadowTopic + "/update/delta").c_str(), 1.0);
                this->client->subscribe((shadowTopic + "/update/accepted").c_str(), 1.0);
                this->client->subscribe((shadowTopic + "/update/rejected").c_str(), 1.0);
                this->client->publish((shadowTopic + "/get").c_str(), "{}");
            }
        }

        this->isClassicReceived = true;
        payload["state"]["desired"].remove("welcome");
        payload["state"]["desired"]["version"] = 1;

        this->shadows["classic"]["state"] = payload["state"]["desired"];

        publishTopic = topic.substring(0, topic.length() - 13) + "/update";
        replyShadow("classic", publishTopic, payload);

        return true;
    }

    if (topic.endsWith("shadow/update/accepted")) {
        JsonObject desired = payload["state"]["desired"];
        if (!desired.isNull()) {
            shadowName = topic.substring(nameOffset, topic.length() - 16);
            payload["state"]["desired"].remove("welcome");
            payload["state"]["desired"]["version"] = 1;

            this->shadows["classic"]["state"] = payload["state"]["desired"];

            publishTopic = topic.substring(0, topic.length() - 16) + "/update";
            replyShadow(shadowName, publishTopic, payload);
        }
        return true;
    }

    if (topic.endsWith("/get/accepted")) {
        JsonObject desired = payload["state"]["desired"];
        if (!desired.isNull()) {
            shadowName = topic.substring(nameOffset, topic.length() - 13);
            payload["state"]["desired"].remove("welcome");
            payload["state"]["desired"]["version"] = 1;

            this->shadows[shadowName]["state"] = payload["state"]["desired"];

            publishTopic = topic.substring(0, topic.length() - 13) + "/update";
            replyShadow(shadowName, publishTopic, payload);

            return true;
        }
    }

    if (topic.endsWith("/update/accepted")) {
        JsonObject desired = payload["state"]["desired"];
        if (!desired.isNull()) {
            shadowName = topic.substring(nameOffset, topic.length() - 16);
            payload["state"]["desired"].remove("welcome");
            payload["state"]["desired"]["version"] = 1;

            this->shadows[shadowName]["state"] = payload["state"]["desired"];

            publishTopic = topic.substring(0, topic.length() - 16) + "/update";
            replyShadow(shadowName, publishTopic, payload);
        }
        return true;
    }

    return true;
}

void ThingClient::loop() {
    if (this->isRunning) {
        if (!this->isClassicReceived) {
            unsigned long classic = this->shadows["classic"]["state"].as<long>();
            unsigned long now = millis();
            if (classic != 0 && now - classic > 10 * 1000) {
                this->shadows["classic"]["state"] = millis();

                char classicShadow[1024];
                snprintf(classicShadow, sizeof(classicShadow), "$aws/things/%s/shadow", this->thingName.c_str());
                String classicShadowTopic = classicShadow;

                this->client->publish((classicShadowTopic + "/get").c_str(), "{}");
            }
        }

    }
}
