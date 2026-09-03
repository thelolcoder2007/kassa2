# AMD MA35D SDK v1.5.0 README

## Overview
Welcome to the AMD Media Acceleration Software Development Kit!
This README describes essential steps needed to successfully
prepare your server as well as host system to have a successful
experience using the MA35D Video Acceleration card.

For a complete list of features, limitations, and known issues
for this software release, see the included ReleaseNotes.md file.

## System Setup

**Requirements**

* Ubuntu 20.04 or 22.04 LTS running Linux kernel v5.15, 5.19 or 6.2
  (note: 5.15 or 6.2 preferred)
* PCI slot configured in the mainboard BIOS for x4x4 bifurcation
* IOMMU enabled in the BIOS
* Virtualization Technology enabled in BIOS
* SRIOV Global enabled in BIOS
* If available, enable "above 4g decoding" in BIOS
* If available, enable ACS (Address Control Services) in your BIOS
* Secure boot must be disabled in BIOS
* 16GB RAM
* at least 4 x Zen3 CPU Cores (with support for 8 threads) or equivalent

**Upgrading from SDK v1.0.0**
Note: this release includes new firmware.  Ensure that, after deployment
of the new packages, that the included setup.sh script is run and any
devices flagged by setup.sh as requiring an update via the maflash utility
are updated.

### Host Configuration

The PF driver requires huge pages which in turn requires that IOMMU is enabled.
Configure in Ubuntu as follows:

In `/etc/default/grub`,the following command-line boot parameters are required:

Note: for Intel processors replace “amd_iommu=on” with “intel_iommu=on”

    GRUB_CMDLINE_LINUX_DEFAULT="quiet splash amd_iommu=on iommu=pt"

Apply the above changes to system:

    $ sudo update-grub

Now allocate at least 8GB of huge pages (each page is 2MB; at least 4096 pages)

    $ echo 'vm.nr_hugepages=4096' | sudo tee -a /etc/sysctl.conf

Restart the system:

    $ sudo reboot

Once the system has rebooted confirm that the changes were applied as follows:

Check that the kernel command line used to boot the OS contains the added IOMMU
changes.  For example:

    $ cat /proc/cmdline
    BOOT_IMAGE=/vmlinuz-5.15.0-50-generic root=/dev/mapper/vg00-rootlv ro quiet splash
        amd_iommu=on iommu=pt

Ensure that libhugetlbfs0 is installed:

    $ sudo apt install libhugetlbfs0

Verify that huge pages are available. For example:

    $ cat /proc/meminfo | grep -i huge
    HugePages_Total:    4096
    HugePages_Free:     4096
    HugePages_Rsvd:        0
    HugePages_Surp:        0
    Hugepagesize:       2048 kB

### Card Validation

Once the card is installed in the system check that it is available to the OS as
expected.  The following is an example of the expected output from the given
‘lspci’ command.  If PCIe slot bifurcation is enabled then two devices should be
reported (example below shows only 1 device).  The ‘lspci’ command arguments ask
for extra verbosity (-vv) and to filter PCIe devices by vendor ID (-d 10ee:)
where the hexadecimal value 10EE identifies Xilinx/AMD.

Highlighted below are some key values worth inspecting:

    $ sudo lspci -vvd 10ee:
    01:00.0 Multimedia controller: Xilinx Corporation Device 5070

## Software Prerequisites

### MA35D Software

### Verification of proper SDK installation
Ensure your local shell environment and system are fully prepared by sourcing
the supplied setup script:

    $ sudo source /opt/amd/ama/ma35/scripts/setup.sh

This will ensure you can run the included applications and it will also
configure your system properly for successful use of the MA35D SDK.

To verify that your SDK is installed successfully as well as ensure
your devices are operational, run the following command:

    $ mautil validate --device all

You should see a clear indication, for each device, of a passing
transcode operation.  Any other result may indicate a failure to properly
install and/or configure the SDK for proper operation on your server.
Please refer to this README and/or the Release Notes to ensure
you have properly configured your server and that you are running
a supported host operating system and kernel version.

### Transcoding

This build of the SDK supports integration with ffmpeg (n6.1.1), the Xilinx
Media Acceleration library (XMA), and Gstreamer (v1.22).  Included in the
applications package are sample applications which may be found in the following
paths:

