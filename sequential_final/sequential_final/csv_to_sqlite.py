import pandas as pd
import sqlite3
import os

# === Settings ===
csv_file = "steam_games.csv"
db_file = "steam.db"
table_name = "steam_games"

# === Step 1: Check CSV file exists ===
if not os.path.exists(csv_file):
    print(f"❌ CSV file '{csv_file}' not found.")
    exit(1)

# === Step 2: Load CSV into DataFrame ===
try:
    df = pd.read_csv(csv_file)
    print(f"✅ Loaded {len(df)} rows from '{csv_file}'")
except Exception as e:
    print(f"❌ Error loading CSV: {e}")
    exit(1)

# === Step 3: Connect to SQLite and create table ===
try:
    conn = sqlite3.connect(db_file)
    df.to_sql(table_name, conn, if_exists="replace", index=False)
    conn.close()
    print(f"✅ Data pushed to SQLite database '{db_file}' (table: '{table_name}')")
except Exception as e:
    print(f"❌ Failed to insert into DB: {e}")
    exit(1)
