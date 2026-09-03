#!/bin/bash

# Disable FLR and VF auto-probe
bus_ids=$(lspci -D -d 10ee:5070|cut -d ' ' -f 1|sort)
num_buses=$(echo "$bus_ids" | wc -l)
for bus_id in $bus_ids; do
  reset_method="/sys/bus/pci/devices/$bus_id/reset_method"
  if [ -e "$reset_method" ] && [[ "`cat $reset_method`" != "" ]]; then
    echo ''|tee $reset_method 1>/dev/null
  fi
  vf_auto_probe="/sys/bus/pci/devices/$bus_id/sriov_drivers_autoprobe"
  if [ -e "$vf_auto_probe" ] && [[ "`cat $vf_auto_probe`" != 0 ]]; then
    echo 0|tee $vf_auto_probe 1>/dev/null
  fi
done
