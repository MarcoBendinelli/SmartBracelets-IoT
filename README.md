# Smart Bracelets :white_circle:

The project involves creating a pair of _smart_ bracelets: one for the child and one for the parent. These bracelets track the child's position and behavior, sending alerts when the child goes too far or falls. Here's a summary of how the bracelets operate:

1. **Pairing**: the parent's and child's bracelets broadcast a random key during pairing. If the received key matches the stored one, the devices store each other's addresses. A special message is then transmitted to stop the pairing phase
2. **Operation**: every 10 seconds, the child's bracelet transmits INFO messages containing the child's coordinates and kinematic status
3. **Alerts**: if the child's status indicates a fall, the parent's bracelet sends a FALL alarm. If the parent's bracelet doesn't receive any messages for a minute, it sends a MISSING alarm. In both cases, it also reports the last received position

The project is implemented using **Contiki** for the software and **Cooja** **OS** for simulating the bracelets. **Contiki** is an open-source operating system designed for the Internet of Things devices. It provides a lightweight and flexible platform for developing IoT applications. **Cooja** is a network simulator specifically designed for Contiki-based IoT systems. It allows to simulate the behavior of IoT devices and networks in a virtual environment, aiding in the testing and debugging of IoT applications.
