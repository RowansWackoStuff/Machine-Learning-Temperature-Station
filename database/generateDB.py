import sqlite3

def manage_database():
    # Step A & B: Connect and create cursor
    connection = sqlite3.connect('my_data.db')
    cursor = connection.cursor()

    # Step C: Ensure table exists
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS sensor_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            device_id TEXT,
            reading_value REAL
        )
    ''')
    connection.commit()

    while True:
        print("\n--- Database Menu ---")
        print("1. View Table")
        print("2. Clear Table")
        print("3. Exit")
        
        choice = input("Select an option (1-3): ").strip()

        if choice == '1':
            # View the table
            cursor.execute("SELECT * FROM sensor_readings")
            rows = cursor.fetchall()
            
            if not rows:
                print("\n[!] The table is currently empty.")
            else:
                print("\nID | Timestamp | Device ID | Reading")
                print("-" * 40)
                for row in rows:
                    print(f"{row[0]} | {row[1]} | {row[2]} | {row[3]}")

        elif choice == '2':
            # Clear the table
            confirm = input("Are you sure you want to delete ALL data? (y/n): ").lower()
            if confirm == 'y':
                cursor.execute("DELETE FROM sensor_readings")
                connection.commit()
                print("\n[+] Table cleared successfully.")
            else:
                print("\n[#] Operation cancelled.")

        elif choice == '3':
            print("Closing connection...")
            break
        
        else:
            print("[!] Invalid choice. Please try again.")

    # Step D: Close
    connection.close()

if __name__ == "__main__":
    manage_database()