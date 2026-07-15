{
  # Used to find the project root
  projectRootFile = "flake.nix";
  programs = {
    # keep-sorted start
    deadnix.enable = true;
    flake-edit.enable = true;
    jsonfmt.enable = true;
    keep-sorted.enable = true;
    mdsh.enable = true;
    nixf-diagnose.enable = true;
    nixfmt.enable = true;
    shellcheck.enable = true;
    # keep-sorted end
    mdformat = {
      enable = true;
      settings.number = true;
    };
    shfmt = {
      simplify = true;
      enable = true;
    };
    yamllint = {
      enable = true;
      settings.rules.line-length = false;
    };
  };
}
