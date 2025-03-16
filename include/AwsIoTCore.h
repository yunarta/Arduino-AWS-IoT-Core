//
// Created by yunarta on 2/20/25.
//

#ifndef AWSIOTCORE_H
#define AWSIOTCORE_H

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Array.h>

class FleetProvisioningClient;

class ThingClient;

// typedef bool (*ThingClientCallback)(const String &topic, JsonDocument payload);
#define ThingClientCallback std::function<bool(const String &shadowName, JsonDocument &payload)>

// typedef bool (*ThingClientShadowCallback)(const String &shadowName, const JsonObject &payload);
#define ThingClientShadowCallback std::function<bool(const String &shadowName, JsonObject &payload)>


class ThingClient {
private:
    ThingClientCallback callback;
    ThingClientShadowCallback shadowCallback;
    JsonDocument shadows;

    PubSubClient *client;
    String thingName;

    bool isRunning;
    bool isClassicReceived;

public:
    ThingClient(PubSubClient *client, const String &thingName);

    void registerShadow(const String &shadowName);

    void updateShadow(const String &shadowName, JsonObject &payload);

    JsonObject getShadow(const String &shadowName);

    void begin();

    void end();

    void setCallback(ThingClientCallback callback);

    void setShadowCallback(ThingClientShadowCallback callback);

    bool onMessage(const String &topic, JsonDocument &payload);

    void loop();
};

// typedef bool (*FleetProvisioningClientCallback)(const String &topic, const JsonDocument &payload);
#define FleetProvisioningClientCallback std::function<bool(const String &topic,  JsonDocument &payload)>

class FleetProvisioningClient {
    PubSubClient *client;
    String provisioningName;
    String thingName;

    bool isRunning;

    void saveCertificate(JsonDocument &payload);

    void requestProvisioning(JsonDocument &payload);

    bool acceptProvisioning(JsonDocument &payload);

    FleetProvisioningClientCallback callback;

public:
    FleetProvisioningClient(PubSubClient *client, const String &provisioningName, const String &thingName);

    void begin();

    void end();

    void setCallback(FleetProvisioningClientCallback callback);

    bool onMessage(const String &topic, JsonDocument &payload);
};


#endif //AWSIOTCORE_H
