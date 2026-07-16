{
  config,
  ...
}:
{
  imports = [
    ./mistserver-pkg/options.nix
  ];

  environment.systemPackages = [ config.services.mistServer.package ];

  networking.firewall.extraInputRules = ''
    ip6 saddr { 2a07:54c1:4932::/48, 2001:67c:2564::/48 } tcp dport 4242 accept
    ip saddr { 130.89.0.0/16, 145.126.0.0/16, 87.208.97.61 } tcp dport 4242 accept
  '';

  services.mistServer = {
    enable = true;
    openFirewall = true;
  };
  networking.firewall.allowedTCPPorts = [ 80 ];
}
