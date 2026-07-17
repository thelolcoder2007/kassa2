{ config, ... }:

{
  imports = [
    ./nginx-base.nix
  ];
  services.nginx.virtualHosts."kassa2.nationalespeeltuin.nl" = {
    locations."/" = {
      root = "/var/mistserver/screenshots";
      extraConfig = ''
        autoindex on;
      '';
    };
    forceSSL = true;
    sslCertificate = "/etc/letsencrypt/live/bergpad.nationalespeeltuin.nl/fullchain.pem";
    sslCertificateKey = "/etc/letsencrypt/live/bergpad.nationalespeeltuin.nl/privkey.pem";
  };
  users.users.root.extraGroups = [ config.users.groups.nginx.name ];
}
