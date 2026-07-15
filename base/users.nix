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
  sops.secrets."thomas_userpassword" = {
    neededForUsers = true;
  };
  sops.secrets."jetse_userpassword" = {
    neededForUsers = true;
  };
  sops.secrets."root_userpassword" = {
    neededForUsers = true;
  };

  users = {
    defaultUserShell = lib.mkForce pkgs.bash;
    mutableUsers = false;
    users = {
      thomas = {
        isNormalUser = true;
        extraGroups = [
          "wheel"
        ];
        openssh.authorizedKeys.keys = [
          "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIC/7b7OH03t60heXNS8OUpmNVgOhUFwcLLQmP0gBC1SQ thomas@HPTHOMAS-ARCH"
        ];
        hashedPasswordFile = config.sops.secrets."thomas_userpassword".path;
      };
      jetse = {
        isNormalUser = true;
        extraGroups = [
          "wheel"
        ];
        openssh.authorizedKeys.keys = [
          "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAID9xQnKsLtZHofQ3AsfFJeBf6cyRJcAmvnHEtj3syTrm jetse@snt.utwente.nl"
          "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIA7QsYc+qndPqCDrwmK0csfhEuXUX+QkjinOjvSZBAsD jetse@novoserve"
        ];
        hashedPasswordFile = config.sops.secrets."jetse_userpassword".path;
      };
      root.hashedPasswordFile = config.sops.secrets."root_userpassword".path;
    };
  };
}
