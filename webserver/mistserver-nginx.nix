{
  config,
  lib,
  pkgs,
  ...
}:

{
  imports = [
    ./nginx-base.nix
  ];
  services.nginx.virtualHosts = {

    # Management interface (port 4343)
    "bergpad.nationalespeeltuin.nl:4343" =
      (import ./certs/nginx-vhost-snakeoil.nix { inherit pkgs; })
      // {
        serverName = "bergpad.nationalespeeltuin.nl";
        listen = lib.mkForce [
          {
            addr = "0.0.0.0";
            port = 4343;
            ssl = true;
          }
          {
            addr = "[::]";
            port = 4343;
            ssl = true;
          }
        ];
        locations."/" = {
          proxyPass = "http://127.0.0.1:4242$request_uri";
          proxyWebsockets = true;
        };
        http3_hq = true;
      };

    # PNG folder (port 443)
    "bergpad.nationalespeeltuin.nl" = {
      locations."/" = {
        root = "/var/mistserver/screenshots";
        extraConfig = ''
          autoindex on;
        '';
      };
      listen = lib.mkForce [
        {
          addr = "0.0.0.0";
          port = 443;
          ssl = true;
        }
        {
          addr = "[::]";
          port = 443;
          ssl = true;
        }
      ];
      http2 = true;
      http3 = true;
      http3_hq = true;
      addSSL = true;
      sslCertificate = "/etc/letsencrypt/live/bergpad.nationalespeeltuin.nl/fullchain.pem";
      sslCertificateKey = "/etc/letsencrypt/live/bergpad.nationalespeeltuin.nl/privkey.pem";
    };
  };
  users.users.root.extraGroups = [ config.users.groups.nginx.name ];
}
