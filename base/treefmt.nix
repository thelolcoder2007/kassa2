{ ... }:

{
  # Used to find the project root
  projectRootFile = "flake.nix";

  programs = {
    # keep-sorted start
    deadnix.enable = true;
    jsonfmt.enable = true;
    keep-sorted.enable = true;
    mdsh.enable = true;
    nixf-diagnose.enable = true;
    nixfmt.enable = true;
    # keep-sorted end
    mdformat = {
      enable = true;
      settings.number = true;
    };
    yamllint = {
      enable = true;
      settings.rules.line-length = false;
    };
  };
}
