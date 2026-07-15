{
  lib,
  host,
  ...
}:
with host;
if vmwareHost && qemuHost then
  throw "Configuration Error: Cannot run both VMware and QEMU hosts simultaneously. Please set one of the host parameters (vmwareHost or qemuHost) to false."
else
  let
    baseConfig = {
      boot.initrd.availableKernelModules = [
        "ata_piix"
        "sd_mod"
        "sr_mod"
      ];
      boot.initrd.kernelModules = [ ];
      boot.extraModulePackages = [ ];

      fileSystems = {
        "/" = {
          device = "/dev/disk/by-uuid/${guid_root}";
          fsType = "btrfs";
        };
        "/nix" = {
          device = "/dev/disk/by-uuid/${guid_root}";
          fsType = "btrfs";
          options = [ "subvol=nix" ];
        };
        "/boot" = {
          device = "/dev/disk/by-uuid/${guid_boot}";
          fsType = "vfat";
          options = [
            "fmask=0077"
            "dmask=0077"
          ];
        };
      };
    };

    vmwareChanges = lib.optionalAttrs vmwareHost {
      virtualisation.vmware.guest.enable = true;
      boot.initrd.availableKernelModules = [
        "vmw_pvscsi"
      ];
    };

    qemuChanges = lib.optionalAttrs qemuHost {
      services.qemuGuest.enable = true;
      boot.initrd.availableKernelModules = [
        "uhci_hcd"
        "virtio_pci"
        "virtio_scsi"
      ];
      boot.kernelModules = [ "kvm-intel" ];
    };

  in
  baseConfig // qemuChanges // vmwareChanges
