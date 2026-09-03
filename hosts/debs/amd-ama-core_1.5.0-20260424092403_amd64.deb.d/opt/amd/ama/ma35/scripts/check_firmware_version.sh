# Check for out of date firmware
bad=0
device_cnt=0
flash_sample_commands=""
device_fw_info=""
bdfs_to_flash="-d "

bus_ids=$(lspci -d 10ee:5070|cut -d ' ' -f 1|sort)
transcoder_buses="$(cat /sys/devices/virtual/misc/ama_transcoder*/bus_id | sort)"
if [ $(echo "$bus_ids" | wc -l) -gt $(echo "$transcoder_buses" | wc -l) ]; then # Check if all the buses were inserted
  bad_bus_ids=""
  for bus_id in $bus_ids; do
    if ! echo "$transcoder_buses" | grep -q "$bus_id"; then
      bad_bus_ids="$bad_bus_ids $bus_id"
    fi
  done
  echo -e "Some MA35D devices were not detected. Perhaps you need to insert the kernel module:\n"
  echo -e "\t\$ sudo modprobe ama_transcoder\n"

  echo -e "Perhaps the card(s) need to be flashed:\n"
  for bus_id in $bad_bus_ids; do
    echo -e "\t\$ sudo /opt/amd/ama/ma35/bin/mamgmt flash --force -d $bus_id -p /opt/amd/ama/ma35/firmware/ma35_firmware.bin"
  done
fi

transcoder_devices=$(find /sys/devices/virtual/misc/ -name "ama_transcoder*")
for transcoder_device in $transcoder_devices; do
  bus_id=$(cat $transcoder_device/bus_id)
  curr_zsp_ver=$(grep 'ZSP Version' $transcoder_device/version_information | tr -d ' ' | awk -F = '{print $2}')
  curr_sc_ver=$(grep 'SC Version' $transcoder_device/version_information | tr -d ' ' | awk -F = '{print $2}')

  if [[ "$curr_zsp_ver" == "0.0.0" || "$curr_zsp_ver" == "255.255.255" || "$curr_sc_ver" == "0.0.0" || "$curr_sc_ver" == "255.255.255" ]]; then
    echo "An error occured booting ma35 device $bus_id. Please reboot and try again. If this problem persists, please email your AMD contact or ama_support@amd.com."
    return
  fi

  rel_zsp_fw=""
  rel_sc_fw=""
  rel_fw_vers=""

  rel_fw_vers=$(/opt/amd/ama/ma35/bin/mamgmt examine -d $bus_id --report device-hw | grep -e "SC" -e "ZSP")
  if [ ! -n "rel_fw_vers" ]; then # string "rel_fw_vers" is empty
    echo "Problem while executing: \"/opt/amd/ama/ma35/bin/mamgmt examine -d all --report device-hw\""
    exit
  fi

  IFS=$'\n' read -d '' -r -a lines <<< "$rel_fw_vers"
  for rel_fw in "${lines[@]}";
  do
    component=$(echo "$rel_fw" | cut -d':' -f1 | xargs) #Extract FW type
    # echo "Component: $component"
    if echo "$rel_fw" | grep -q "upgradeable"; then # Check if FW upgrade is required
      version_current=$(echo "$rel_fw" | cut -d':' -f2 | awk '{print $1}')  #Extract current FW version
      version_upgrade=$(echo "$rel_fw" | awk -F'upgradeable to ' '{print $2}' | sed 's/)//' | xargs)  #Extract upgradeable FW version
      # echo "Current Version: $version_current ; Upgradeable Version: $version_upgrade"

      if [ "$component" == "ZSP" ] && [ ! -n "$rel_zsp_fw" ]; then # string "rel_zsp_fw" is empty
        rel_zsp_fw=$version_upgrade
      elif [ "$component" == "SC" ] && [ ! -n "$rel_sc_fw" ]; then # string "rel_sc_fw" is empty
        rel_sc_fw=$version_upgrade
      fi
    else  #No upgrade, just take the current running version info
      if [ "$component" == "ZSP" ] && [ ! -n "$rel_zsp_fw" ]; then # string "rel_zsp_fw" is empty
        rel_zsp_fw=$curr_zsp_ver
      elif [ "$component" == "SC" ] && [ ! -n "$rel_sc_fw" ]; then # string "rel_sc_fw" is empty
        rel_sc_fw=$curr_sc_ver
      fi
    fi
  done

  if [ ! -n "rel_sc_fw" ] || [ ! -n "rel_zsp_fw" ]; then # string "rel_sc_fw/rel_zsp_fw" is empty
    echo "Problem while getting ZSP/SC FW version info for $bus_id!"
    continue
  fi
  # echo "Latest FW version --> ZSP: $rel_zsp_fw, SC: $rel_sc_fw"

  IFS='.' read -r rel_zsp_major rel_zsp_minor rel_zsp_patch <<< "$rel_zsp_fw"
  IFS='.' read -r rel_sc_major rel_sc_minor rel_sc_patch <<< "$rel_sc_fw"

  IFS='.' read -r zsp_ver_major zsp_ver_minor zsp_ver_patch <<< "$curr_zsp_ver"
  IFS='.' read -r sc_ver_major sc_ver_minor sc_ver_patch <<< "$curr_sc_ver"

  if (( ((zsp_ver_major > rel_zsp_major) || (zsp_ver_major == rel_zsp_major && (zsp_ver_minor > rel_zsp_minor || (zsp_ver_minor == rel_zsp_minor && zsp_ver_patch >= rel_zsp_patch)))) && ((sc_ver_major > rel_sc_major) || (sc_ver_major == rel_sc_major && (sc_ver_minor > rel_sc_minor || (sc_ver_minor == rel_sc_minor && sc_ver_patch >= rel_sc_patch)))) )); then
    continue
  fi

  device_fw_info+="$bus_id \t\t $curr_zsp_ver \t\t\t\t $rel_zsp_fw \t\t\t $curr_sc_ver \t\t\b\b $rel_sc_fw \n"
  bdfs_to_flash+="${bus_id#0000:} "
  bad=1
  let device_cnt++
done

if [ $bad -ne 0 ]; then
  echo
  if [[ "$(lspci -D -d 10ee:5071)" && "$bus_ids" == "" ]]; then
    echo "This environment has out of date firmware. Please report this to a system administrator."
    return
  fi
  echo "The following MA35D devices need an update to work as expected with this version of the AMA Video SDK."
  echo -e "The following device summary shows the current device firmware and the required update:\n"
  echo -e " [PCI BDF] \t [Current ZSP Firmware] \t [New ZSP Firmware] \t [Current SC Firmware] \t [New SC Firmware]"
  echo -e "$device_fw_info"

  flash_sample_commands="sudo /opt/amd/ama/ma35/bin/mamgmt flash --force $bdfs_to_flash -p /opt/amd/ama/ma35/firmware/ma35_firmware.bin"
  echo -e "\nThe setup script will update the cards to the values above. Cold boot the server once the update is done."
  read -p "Do you want to proceed? [Y/N]: " user_input
  user_resp=$(echo "$user_input" | tr '[:lower:]' '[:upper:]')
  if [ "$user_resp" == "Y" ]; then
    echo -e "$flash_sample_commands"
    $flash_sample_commands
  elif [ "$user_resp" == "N" ]; then
    echo "Exiting...."
  else
      echo "Invalid choice. Please enter 'Y' or 'N'."
  fi
fi

