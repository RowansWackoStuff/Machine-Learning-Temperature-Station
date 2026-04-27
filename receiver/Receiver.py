from pyrf24 import RF24, RF24_PA_LOW, RF24_1MBPS, RF24_PA_MAX, RF24_PA_HIGH
import time

# Setup for Raspberry Pi 3
# CE pin is connected to GPIO 22, CSN pin is connected to GPIO 8 (CE0)
radio = RF24(22, 0) 

def setup_receiver():
    # Check if chip is awake
    if not radio.begin():
        raise RuntimeError("nRF24L01 hardware not responding")
    
    radio.setChannel(76)

    # Set the address (must match the transmitter's address)
    address = b"NODE1"
    radio.openReadingPipe(1, address)
    
    # Set power level and data rate
    radio.setPALevel(RF24_PA_HIGH) # Use LOW for close-range testing
    radio.setDataRate(RF24_1MBPS)
    
    # Start listening
    radio.startListening()
    print("Receiver initialized. Waiting for data...")

def run_receiver():
    i = 0
    while True:
        if radio.available():
            # Read the payload
            payload_length = radio.getDynamicPayloadSize()
            data = radio.read(payload_length)
            
            # Decode assuming the sender sent a string
            try:
                decoded_data = data.decode('utf-8')
                print(f"Received: {decoded_data}")
            except UnicodeDecodeError:
                print(f"Received raw bytes: {data}")
        
        time.sleep(0.01) # Small delay to prevent high CPU usage
        # print(i)
        # i += 1

if __name__ == "__main__":
    print("Begin")
    setup_receiver()
    try:
        run_receiver()
    except KeyboardInterrupt:
        print("\nShutting down receiver.")
        radio.stopListening()