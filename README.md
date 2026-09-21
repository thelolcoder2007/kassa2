<!-- markdownlint-disable MD013 -->

# SNTpings streaming host configuration

This repository is used for the configuration of the HSLS streaming computer for SNTpings 2026.
It contains everything you need to have the same configuration as the SNT does.

## Setup instructions

1. Boot into a NixOS `.iso` on a host you want to use as HSLS streaming device.
2. Run the following command: `/tmp/setup/setup.sh`
3. Verify everything is correctly set up.
4. Run `sudo nixos-install --flake "/mnt/etc/nixos/kassa2#kassa2"`
5. Reboot
6. Profit
