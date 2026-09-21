{
  config,
  ...
}:

{
  imports = [
    ./nginx-base.nix
  ];
  systemd.tmpfiles.rules = [
    "d /run/mistserver 0755 root root -"
  ];
  systemd.services.nginx.serviceConfig.ReadOnlyPaths = [
    "/run/mistserver"
  ];
  services.nginx.virtualHosts."bergpad.nationalespeeltuin.nl" = {
    locations."/" = {
      root = "/run/mistserver";
      extraConfig = ''
        autoindex on;
      '';
    };
    http2 = true;
    http3 = true;
    http3_hq = true;
    addSSL = true;
    enableACME = true;
  };
  users.users.root.extraGroups = [ config.users.groups.nginx.name ];
}
