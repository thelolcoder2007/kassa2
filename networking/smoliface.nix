{ config, host, ... }:

let
  firewall_ip_addr4 = "145.220.6.1";
  firewall_ip_addr6 = "fe80::ee0d:9aff:fe5f:44c8";
  inherit (host) ip_addr4 ip_addr6 hostName;

  DNS_server = {
    # DNS provided by Church of Cyberology (which is really close in terms of BGP hops)
    ip_addr4 = "192.42.116.9";
    ip_addr6 = "2001:67c:e60:c0c::53:2";
  };
in
{
  services.udev.extraRules = ''
    SUBSYSTEM=="net", ACTION=="add", KERNELS=="0000:07:00.0", NAME="smoliface"
  '';
  networking = {
    inherit hostName;
    domain = "nationalespeeltuin.nl";
    useDHCP = false;

    firewall.enable = true;

    nftables.enable = true;

    interfaces.smoliface = {
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
      interface = "smoliface";
    };
    defaultGateway6 = {
      address = firewall_ip_addr6;
      interface = "smoliface";
    };
  };

  # Fuck resolvconf, all my homies hate resolvconf. Real users write their own resolv.conf
  networking.resolvconf.enable = false;
  environment.etc."resolv.conf" = {
    enable = true;
    text = ''
      nameserver ${DNS_server.ip_addr4}
      nameserver ${DNS_server.ip_addr6}
      search ${config.networking.domain}
      options edns0 inet6
    '';
  };
}
