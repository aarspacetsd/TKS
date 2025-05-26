import paho.mqtt.client as mqtt

BROKER = '35.173.244.119'
PORT = 1883
TOPIC = 'test/topic'

client = mqtt.Client()
client.connect(BROKER, PORT, 60)
client.publish(TOPIC, "Halo dari lokal!")
print("Pesan terkirim ke NanoMQ!")
client.disconnect()
