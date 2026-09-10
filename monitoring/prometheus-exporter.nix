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
        _: value: value.enable
      ) config.services.prometheus.exporters;
      allowedPorts = map (exporter: exporter.port) promExportersEnabled;
    in
    ''
      		ip saddr 0.0.0.0 tcp dport {${builtins.concatStringsSep ", " allowedPorts}} accept comment "Allow Prometheus from Jetse's Prometheus daemon";
      		ip6 saddr :: tcp dport {${builtins.concatStringsSep ", " allowedPorts}} accept comment "Allow Prometheus from Jetse's Prometheus daemon";
      	'';

  boot.kernel.sysctl."kernel.perf_event_paranoid" = 0; # Prometheus recommends it, I don't really know what it does
}
