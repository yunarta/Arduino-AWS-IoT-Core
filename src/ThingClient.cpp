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
    this->callback = nullptr;
    this->shadowCallback = nullptr;

#ifdef LOG_INFO
    Serial.printf("[INFO] ThingClient initialized for thing: %s\n", thingName.c_str());
#endif
}

void ThingClient::begin() {
    this->isRunning = true;
    this->isClassicReceived = false;
    this->listPendingJobsRequested = false;

    char commandTopic[1024];
    snprintf(commandTopic, sizeof(commandTopic), "$aws/commands/things/%s/executions/+/request/json",
             this->thingName.c_str());
    this->client->subscribe(commandTopic, 1.0);

    auto jobsTopic = StringPrintF("$aws/things/%s", this->thingName.c_str());

    this->client->subscribe((jobsTopic + "/jobs/notify").c_str(), 1.0);
    this->client->subscribe((jobsTopic + "/jobs/+/get/accepted").c_str(), 1.0);

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

void ThingClient::setCommandCallback(ThingClientCommandCallback callback) {
    this->commandCallback = callback;

#ifdef LOG_DEBUG
    Serial.println("[DEBUG] Shadow callback set.");
#endif
}

void ThingClient::setJobsCallback(ThingClientJobsCallback callback) {
    this->jobsCallback = callback;

#ifdef LOG_DEBUG
    Serial.println("[DEBUG] Shadow callback set.");
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

    this->shadows[shadowName]["timestamp"] = 0L;

    this->client->subscribe((shadowTopic + "/get/accepted").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/get/rejected").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/delta").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/accepted").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/rejected").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/delta").c_str(), 1.0);
    this->client->subscribe((shadowTopic + "/update/documents").c_str(), 1.0);

#ifdef LOG_INFO
    Serial.printf("[INFO] Shadow '%s' registered.\n", shadowName.c_str());
#endif
}

void ThingClient::preloadShadow(const String &shadowName, JsonObject &payload) {
    this->shadows[shadowName]["state"] = payload;
}

void ThingClient::requestShadow(const String &shadowName) {
    if (this->client->connected()) {
        String shadowTopic = StringPrintF("$aws/things/%s/shadow/name/%s/get",
                                                          this->thingName.c_str(),
                                                          shadowName.c_str());
        this->client->publish(shadowTopic.c_str(), "{}");
    }
}

bool ThingClient::isValidated(const String &shadowName) {
    return this->shadows[shadowName]["loaded"].as<bool>();
}

