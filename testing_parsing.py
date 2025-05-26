import paho.mqtt.client as mqtt
import time

MQTT_BROKER = 'localhost'   # Samakan dengan broker program utama
MQTT_PORT = 1883
MQTT_TOPIC = 'sensor/csv'

# Dummy CSV content (string), header + 2 baris data contoh
dummy_csv = """timestamp,temperature,moisture,latitude,longitude,ph
2025-05-27 10:00:00,25.6,Basah,"Lat: -6.200000","Lng: 106.816666",6.80
2025-05-27 10:03:00,26.1,Kering,"Lat: -6.201000","Lng: 106.817000",6.75
"""

def main():
    client = mqtt.Client()
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    
    print(f"Mengirim dummy CSV data ke topic {MQTT_TOPIC} ...")
    client.publish(MQTT_TOPIC, dummy_csv)
    print("Data terkirim!")

    # Tunggu sebentar agar pesan terkirim sempurna
    time.sleep(1)
    client.disconnect()

if __name__ == '__main__':
    main()
