{ config, lib, ... }:

{
  services.prometheus = {
    enable = true;
    exporters = {
      fail2ban.enable = true;
      nginx = {
        enable = true;
        scrapeUri = "http://localhost/stub_status";
      };
      node-cert = {
        enable = true;
        paths = [ "${config.security.acme.certs."bergpad.nationalespeeltuin.nl".directory}/*" ];
      };
      node = {
        enable = true;
        enabledCollectors = [
          "systemd"
          "sysctl"
          "filesystem"
          "perf"
        ];
      };
      systemd = {
        extraFlags = [ "--systemd.collector.enable-restart-count" ];
        enable = true;
      };
    };
  };
  networking.firewall.extraInputRules =
    let
    promExportersEnabled = lib.filterAttrs (
      name: exporter:
        let
          result = builtins.tryEval (
            builtins.isAttrs exporter && exporter ? enable && exporter.enable
          );
        in
        result.success && result.value
    ) config.services.prometheus.exporters;

    allowedPorts = lib.pipe promExportersEnabled [
      (lib.mapAttrsToList (
        _: exporter:
        let result = builtins.tryEval (toString exporter.port);
        in if result.success then result.value else null
      ))
      (builtins.filter (p: p != null))
    ];
    in
    ''
  		ip saddr 194.171.96.49 tcp dport {${builtins.concatStringsSep ", " allowedPorts}} accept comment "Allow Prometheus from Jetse's Prometheus daemon";
   	'';

  boot.kernel.sysctl."kernel.perf_event_paranoid" = 0; # Prometheus recommends it, I don't really know what it does
}
