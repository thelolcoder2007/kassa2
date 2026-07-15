{ host, ... }:

let
  firewall_ip_addr4 = "145.220.6.1";
  firewall_ip_addr6 = "2001:67c:6ec:abba:145:220:6:1";
  inherit (host) ip_addr4 ip_addr6 hostName;

  DNS_server = {
    ip_addr4 = "9.9.9.9";
    ip_addr6 = "2606:4700:4700::1111";
  };
in
{
  networking = {
    inherit hostName;
    domain = "nationalespeeltuin.nl";
    useDHCP = false;

    firewall.enable = true;

    nftables.enable = true;

    interfaces.enp3s0f0 = {
      ipv6.addresses = [
        {
          address = ip_addr6;
          prefixLength = 64;
        }
      ];
      ipv4 = {
        addresses = [
          {
            address = ip_addr4;
            prefixLength = 24;
          }
        ];
      };
    };

    defaultGateway = {
      address = firewall_ip_addr4;
      interface = "enp3s0f0";
    };
    defaultGateway6 = {
      address = firewall_ip_addr6;
      interface = "enp3s0f0";
    };
  };

  # Fuck resolvconf, all my homies hate resolvconf. Real users write their own resolv.conf
  networking.resolvconf.enable = false;
  environment.etc."resolv.conf" = {
    enable = true;
    text = ''
      nameserver ${DNS_server.ip_addr4}
      nameserver ${DNS_server.ip_addr6}
      search nationalespeeltuin.nl
      options edns0 inet6
    '';
  };
}