#### FFmpeg application
    /opt/amd/ama/ma35/bin/ffmpeg

#### XMA reference applications
    /opt/amd/ama/ma35/bin/ma35_decoder_app
    /opt/amd/ama/ma35/bin/ma35_encoder_app
    /opt/amd/ama/ma35/bin/ma35_scaler_app
    /opt/amd/ama/ma35/bin/ma35_transcoder_app

#### Gstreamer gst-launch application
    /opt/amd/ama/ma35/bin/gst-launch-1.0

#### FFmpeg transcode example
    $ /opt/amd/ama/ma35/bin/ffmpeg -y -hwaccel ama -hwaccel_device /dev/ama_transcoder0 -c:v h264_ama -i input.264 \
    -c:v hevc_ama -b:v 10M -frames 1200 -f rawvideo  /tmp/h264_to_hevc.hevc

#### XMA transcode example
    $ /opt/amd/ama/ma35/bin/ma35_transcoder_app -streams 1 -frames 2000 -c:v h264_ama \
    -i input.264 -c:v h264_ama -b:v 10M  -o ./transcode/avc_transcode.264

#### Gstreamer gst-launch transcode example
    $ /opt/amd/ama/ma35/bin/gst-launch-1.0 filesrc location=input.264 ! h264parse ! ama_h264dec ! identity eos-after=1200 ! \
    ama_h265enc bitrate=10000000 ! filesink location=/tmp/h264_to_hevc.hevc

### Using mautil for Telemetry

The mautil binary can be used to display the values of various board sensors
(electrical and thermal).

You can obtain the list of devices by requesting a host report:

    $ mautil examine -r host
    System Configuration
    OS Name              : Linux
    Distribution         : Ubuntu 20.04.5 LTS
    Kernel Release       : 5.15.0-50-generic
    OS Version           : #56~20.04.1-Ubuntu SMP Tue Sep 27 15:51:29 UTC 2022
    Machine              : x86_64
    CPU Cores            : 16
    Memory               : 31793 MB
    glibc                : 2.31
    Model                : Precision 3660

    Build Version        : 1.1.0-2401281829
    Build Date           : 2024-01-28 18:29:02-08:00
    Build Tag            : N/A


    Devices present
    device bdf: [0000:29:00.0]
    device bdf: [0000:10:00.0]

For a particular device, a report can be requested for thermal telemetry:

    $ mautil examine -r thermal -d 0000:b4:00.0

    ---------------------------------
    1/1 [0000:b4:00.0] : MA35 Device
    ---------------------------------
    MA35 Thermal Info:
    Device Temperature:
      id: ma35_temp_s3 [111 C]
    Board Temperature:
      id: board_temp [65 C]

as well as electrical telemety:

    $ /opt/amd/ama/ma35/bin/mautil examine -r electrical -d 0000:01:00.0

    1/1 [0000:01:00.0] : MA35 Device
    MA35 Electrical Info:
    Device Electrical Info:
    id: aux [732 mV]
    id: ddr0 [868 mV]
    id: ml_engine [747 mV]
    id: enc [749 mV]
    Board Electrical Info:
    id: 12V PEX Current [666 mA]
    id: 3V AUX Current [80 mA]
    id: 3V PEX Current [226 mA]
    id: 12V PEX Voltage [12016 mV]
    id: 3V AUX Voltage [3328 mV]
    id: 3V PEX Voltage [3328 mV]
    id: board_power [9021 mW]

It is also possible to request both reports at the same time:

    $ /opt/amd/ama/ma35/bin/mautil examine -r electrical thermal -d 0000:01:00.0

    1/1 [0000:01:00.0] : MA35 Device
    MA35 Electrical Info:
    Device Electrical Info:
    id: aux [731 mV]
    id: ddr0 [868 mV]
    id: ml_engine [747 mV]
    id: enc [749 mV]
    Board Electrical Info:
    id: 12V PEX Current [666 mA]
    id: 3V AUX Current [80 mA]
    id: 3V PEX Current [226 mA]
    id: 12V PEX Voltage [12056 mV]
    id: 3V AUX Voltage [3328 mV]
    id: 3V PEX Voltage [3328 mV]
    id: board_power [9047 mW]

    MA35 Thermal Info:
    Device Temperature:
    id: ma35_temp_s2 [89 C]
    Board Temperature:
    id: board_temp [45 C]
