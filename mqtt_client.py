import paho.mqtt.client as paho
import time
import matplotlib.pyplot as plt
import numpy as np
import serial
# https://os.mbed.com/teams/mqtt/wiki/Using-MQTT#python-client
num = 0
t = np.arange(0,2,0.1) 
x = np.arange(0,2,0.1)
y = np.arange(0,2,0.1)
z = np.arange(0,2,0.1)
# MQTT broker hosted on local machine
mqttc = paho.Client()
# Settings for connection
# TODO: revise host to your IP
host = "172.20.10.2"
topic = "Mbed"
# Callbacks
def on_connect(self, mosq, obj, rc):
    print("Connected rc: " + str(rc))

def on_message(mosq, obj, msg):
    global num
    # print("Message: " + str(msg.payload))
    line = str(msg.payload)
    line = line.rstrip("'")
    line = line.rstrip("0")
    line = line.rstrip("x")
    line = line.rstrip("\*")
    x_line = line[2:6]
    y_line = line[7:13]
    z_line = line[17:25]
    x[num] = int(x_line)
    y[num] = int(y_line)
    z[num] = int(z_line)
    num = num + 1
def on_subscribe(mosq, obj, mid, granted_qos):
    print("Subscribed OK")

def on_unsubscribe(mosq, obj, mid, granted_qos):
    print("Unsubscribed OK")

# Set callbacks
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_subscribe = on_subscribe
mqttc.on_unsubscribe = on_unsubscribe
# Connect and subscribe
print("Connecting to " + host + "/" + topic)
mqttc.connect(host, port=1883, keepalive=60)
mqttc.subscribe(topic, 0)

for a in range(0, 19):
    mqttc.loop()
    x[a] = x[num]
    y[a] = y[num]
    z[a] = z[num] 

fig, ax = plt.subplots(3, 1)
ax[0].plot(t,x)
ax[0].set_xlabel('Time')
ax[0].set_ylabel('Amplitude')
ax[1].plot(t,y)
ax[1].set_xlabel('Time')
ax[1].set_ylabel('Amplitude')
ax[2].plot(t,z)
ax[2].set_xlabel('Time')
ax[2].set_ylabel('Amplitude')
plt.show()

# Loop forever, receiving messages
mqttc.loop_forever()