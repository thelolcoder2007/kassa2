{ config, ... }:
{
  services.openssh = {
    enable = true;
    listenAddresses = [
      {
        addr = (builtins.elemAt config.networking.interfaces.ens192.ipv4.addresses 0).address;
      }
      {
        addr = (builtins.elemAt config.networking.interfaces.ens192.ipv6.addresses 0).address;
      }
    ];
  };
  services.fail2ban = {
    enable = true;
    bantime = "1h";
    bantime-increment.enable = true;
    maxretry = 10;
  };
}
