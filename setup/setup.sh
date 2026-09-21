#!/usr/bin/env bash

mkdir -p /tmp/kassa2
git clone https://github.com/thelolcoder2007/kassa2.git /tmp/kassa2
sudo nix --experimental-features "nix-command flakes" run github:nix-community/disko/latest -- --mode destroy,format,mount /tmp/kassa2/setup/btrfs-subvolumes.nix
sudo mkdir -p /nix/persist/var/lib/sops-nix/
sudo cp /etc/ssh/ssh_host_ed25519_key /nix/persist/var/lib/sops-nix/key.txt
sudo chmod 0400 /nix/persist/var/lib/sops-nix/key.txt
sudo cp /nix/persist /mnt/nix -r
sudo nixos-generate-config --root /mnt
git clone git@github:thelolcoder2007/kassa2 /mnt/etc/nixos/kassa2
rm /mnt/etc/nixos/kassa2/hosts/hardware-configuration-kassa2.nix
mv /mnt/etc/nixos/hardware-configuration.nix /mnt/etc/nixos/kassa2/hosts/hardware-configuration-kassa2.nix
less /mnt/etc/nixos/kassa2/README.md
