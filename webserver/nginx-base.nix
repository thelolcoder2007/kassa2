{
  security.acme = {
    defaults = {
      email = "thomas.erents@gmail.com";
    };
    acceptTerms = true;
  };
  services.nginx = {
    enable = true;
    recommendedProxySettings = true;
    recommendedTlsSettings = true;
    virtualHosts."localhost" = {
      listenAddresses = [
        "127.0.0.1"
        "[::1]"
      ];
      locations."/stub_status".extraConfig = ''
        stub_status;
        server_tokens on;
        allow 127.0.0.1;
        allow ::1;
        deny all;
      '';
    };
  };
  networking.firewall = {
    allowedTCPPorts = [
      80
      443
    ];
    allowedUDPPorts = [ 443 ];
  };
}
