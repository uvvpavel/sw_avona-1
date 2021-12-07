============================
CES2022 Wake on Keyword Demo
============================

This is the low power wake on keyword Avona CES2022 demo.

****************** 
Supported Hardware
****************** 

This demo runs on the Voice Reference Design board (XMS0001).

***** 
Setup
***** 

This example requires the xcore_sdk (demo/ces2022 branch) and Amazon Wakeword.

Set the environment variable XCORE_SDK_PATH to the root of the xcore_sdk.

.. code-block:: console

    $ export XCORE_SDK_PATH=/path/to/sdk

Set the environment variable WW_PATH to the root of the Amazon Wakeword.

.. code-block:: console

    $ export WW_PATH=/path/to/wakeword


*********************
Building the Firmware
*********************

Run make:

.. code-block:: console

    $ make

After building the firmware, flash the board with it. The following command will both create the filesystem with the wakeword model and flash the board:

.. code-block:: console

    $ make flash

Note that MacOS users will first need to install `dosfstools` before running the above command:

.. code-block:: console

    $ brew install dosfstools


********************
Running the Demo
********************

1) Plug the Voice Reference Design board into a Raspberry Pi 3B+.
2) Plug speakers into the Pi's audio output jack.
3) The Raspberry Pi should have a clean install of Raspberry Pi OS.
4) Copy the 'host/ces_rpi_demo' directory over to the Pi. This can be copied directly onto its SD Card, via SFTP, or any other means.
5) On the Pi, either locally with keyboard and monitor, or via SSH, cd to the ces_rpi_demo directory and build the demo by running:

    .. code-block:: console

      $ make

6) Start the demo by running:

    .. code-block:: console

      $ sudo ./demo.sh

The demo will display the SPI audio buffer level and whether it is listening, not listening, or recording. Initially it will wait for the SPI audio buffer to be full if it isn't already, and then start listening. Once the keyword is heard it will start recording. Viewers will see the buffer level drop as it drains the previous 2 seconds worth of audio from the buffer. It will record for 5 seconds, for a total of 7 seconds including the initial buffer contents, and then will wait for the buffer to fill up before exiting. The recording will then be played out the speakers. The above will then repeat. The demo can be exited at any time by pressing ctrl-c.