void ThingClient::preloadedShadowValidated(const String &shadowName) {
    this->shadows[shadowName]["loaded"] = true;
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
    this->shadows[shadowName]["loaded"] = true;

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

void ThingClient::listPendingJobs() {
    if (!listPendingJobsRequested) {
        String topic = StringPrintF("$aws/things/%s/jobs/get",
                                    this->thingName.c_str()
        );

        JsonDocument payload;
        String jsonString;

        payload["clientToken"] = thingName;
        serializeJson(payload, jsonString);

        listPendingJobsRequested = true;
        this->client->publish(topic.c_str(), jsonString.c_str());
    }
}

void ThingClient::startPendingJobs(unsigned int timeout) {
    String topic = StringPrintF("$aws/things/%s/jobs/start-next",
                                this->thingName.c_str()
    );

    JsonDocument doc;
    String jsonString;

    doc["clientToken"] = thingName;
    if (timeout > 0) {
        doc["stepTimeoutInMinutes"] = timeout;
    }
    serializeJson(doc, jsonString);

    this->client->publish(topic.c_str(), jsonString.c_str());
}

void ThingClient::commandReply(const String &executionId, const CommandReply &payload) {
    String topic = StringPrintF("$aws/commands/things/%s/executions/%s/response/json",
                                this->thingName.c_str(),
                                executionId.c_str()
    );

    JsonDocument doc;
    String jsonString;

    doc["status"] = payload.status;
    doc["statusReason"]["reasonCode"] = payload.statusCode;
    doc["statusReason"]["reasonDescription"] = payload.statusReason;
    doc["result"] = payload.result;
    serializeJson(doc, jsonString);

    this->client->publish(topic.c_str(), jsonString.c_str());
}

void ThingClient::jobReply(const String &jobId, const JobReply &payload) {
    String topic = StringPrintF("$aws/things/%s/jobs/%s/update",
                                this->thingName.c_str(),
                                jobId.c_str()
    );

    JsonDocument doc;
    String jsonString;

    doc["status"] = payload.status;
    doc["statusDetails"] = payload.statusDetails;
    doc["expectedVersion"] = payload.expectedVersion;

    serializeJson(doc, jsonString);

    this->client->publish(topic.c_str(), jsonString.c_str());
}

void ThingClient::requestJobDetail(const String &jobId) {
    String topic = StringPrintF("$aws/things/%s/jobs/%s/get",
                                this->thingName.c_str(),
                                jobId.c_str()
    );

    JsonDocument doc;
    String jsonString;

    doc["thingName"] = thingName;
    doc["includeJobDocument"] = true;
    doc["clientToken"] = thingName;
    doc["jobId"] = jobId;
    serializeJson(doc, jsonString);

    this->client->publish(topic.c_str(), jsonString.c_str());
}

bool ThingClient::processCommandMessage(const String &topic, JsonDocument &payload) {
    char commandPrefix[1024];
    String executionId;
    JsonDocument reply_payload;
    String jsonString;

    snprintf(commandPrefix, sizeof(commandPrefix), "$aws/commands/things/%s/executions/", this->thingName.c_str());
    size_t nameOffset = strlen(commandPrefix);

    if (topic.startsWith(commandPrefix)) {
        if (topic.endsWith("/request/json")) {
            executionId = topic.substring(nameOffset, topic.length() - 13);

            this->commandCallback(executionId, payload);
            return true;
        }
    }

    return false;
}

bool ThingClient::processJobMessage(const String &topic, JsonDocument &payload) {
    char commandPrefix[1024];
    String shadowTopic;
    String shadowName;
    JsonDocument reply_payload;
    String jsonString;
    String jobId;

    snprintf(commandPrefix, sizeof(commandPrefix), "$aws/things/%s/jobs/", this->thingName.c_str());
    size_t nameOffset = strlen(commandPrefix);

    if (topic.startsWith(commandPrefix)) {
        if (topic.endsWith("/jobs/get/accepted")) {
            // handle /jobs/get/accepted (list all jobs)
            listPendingJobsRequested = false;
            if (jobsCallback != nullptr) {
                jobsCallback("", payload);
            }
            return true;
        } else if (topic.endsWith("/jobs/start-next/accepted")) {
            // handle /jobs/get/accepted (list all jobs and start them)
            return true;
        } else if (topic.endsWith("/jobs/notify")) {
            // notification for newly job added
            listPendingJobs();
            return true;
        } else if (topic.endsWith("/get/accepted")) {
            // handle /get/accepted (job detail)
            jobId = topic.substring(nameOffset, topic.length() - 13);
            if (jobsCallback != nullptr) {
                jobsCallback(jobId, payload);
            }
            return true;
        } else if (topic.endsWith("/update/accepted")) {
            // handle /get/accepted (job update)
            jobId = topic.substring(nameOffset, topic.length() - 13);

            return true;
        }
    }

    return false;
}

bool ThingClient::processShadowMessage(const String &topic, JsonDocument &payload) {
    char shadowChars[1024];
    String shadowTopic;
    String shadowName;
    JsonDocument reply_payload;
    String jsonString;

    snprintf(shadowChars, sizeof(shadowChars), "$aws/things/%s/shadow/name/", this->thingName.c_str());
    size_t nameOffset = strlen(shadowChars);

    if (topic.startsWith(shadowChars)) {
        if (topic.endsWith("/get/accepted")) {
            JsonObject desired = payload["state"]["desired"];
            if (!desired.isNull()) {
                shadowName = topic.substring(nameOffset, topic.length() - 13);

                String publishTopic = topic.substring(0, topic.length() - 13) + "/update";
                if (this->shadowCallback != nullptr) {
                    this->shadowCallback(shadowName, desired, true);
                }
#ifdef LOG_INFO
                Serial.printf("[INFO] Shadow '%s' GET accepted received.\n", shadowName.c_str());
#endif
                return true;
            }
        }

        if (topic.endsWith("/update/delta")) {
            this->shadows[shadowName]["delta"] = 1;
        }

        if (topic.endsWith("/update/documents")) {
            JsonObject desired = payload["current"]["state"]["desired"];
            if (!desired.isNull()) {
                shadowName = topic.substring(nameOffset, topic.length() - 17);

                if (this->shadowCallback != nullptr) {
                    bool shouldMutate = this->shadows[shadowName]["delta"].as<int>() > 0;
                    this->shadows[shadowName]["delta"] = 0;
                    this->shadowCallback(shadowName, desired, shouldMutate);
                }
#ifdef LOG_INFO
                Serial.printf("[INFO] Shadow '%s' UPDATE documents received.\n", shadowName.c_str());
#endif
                return true;
            }

            return true;
        }
    }

    return false;
}

bool ThingClient::onMessage(const String &topic, JsonDocument &payload) {
    if (!this->isRunning) {
#ifdef LOG_DEBUG
        Serial.println("[DEBUG] Received message but ThingClient is not running.");
#endif
        return false;
    }

    if (processShadowMessage(topic, payload)) {
        return true;
    }

    if (processCommandMessage(topic, payload)) {
        return true;
    }

    if (processJobMessage(topic, payload)) {
        return true;
    }

#ifdef LOG_DEBUG
    Serial.printf("[DEBUG] Received topic: %s but no handler matched.\n", topic.c_str());
#endif
    return false;
}

void ThingClient::loop() {
    if (this->isRunning) {
        unsigned long now = millis() + 10000L;

        for (JsonPair kv: this->shadows.as<JsonObject>()) {
            auto key = kv.key();
            auto shadow = this->shadows[key].as<JsonObject>();

#ifdef LOG_TRACE
            Serial.printf("[DEBUG] Visiting shadow '%s'.\n", key.c_str());
#endif
            if (shadow["loaded"].isNull()) {
                auto time = shadow["timestamp"].as<unsigned long>();

#ifdef LOG_TRACE
                Serial.printf("[DEBUG] Checking if we can start sending out %ul, %ul, %d\n", now, time, (now - time) > 10 * 1000);
#endif
                if (now - time > 10 * 1000L) {
                    shadow["timestamp"] = millis() + 10000L;
                    requestShadow(key.c_str());
                    // String shadowTopic = StringPrintF("$aws/things/%s/shadow/name/%s/get",
                    //                                   this->thingName.c_str(),
                    //                                   key.c_str());
                    // this->client->publish(shadowTopic.c_str(), "{}");
#ifdef LOG_DEBUG
                    Serial.printf("[DEBUG] Requested state for shadow '%s'.\n", key.c_str());
#endif
                }
            }
        }
    }
}
