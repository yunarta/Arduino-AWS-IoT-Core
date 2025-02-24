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

typedef bool (*ThingClientCallback)(String topic, JsonDocument payload);
typedef bool (*ThingClientShadowCallback)(String shadowName, JsonObject payload);

class ThingClient {
private:
  ThingClientCallback callback;
  ThingClientShadowCallback shadowCallback;
  JsonDocument shadows;

  PubSubClient* client;
  String thingName;

  bool isRunning;
  bool isClassicReceived;


public:
  ThingClient(PubSubClient* client, String thingName);

  void registerShadow(const String &shadowName);
  void updateShadow(const String &shadowName, JsonObject payload);
  JsonObject getShadow(const String &shadowName);

  void begin();
  void end();

  void setCallback(ThingClientCallback callback);
  void setShadowCallback(ThingClientShadowCallback callback);
  bool onMessage(String topic, JsonDocument payload);

  void loop();
};

typedef bool (*FleetProvisioningClientCallback)(String topic, JsonDocument payload);

class FleetProvisioningClient {
private:
  FleetProvisioningClientCallback callback;

  PubSubClient* client;
  String provisioningName;
  String thingName;

  bool isRunning;

  void saveCertificate(JsonDocument payload);
  void requestProvisioning(JsonDocument payload);
  bool acceptProvisioning(JsonDocument payload);

public:
  FleetProvisioningClient(PubSubClient* client, String provisioningName, String thingName);

  void begin();
  void end();

  void setCallback(FleetProvisioningClientCallback callback);
  bool onMessage(String topic, JsonDocument payload);
};


#endif //AWSIOTCORE_H
