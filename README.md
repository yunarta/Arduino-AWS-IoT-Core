# AWS IoT Core with Arduino

This project demonstrates how to connect an Arduino device to AWS IoT Core. It includes examples and instructions for setting up and using the AWS IoT Core service with your Arduino.

## Getting Started

### Prerequisites

- Arduino board (e.g., Arduino Uno, Arduino Nano)
- Arduino IDE installed
- AWS account
- AWS IoT Core service set up

### Installation

1. Clone this repository:
    ```sh
    git clone https://github.com/yourusername/aws-iot-core.git
    cd aws-iot-core
    ```

2. Open the project in Arduino IDE.

3. Install necessary libraries:
    - `ArduinoJson`
    - `Array`
    - `FS`
    - [LittleFS](http://_vscodecontentref_/2)
    - [PubSubClient](http://_vscodecontentref_/3)

### Configuration

1. Create a new thing in AWS IoT Core and download the certificates.
2. Update the `aws_iot_config.h` file with your AWS IoT Core endpoint, thing name, and paths to the certificates.

### Usage

#### FleetProvisioningClient

The [FleetProvisioningClient](http://_vscodecontentref_/4) class handles the provisioning of devices. It requests a certificate from AWS IoT Core and uses it to provision the device.

#### Example

```cpp
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "AwsIoTCore.h"

WiFiClient wifiClient;
PubSubClient client(wifiClient);
FleetProvisioningClient provisioningClient(&client, "ProvisioningTemplateName", "ThingName");

void setup() {
    Serial.begin(115200);
    provisioningClient.begin();
}

void loop() {
    client.loop();
}
```

#### ThingClient
The ThingClient class manages the shadows of IoT things. It allows you to update and retrieve the state of your IoT device.

#### Example

```cpp
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "AwsIoTCore.h"

WiFiClient wifiClient;
PubSubClient client(wifiClient);
ThingClient thingClient(&client, "ThingName");

void setup() {
    Serial.begin(115200);
    thingClient.begin();
}

void loop() {
    client.loop();
    thingClient.loop();
}
```

License
This project is licensed under the MIT License - see the LICENSE file for details.
