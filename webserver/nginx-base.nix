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
  };
  networking.firewall = {
    allowedTCPPorts = [
      80
      443
    ];
    allowedUDPPorts = [ 443 ];
  };
}
