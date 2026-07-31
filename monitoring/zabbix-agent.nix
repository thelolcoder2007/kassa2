{
  config,
  lib,
  pkgs,
  ...
}:

{
  imports = [
    ../base/sops.nix
  ];
  sops.secrets."zabbix-agent-PSK" = {
    owner = "zabbix-agent";
    group = "zabbix-agent";
  };
  services.zabbixAgent = {
    enable = true;
    server = "87.208.98.246,2a07:54c1:4932::/48,127.0.0.1,::1";
    settings = {
      Hostname = config.networking.hostName;
      UserParameter =
        let
          nix-update-script-path = pkgs.writeShellScript "zabbix-update-check.sh" ''
            cp /etc/nixos/kassa2/flake.* /tmp/ >/dev/null
            cd /tmp
            ${lib.getExe pkgs.nix} flake update --output-lock-file /dev/stdout 2> /dev/null | ${lib.getExe pkgs.git} diff /dev/stdin /tmp/flake.lock 2> /dev/null | wc -l
            rm /tmp/flake.*
          '';
        in
        [
          "nix_updates,${nix-update-script-path}"
        ];
      TLSConnect = "psk";
      TLSAccept = "psk";
      TLSPSKFile = config.sops.secrets."zabbix-agent-PSK".path;
      TLSPSKIdentity = "PSK 001";
    };
    package = pkgs.zabbix74.agent2;
  };
  users.users.zabbix-agent.extraGroups = [ "nginx" ]; # For certs access
  networking.firewall.extraInputRules = ''
    ip saddr 87.208.98.246 tcp dport 10050 accept
    ip6 saddr 2a07:54c1:4932:111::/64 tcp dport 10050 accept
  '';
  systemd.services.zabbix-agent.serviceConfig.ReadOnlyPaths = [ "/etc/nixos/kassa2" ];
  environment.etc."current-system-packages".text =
    let
      packages = map (p: "${p.name}") config.environment.systemPackages;
      sortedUnique = builtins.sort builtins.lessThan (lib.lists.unique packages);
      formatted = builtins.concatStringsSep "\n" sortedUnique;
    in
    formatted;

  users.users.zabbix-agent.home = lib.mkForce "/tmp";
}
