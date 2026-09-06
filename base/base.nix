{
  config,
  lib,
  pkgs,
  ...
}:

{
  imports = [
    ./sops.nix
  ];

  # Github plz no ratelimit tyyy
  sops.secrets.github_token = { };
  sops.templates."nix-github-token.env".content = ''
    access-tokens = github.com=${config.sops.placeholder.github_token}
  '';

  nix = {
    extraOptions = ''
      !include ${config.sops.templates."nix-github-token.env".path}
    '';
    settings = {
      trusted-users = lib.attrNames (lib.filterAttrs (_: val: val.isNormalUser) config.users.users);
      cores = 0;
      max-jobs = "auto";
      experimental-features = [
        "nix-command"
        "flakes"
        "pipe-operators"
      ];
    };
  };

  environment = {
    variables = {
      # keep-sorted start
      "NH_FLAKE" = "/etc/nixos/kassa2";
      # keep-sorted end
    };
    systemPackages = with pkgs; [
      # keep-sorted start
      bat
      btop
      comma
      dig
      fastfetch
      file
      gh
      git
      htop
      iftop
      iotop
      nano
      nh
      nixfmt
      nixfmt-tree
      pciutils
      screen
      tcpdump
      traceroute
      usbutils
      vim
      wget
      # keep-sorted end
    ];
  };

  boot = {
    loader.grub = {
      # Use GRUB
      enable = true;
      device = "nodev"; # Boot EFI please
      efiSupport = true;
      efiInstallAsRemovable = true; # Just because this is easier to boot from on virtualized hosts
    };
    kernel.sysctl."kernel.task_delayacct" = 1;
    kernelPackages = pkgs.linuxPackages_latest;
  };

  time.timeZone = "Europe/Amsterdam";

  system.stateVersion = "26.05";

  nixpkgs.hostPlatform = lib.mkDefault "x86_64-linux";
}
