#include "mbed.h"
#include "drivers/DigitalOut.h"

#include "erpc_simple_server.h"
#include "erpc_basic_codec.h"
#include "erpc_crc16.h"
#include "UARTTransport.h"
#include "DynamicMessageBufferFactory.h"
#include "blink_led_server.h"
//MQTT
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
/**
 * Macros for setting console flow control.
 */
#define CONSOLE_FLOWCONTROL_RTS     1
#define CONSOLE_FLOWCONTROL_CTS     2
#define CONSOLE_FLOWCONTROL_RTSCTS  3
#define mbed_console_concat_(x) CONSOLE_FLOWCONTROL_##x
#define mbed_console_concat(x) mbed_console_concat_(x)
#define CONSOLE_FLOWCONTROL mbed_console_concat(MBED_CONF_TARGET_CONSOLE_UART_FLOW_CONTROL)

//accelerometer
#include "stm32l475e_iot01_accelero.h"

EventQueue queue(32 * EVENTS_EVENT_SIZE);
Thread t;
Thread MQTT_thread;
int16_t pDataXYZ[3] = {0};
int type = 0;


// GLOBAL VARIABLES
WiFiInterface *wifi;
InterruptIn btn2(BUTTON1);
// /InterruptIn btn3(SW3);
volatile int message_num = 0;
volatile int arrivedcount = 0;
volatile bool closed = false;

const char* topic = "Mbed";

Thread mqtt_thread(osPriorityHigh);
EventQueue mqtt_queue;

void messageArrived(MQTT::MessageData& md) {
        MQTT::Message &message = md.message;
        char msg[300];
        sprintf(msg, "Message arrived: QoS%d, retained %d, dup %d, packetID %d\r\n", message.qos, message.retained, message.dup, message.id);
        printf(msg);
        ThisThread::sleep_for(1000ms);
        char payload[300];
        sprintf(payload, "Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
        printf(payload);
        ++arrivedcount;
}

void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client) {
    if (type){
        MQTT::Message message;
        BSP_ACCELERO_AccGetXYZ(pDataXYZ);
        char buff[100];
        sprintf(buff, "%d   %d        %d", pDataXYZ[0], pDataXYZ[1], pDataXYZ[2]);
        message.qos = MQTT::QOS0;
        message.retained = false;
        message.dup = false;
        message.payload = (void*) buff;
        message.payloadlen = strlen(buff) + 1;
        int rc = client->publish(topic, message);
        // printf("rc:  %d\r\n", rc);
        printf("%s\n", buff);
    }else{}
}

void close_mqtt() {
    closed = true;
}

/****** erpc declarations *******/
void led_on(uint8_t a) {
    if(a){
        type = 1;
    }    
}

void led_off(uint8_t a) {
    if(a)   type = 0;
}

/** erpc infrastructure */
ep::UARTTransport uart_transport(D1, D0, 9600);
ep::DynamicMessageBufferFactory dynamic_mbf;
erpc::BasicCodecFactory basic_cf;
erpc::Crc16 crc16;
erpc::SimpleServer rpc_server;

/** LED service */
LEDBlinkService_service led_service;

int MQTT_Thread(){
    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
            printf("ERROR: No WiFiInterface found.\r\n");
            return -1;
    }


    printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
            printf("\nConnection error: %d\r\n", ret);
            return -1;
    }
 
    NetworkInterface* net = wifi;
    MQTTNetwork mqttNetwork(net);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    //TODO: revise host to your IP
    const char* host = "172.20.10.2";
    const int port=1883;
    printf("Connecting to TCP network...\r\n");
    printf("address is %s/%d\r\n", host, port);

    int rc = mqttNetwork.connect(host, port);//(host, 1883);
    if (rc != 0) {
            printf("Connection error.");
            return -1;
    }
    printf("Successfully connected!\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Mbed";

    if ((rc = client.connect(data)) != 0){
            printf("Fail to connect MQTT\r\n");
    }
    // if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
    //         printf("Fail to subscribe\r\n");
    // }

    mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));
    mqtt_queue.call_every(100ms, &publish_message, &client);

    int num = 0;
    while (num != 5) {
            client.yield(100);
            ++num;
    }

    while (1) {
            if (closed) break;
            client.yield(500);
            ThisThread::sleep_for(500ms);
    }

    printf("Ready to close MQTT Network......\n");

    if ((rc = client.unsubscribe(topic)) != 0) {
        printf("Failed: rc from unsubscribe was %d\n", rc);
    }
    if ((rc = client.disconnect()) != 0) {
        printf("Failed: rc from disconnect was %d\n", rc);
    }

    mqttNetwork.disconnect();
    printf("Successfully closed!\n");

    return 0;
}

int main(void) {
    //Initial accelerometer
    printf("Start accelerometer init\n");
    BSP_ACCELERO_Init();
    MQTT_thread.start(MQTT_Thread);
    // Initialize the rpc server
     uart_transport.setCrc16(&crc16);

    // Set up hardware flow control, if needed
    #if CONSOLE_FLOWCONTROL == CONSOLE_FLOWCONTROL_RTS
        uart_transport.set_flow_control(mbed::SerialBase::RTS, STDIO_UART_RTS, NC);
    #elif CONSOLE_FLOWCONTROL == CONSOLE_FLOWCONTROL_CTS
        uart_transport.set_flow_control(mbed::SerialBase::CTS, NC, STDIO_UART_CTS);
    #elif CONSOLE_FLOWCONTROL == CONSOLE_FLOWCONTROL_RTSCTS
        uart_transport.set_flow_control(mbed::SerialBase::RTSCTS, STDIO_UART_RTS, STDIO_UART_CTS);
    #endif

    printf("Initializing server.\n");
    rpc_server.setTransport(&uart_transport);
    rpc_server.setCodecFactory(&basic_cf);
    rpc_server.setMessageBufferFactory(&dynamic_mbf);

    // Add the led service to the server
    printf("Adding LED server.\n");
    rpc_server.addService(&led_service);

    // Run the server. This should never exit
    printf("Running server.\n");
    rpc_server.run();
}

