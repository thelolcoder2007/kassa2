{
  config,
  lib,
  pkgs,
  ...
}:
let
  cfg = config.services.mistServer;
  inherit (lib)
    mkEnableOption
    mkOption
    types
    mkIf
    ;
in
{
  options.services.mistServer = {
    enable = mkEnableOption "Enable mistserver, a streaming server";
    package = mkOption {
      description = "Package to use mistserver with";
      default = pkgs.callPackage ./package.nix { };
      type = types.package;
    };
    openFirewall = mkEnableOption "Open the firewall for Mistserver";
    configFile = mkOption {
      type = types.str;
      description = "The config file (as string)";
      default = "";
    };
  };

  config = {
    systemd = {
      tmpfiles.rules = [
        "f+ /etc/mistserver.conf 0640 root root \"${cfg.configFile}\""
      ];
      services."mistserver" = {
        after = [ "network.target" ];
        description = "MistServer";
        wantedBy = [ "multi-user.target" ];
        serviceConfig = {
          Type = "simple";
          Restart = "always";
          RestartSec = 2;
          TasksMax = "infinity";
          TimeoutStopSec = 8;
        };
        script = "${cfg.package}/bin/MistController -c /etc/mistserver.conf";
        postStop = "${lib.getExe pkgs.bash} -c \"${pkgs.coreutils-full}/bin/rm -f /dev/shm/*Mst*\"";
      };
    };
    networking.firewall = mkIf cfg.openFirewall rec {
      allowedTCPPorts = [
        0443 # HTTPS/HSLS
        1935 # RTMP
        4200 # DTSC
        4433 # Extra HTTPS/HSLS
        5554 # RTSP
        8080 # HTTP/HLS
      ];
      allowedUDPPorts = allowedTCPPorts ++ [
        8889 # SRT
        18203 # WebRTC
      ];
    };
  };
}
