{
  config,
  inputs,
  lib,
  pkgs,
  ...
}:
{
  imports = [
    ../base/sops.nix
    inputs.bart-pkgs.nixosModules.mistserver
  ];
  users = {
    groups.mistserver = { };
    users.mistserver = {
      isSystemUser = true;
      extraGroups = [ config.users.groups.nginx.name ];
      group = "mistserver";
    };
  };
  sops.secrets."mistserver-config" = {
    owner = "mistserver";
    group = "mistserver";
    reloadUnits = [
      "mistserver.service"
    ];
  };

  networking.firewall.extraInputRules = ''
    ip6 saddr { 2a07:54c1:4932::/48, 2001:67c:2564::/48 } tcp dport 4343 accept
    ip saddr { 130.89.0.0/16, 145.126.0.0/16, 87.208.98.246 } tcp dport 4343 accept
  '';

  services.mistserver = {
    enable = true;
    openFirewall = true;
    configFile = config.sops.secrets."mistserver-config".path;
    settings = {
      auto_push = null;
      bandwidth = {
        exceptions = [
          "::1"
          "127.0.0.0/8"
          "10.0.0.0/8"
          "192.168.0.0/16"
          "172.16.0.0/12"
        ];
      };
      config = {
        accesslog = "LOG";
        controller = {
          interface = null;
          port = null;
          username = null;
        };
        debug = null;
        defaultStream = null;
        prometheus = null;
        protocols = [
          { connector = "AAC"; }
          { connector = "CMAF"; }
          { connector = "DTSC"; }
          { connector = "EBML"; }
          { connector = "FLAC"; }
          { connector = "FLV"; }
          { connector = "H264"; }
          { connector = "HDS"; }
          { connector = "HLS"; }
          { connector = "HTTP"; }
          { connector = "HTTPTS"; }
          { connector = "JPG"; }
          { connector = "JSON"; }
          { connector = "MP3"; }
          { connector = "MP4"; }
          { connector = "OGG"; }
          { connector = "RTMP"; }
          { connector = "RTSP"; }
          { connector = "SDP"; }
          { connector = "SubRip"; }
          { connector = "TSSRT"; }
          { connector = "WAV"; }
          { connector = "WSRaw"; }
          { connector = "WebRTC"; }
          {
            cert = [ "${config.security.acme.certs."bergpad.nationalespeeltuin.nl".directory}/fullchain.pem" ];
            connector = "HTTPS";
            key = [ "${config.security.acme.certs."bergpad.nationalespeeltuin.nl".directory}/key.pem" ];
            port = 4433;
            pubaddr = [ "https://bergpad.nationalespeeltuin.nl:4433" ];
          }
        ];
        serverid = "kassa2.nationalespeeltuin.nl";
        sessionInputMode = 15;
        sessionOutputMode = 15;
        sessionStreamInfoMode = 1;
        sessionUnspecifiedMode = 0;
        sessionViewerMode = 14;
        tknMode = 15;
        triggers = null;
        trustedproxy = [ ];
        weights = {
          abr = 20;
          bw = 20;
          control = 20;
          cpu_server = 20;
          cpu_viewer_batt = 100;
          cpu_viewer_pwrd = 60;
          latency = 50;
          permissibility = 20;
          recovery = 80;
          stability = 50;
          ttff = 50;
        };
      };
      extwriters = null;
      jwks = null;
      push_settings = {
        maxspeed = 0;
        wait = 3;
      };
      streams = {
        recv-tcp = {
          name = "recv-tcp";
          processes = [ ];
          source = "push://invalid,host";
          stop_sessions = false;
          tags = [ ];
        };
      };
      ui_settings = {
        HTTPUrl = "http://bergpad.nationalespeeltuin.nl:8080/";
        connections_filter = {
          hosts = [ ];
          protocols = [ ];
          sessids = [ ];
          streams = [ ];
        };
        sort_autopushes = {
          by = "Target";
          dir = 1;
        };
        sort_pushes = {
          by = "Statistics";
          dir = 1;
        };
        sortstreams = {
          by = "name";
          dir = 1;
        };
      };
      variables = null;
    };
  };
  systemd.services."mistserver" =
    let
      cfg = config.services.mistserver;

      runtimeConfigFile = "${cfg.dataDir}/config.json";
      settingsFormat = pkgs.formats.json { };
      settingsJson = settingsFormat.generate "mistserver.json" cfg.settings;
    in
    lib.mkForce {
      after = [ "network.target" ];
      description = "mistserver, a streaming server";
      wantedBy = [ "multi-user.target" ];
      preStart = lib.optionalString (cfg.configFile != null) ''
        ${lib.getExe pkgs.jq} -s 'reduce .[] as $obj ({}; . * $obj)' ${cfg.configFile} ${settingsJson} > ${runtimeConfigFile}
        chmod 0660 ${runtimeConfigFile}
      '';
      script = ''
        ${lib.getExe cfg.package} -c ${runtimeConfigFile}
      '';
      serviceConfig = {
        Type = "simple";
        Restart = "always";
        DynamicUser = true;
        RestartSec = 2;
        TimeoutStopSec = 8;
        TasksMax = "infinity";
        StateDirectory = "mistserver";
        UMask = "0027";
        MemoryDenyWriteExecute = true;
        PrivateDevices = true;
        PrivateTmp = true;
        ProtectHome = true;
        ProtectControlGroups = true;
        RestrictSUIDSGID = true;
        RestrictRealtime = true;
        LockPersonality = true;
        ProtectKernelLogs = true;
        ProtectKernelTunables = true;
        ProtectHostname = true;
        ProtectKernelModules = true;
        PrivateUsers = true;
        ProtectClock = true;
        SystemCallArchitectures = "native";
        SystemCallErrorNumber = "EPERM";
        SystemCallFilter = "@system-service";
      };
    };
}
