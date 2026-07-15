{ lib, ... }:

{
  fileSystems."/" = {
    device = "/nonexistent";
    fsType = "ext4";
  };

  nixpkgs.hostPlatform = lib.mkDefault "x86_64-linux";
  boot.loader.grub.device = "nodev";
  system.stateVersion = "26.05";
}
