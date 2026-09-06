{
  description = "Kassa 2";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-26.05";

    sops-nix = {
      url = "github:Mic92/sops-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };

    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    bart-pkgs = {
      url = "git+https://git.bartoostveen.nl/bart/nix-packages.git";
      inputs.treefmt-nix.follows = "treefmt-nix";
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

    let
      mkPkgs =
        system:
        import nixpkgs {
          inherit system;
          overlays = [
            (_: _: { _bartPackages.prefix = "bart"; })
            inputs.bart-pkgs.overlays.default
          ];
        };
      systems = [ "x86_64-linux" ];

      inherit (nixpkgs.lib)
        genAttrs
        nixosSystem
        ;

      pkgsForSystem = genAttrs systems mkPkgs;

      forEachSystem =
        f:
        genAttrs systems (
          system:
          f {
            inherit system;
            pkgs = pkgsForSystem.${system};
          }
        );
    in
    {
      # for `nix fmt`
      formatter = forEachSystem (
        { pkgs, ... }: (treefmt-nix.lib.evalModule pkgs ./base/treefmt.nix).config.build.wrapper
      );

      # for `nix flake check`
      checks = forEachSystem (
        { pkgs, ... }: {
          formatting = (treefmt-nix.lib.evalModule pkgs ./base/treefmt.nix).config.build.check self;
        }
      );

      nixosConfigurations."kassa2" = nixosSystem {
        pkgs = pkgsForSystem.x86_64-linux;
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
