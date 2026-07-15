{
  config,
  inputs,
  lib,
  pkgs,
  ...
}:

{
  imports = [
    inputs.home-manager.nixosModules.home-manager
    ./sops.nix
  ];
  home-manager.users =
    lib.genAttrs (lib.attrNames (lib.filterAttrs (_: val: val.isNormalUser) config.users.users))
      (_: {
        home = {
          stateVersion = "26.11";
          file.".local/share/nix/trusted-settings.json" = {
            text = ''
              {
                "extra-substituters": {
                  "https://nix-community.cachix.org": true
                },
                "extra-trusted-public-keys": {
                  "nix-community.cachix.org-1:mB9FSh9qf2dCimDSUo8Zy7bkq5CX+/rkCWyvRCYg3Fs=": true
                }
              }
            '';
          };
        };
      });
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
      trusted-users = (config.users.users |> lib.filterAttrs (_: val: val.isNormalUser) |> lib.attrNames);
      cores = 0;
      max-jobs = "auto";
      experimental-features = [
        "nix-command"
        "flakes"
        "pipe-operators"
      ];
      extra-trusted-substituters = [
        # keep-sorted start
        "https://nix-community.cachix.org"
        # keep-sorted end
      ];
      extra-trusted-public-keys = [
        # keep-sorted start
        "nix-community.cachix.org-1:mB9FSh9qf2dCimDSUo8Zy7bkq5CX+/rkCWyvRCYg3Fs="
        # keep-sorted end
      ];
    };
  };

  environment = {
    variables = {
      # keep-sorted start
      "NH_FLAKE" = "/etc/nixos/nixos-repository";
      # keep-sorted end
    };
    systemPackages = with pkgs; [
      # keep-sorted start
      bat
      btop
      dig
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
      tcpdump
      traceroute
      vim
      wget
      # keep-sorted end
    ];
  };

  boot.kernel.sysctl."kernel.task_delayacct" = 1;
  boot = {
    loader.grub = {
      # Use GRUB
      enable = true;
      device = "nodev"; # Boot EFI please
      efiSupport = true;
      efiInstallAsRemovable = true; # Just because this is easier to boot from on virtualized hosts
    };
    kernelPackages = pkgs.linuxPackages_latest;
  };

  time.timeZone = "Europe/Amsterdam";

  system.stateVersion = "26.05";

  nixpkgs.hostPlatform = lib.mkDefault "x86_64-linux";
}
