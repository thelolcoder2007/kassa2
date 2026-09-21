{
  description = "The streaming host for SNTpings 2026";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

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

      nixosConfigurations."kassa2" = nixpkgs.lib.nixosSystem {
        modules = [
          ./hosts/kassa2.nix
          ./hosts/hardware-configuration-kassa2.nix
        ];
        specialArgs = {
          inherit inputs;
        };
      };
    };
}
