{
  description = "Kassa 2";

  nixConfig = {
    extra-substituters = [
      "https://nix-community.cachix.org"
    ];
    extra-trusted-public-keys = [
      "nix-community.cachix.org-1:mB9FSh9qf2dCimDSUo8Zy7bkq5CX+/rkCWyvRCYg3Fs="
    ];
  };

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };

    sops-nix = {
      url = "github:Mic92/sops-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };

    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      nixpkgs,
      treefmt-nix,
      self,
      ...
    }@inputs:
    {
      # for `nix fmt`
      formatter.x86_64-linux =
        (treefmt-nix.lib.evalModule nixpkgs.outputs.legacyPackages.x86_64-linux ./base/treefmt.nix)
        .config.build.wrapper;
      # for `nix flake check`
      checks.x86_64-linux.formatting = (treefmt-nix.lib.evalModule nixpkgs.outputs.legacyPackages.x86_64-linux ./base/treefmt.nix).config.build.check self;

      # Host configs:
      nixosConfigurations =
        let
          inherit (nixpkgs.lib) nixosSystem;
        in
        {
          "kassa2" = nixosSystem {
            modules = [
              ./hosts/kassa2.nix
              ./hosts/hardware-configuration-kassa2.nix
            ];
            specialArgs = {
              inherit inputs;
            };
          };
        };
    };
}
