import os
import shutil
import mysql.connector
import csv
import io
import paho.mqtt.client as mqtt

DEST_FOLDER = './S3'

# Konfigurasi koneksi MySQL
DB_CONFIG = {
    'host': 'localhost',
    'user': 'root',
    'password': 'AhmadAkmal12@',
    'database': 'sensor_db'
}

MQTT_BROKER = '35.173.244.119'  # Ganti sesuai broker MQTT-mu
MQTT_PORT = 1883
MQTT_TOPIC = 'sensor/csv'

def init_db():
    conn = mysql.connector.connect(
        host=DB_CONFIG['host'],
        user=DB_CONFIG['user'],
        password=DB_CONFIG['password']
    )
    cursor = conn.cursor()
    cursor.execute("CREATE DATABASE IF NOT EXISTS sensor_db")
    conn.database = DB_CONFIG['database']
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS sensor_log (
            id INT AUTO_INCREMENT PRIMARY KEY,
            timestamp DATETIME,
            temperature FLOAT,
            moisture VARCHAR(20),
            latitude FLOAT,
            longitude FLOAT,
            ph FLOAT
        )
    ''')
    conn.commit()
    conn.close()

def parse_line(line):
    parts = [p.strip() for p in line]
    if len(parts) != 6:
        print("Format tidak sesuai:", line)
        return None
    timestamp = parts[0]
    temperature = float(parts[1])
    moisture = parts[2]
    lat_str = parts[3].replace('Lat: ', '').replace('"', '')
    lng_str = parts[4].replace('Lng: ', '').replace('"', '')
    latitude = float(lat_str)
    longitude = float(lng_str)
    ph = float(parts[5])
    return (timestamp, temperature, moisture, latitude, longitude, ph)

def process_csv_string(csv_string, filename='mqtt_data.csv'):
    conn = mysql.connector.connect(**DB_CONFIG)
    cursor = conn.cursor()

    # Simpan dulu file CSV ke folder S3 (optional)
    if not os.path.exists(DEST_FOLDER):
        os.makedirs(DEST_FOLDER)
    filepath = os.path.join(DEST_FOLDER, filename)
    with open(filepath, 'w') as f:
        f.write(csv_string)

    # Proses CSV dari string menggunakan io.StringIO
    f = io.StringIO(csv_string)
    reader = csv.reader(f)
    header = next(reader, None)  # Skip header baris pertama jika ada
    for line in reader:
        parsed = parse_line(line)
        if parsed:
            cursor.execute('''
                INSERT INTO sensor_log (timestamp, temperature, moisture, latitude, longitude, ph)
                VALUES (%s, %s, %s, %s, %s, %s)
            ''', parsed)
    conn.commit()
    conn.close()

    print(f"Data dari {filename} sudah diproses dan disimpan ke database serta file disimpan di folder {DEST_FOLDER}")

# MQTT Callback saat menerima pesan
def on_message(client, userdata, msg):
    print(f"Terima pesan di topik {msg.topic}")
    payload = msg.payload.decode('utf-8')
    # Proses payload CSV langsung
    process_csv_string(payload, filename='mqtt_data.csv')

def main():
    init_db()

    client = mqtt.Client()
    client.on_message = on_message

    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.subscribe(MQTT_TOPIC)

    print(f"Menghubungkan ke broker MQTT di {MQTT_BROKER}:{MQTT_PORT} dan subscribe topik '{MQTT_TOPIC}'")
    client.loop_forever()

if __name__ == '__main__':
    main()
