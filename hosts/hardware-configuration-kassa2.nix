{
  config,
  lib,
  modulesPath,
  ...
}:

{
  imports = [
    (modulesPath + "/installer/scan/not-detected.nix")
  ];

  boot.initrd.availableKernelModules = [
    "nvme"
    "xhci_pci"
    "ahci"
    "usbhid"
    "usb_storage"
    "sd_mod"
  ];
  boot.initrd.kernelModules = [ ];
  boot.kernelModules = [ "kvm-amd" ];
  boot.extraModulePackages = [ ];
  fileSystems = {
    "/" = {
      device = "/dev/disk/by-uuid/ae25ecae-4dc5-45e0-a7e8-8846d0615806";
      fsType = "btrfs";
      options = [ "subvol=root" ];
    };

    "/boot" = {
      device = "/dev/disk/by-uuid/747D-C42E";
      fsType = "vfat";
      options = [
        "fmask=0077"
        "dmask=0077"
      ];
    };

    "/nix" = {
      device = "/dev/disk/by-uuid/ae25ecae-4dc5-45e0-a7e8-8846d0615806";
      fsType = "btrfs";
      options = [ "subvol=nix" ];
    };
    "/var/lib/ffmpeg" = {
      device = "/dev/disk/by-uuid/7e790150-8de0-4187-af7b-b6a42d674dae";
      fsType = "ext4";
    };
  };

  swapDevices = [ ];

  nixpkgs.hostPlatform = lib.mkDefault "x86_64-linux";
  hardware.cpu.amd.updateMicrocode = lib.mkDefault config.hardware.enableRedistributableFirmware;
}
