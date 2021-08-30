# 02_Project_Embedded_System
Standard Setup of Toolchain for Linux

Step 1. Install Prerequisites
To compile with ESP-IDF you need to get the following packages. The command to run depends on which distribution of Linux you are using:

Ubuntu and Debian:
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-setuptools cmake ninja

Step 2. Get ESP-IDF
Linux and macOS

Open Terminal, and run the following commands:
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git

ESP-IDF will be downloaded into ~/esp/esp-idf.
Consult ESP-IDF Versions for information about which ESP-IDF version to use in a given situation.

Step 3. Set up the tools
Aside from the ESP-IDF, you also need to install the tools used by ESP-IDF, such as the compiler, debugger, Python packages, etc.

Linux and macOS
cd ~/esp/esp-idf
./install.sh esp32

To prefer the Espressif download server when installing tools, use the following sequence of commands when running install.sh:
cd ~/esp/esp-idf
export IDF_GITHUB_ASSETS="dl.espressif.com/github_assets"
./install.sh

Step 4. Set up the environment variables
The installed tools are not yet added to the PATH environment variable. To make the tools usable from the command line, some environment variables must be set. ESP-IDF provides another script which does that.

Linux and macOS
In the terminal where you are going to use ESP-IDF, run:
. $HOME/esp/esp-idf/export.sh

Step 5. Start a Project
Linux and macOS
cd ~/esp
git clone https://github.com/Ducndc/02_Project_Embedded_System.git

Step 6. Connect Your Device
Now connect your ESP32 board to the computer and check under what serial port the board is visible.
Linux: starting with /dev/tty

Step 7. Configure
Linux and macOS
cd ~/esp/02_Project_Embedded_System
idf.py set-target esp32
idf.py menuconfig

Step 8. Build the Project
idf.py build

Step 9. Flash onto the Device
Flash the binaries that you just built (bootloader.bin, partition-table.bin and hello-world.bin) onto your ESP32 board by running:
idf.py -p PORT [-b BAUD] flash
You can also change the flasher baud rate by replacing BAUD with the baud rate you need. The default baud rate is 460800

Step 10. Monitor
To check if “hello_world” is indeed running, type idf.py -p PORT monitor (Do not forget to replace PORT with your serial port name).
idf.py -p /dev/ttyUSB0 monitor


