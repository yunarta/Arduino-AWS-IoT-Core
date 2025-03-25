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
#define ThingClientCommandCallback std::function<bool(const String &executionId, JsonDocument &payload)>

// typedef bool (*ThingClientShadowCallback)(const String &shadowName, const JsonObject &payload);
#define ThingClientJobsCallback std::function<bool(const String &jobId, JsonDocument &payload)>

// typedef bool (*ThingClientShadowCallback)(const String &shadowName, const JsonObject &payload);
#define ThingClientShadowCallback std::function<bool(const String &shadowName, JsonObject &payload, bool shouldMutate)>

struct CommandReply {
    String status;
    String statusCode;
    String statusReason;
    JsonDocument result;
};

struct JobReply {
    String status;
    long expectedVersion;
    JsonDocument statusDetails;
};

class ThingClient {
private:
    ThingClientCallback callback;
    ThingClientCommandCallback commandCallback;
    ThingClientJobsCallback jobsCallback;
    ThingClientShadowCallback shadowCallback;
    JsonDocument shadows;

    PubSubClient *client;
    String thingName;

    bool isRunning;
    bool isClassicReceived;
    bool listPendingJobsRequested;

    bool processCommandMessage(const String &topic, JsonDocument &payload);

    bool processJobMessage(const String &topic, JsonDocument &payload);

    bool processShadowMessage(const String &topic, JsonDocument &payload);

public:
    ThingClient(PubSubClient *client, const String &thingName);

    void registerShadow(const String &shadowName);

    void preloadShadow(const String &shadowName, JsonObject &payload);

    bool isValidated(const String &shadowName);

    void preloadedShadowValidated(const String &shadowName);

    void requestShadow(const String &shadowName);

    void updateShadow(const String &shadowName, JsonObject &payload);

    JsonObject getShadow(const String &shadowName);

    void listPendingJobs();

    void startPendingJobs(unsigned int startPendingJobs = 0);

    void commandReply(const String &executionId, const CommandReply &payload);

    void jobReply(const String &jobId, const JobReply &payload);

    void requestJobDetail(const String &jobId);

    void begin();

    void end();

    void setCallback(ThingClientCallback callback);

    void setCommandCallback(ThingClientCommandCallback callback);

    void setJobsCallback(ThingClientJobsCallback callback);

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
