# config.py

SENSORS = ['temperature']
ACTUATORS = ['feeder', 'thermostat', 'filter', 'CO2', 'lights']
FLAGS = ['feeder_status', 'feeder_duration', 'season_setting', 'reset_flag']

# SQLite DB file
DB_FILE = 'data/sensor_data.db'

# JSON file to save actuator + flag state
STATE_FILE = 'data/state.json'
