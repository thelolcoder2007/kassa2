{ config, ... }:
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
        autoindex_format xml;
        xslt_stylesheet ${./autoindex.xsl};
        autoindex_localtime off;

        add_header 'Access-Control-Allow-Origin' '*' always;
        add_header 'Access-Control-Allow-Methods' 'GET, OPTIONS' always;

        if ($request_method = 'OPTIONS') {
            add_header 'Access-Control-Max-Age' 3600;
            add_header 'Content-Type' 'text/plain; charset=utf-8';
            add_header 'Content-Length' 0;
            return 204;
        }
      '';
      tryFiles = "$uri $uri/ /index.html";
    };
    http2 = true;
    http3 = true;
    http3_hq = true;
    addSSL = true;
    enableACME = true;
  };
  users.users.root.extraGroups = [ config.users.groups.nginx.name ];
}
