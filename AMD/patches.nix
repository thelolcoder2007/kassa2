# AMD bullshit
{
  imports = [
    ./ama-ma35d.nix
  ];
  hardware.ama-ma35d = {
    enable = true;
    debDir = ../debs;
  };
}
