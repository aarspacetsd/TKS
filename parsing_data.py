import os
import shutil
import mysql.connector
import time
import csv

WATCH_FOLDER = './csv_data'
DEST_FOLDER = './S3'

# Konfigurasi koneksi MySQL
DB_CONFIG = {
    'host': 'localhost',
    'user': 'root',
    'password': 'your_mysql_password',
    'database': 'sensor_db'
}

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
    lat_str = parts[3].replace('Lat: ', '')
    lng_str = parts[4].replace('Lng: ', '')
    latitude = float(lat_str)
    longitude = float(lng_str)
    ph = float(parts[5])
    return (timestamp, temperature, moisture, latitude, longitude, ph)

def process_file(filepath):
    conn = mysql.connector.connect(**DB_CONFIG)
    cursor = conn.cursor()

    with open(filepath, 'r') as f:
        reader = csv.reader(f)
        for line in reader:
            parsed = parse_line(line)
            if parsed:
                cursor.execute('''
                    INSERT INTO sensor_log (timestamp, temperature, moisture, latitude, longitude, ph)
                    VALUES (%s, %s, %s, %s, %s, %s)
                ''', parsed)
    
    conn.commit()
    conn.close()

    # Pindahkan file ke folder S3
    filename = os.path.basename(filepath)
    shutil.move(filepath, os.path.join(DEST_FOLDER, filename))
    print(f'File {filename} telah diproses dan dipindahkan ke folder S3.')

def watch_folder():
    print(f"Memantau folder: {WATCH_FOLDER}")
    while True:
        files = [f for f in os.listdir(WATCH_FOLDER) if f.endswith('.csv')]
        for file in files:
            filepath = os.path.join(WATCH_FOLDER, file)
            process_file(filepath)
        time.sleep(5)

if __name__ == '__main__':
    os.makedirs(WATCH_FOLDER, exist_ok=True)
    os.makedirs(DEST_FOLDER, exist_ok=True)
    init_db()
    watch_folder()
