{ config, lib, pkgs, ... }:

let
  ffmpeg-sh = pkgs.writeShellScript "ffmpeg.sh" ''
    rtmp_key=$(${lib.getExe' pkgs.coreutils-full "cat"} ${config.sops.secrets."rtmp_key".path})

    ffmpeg -f v4l2 -video_size 3840x2160 -framerate 60  -i /dev/video0 \
    -map 0:v -c:v libx264 -crf 20 -preset ultrafast -g 30 -threads 1 -f rtsp "rtsp://127.0.0.1:5554/$rtmp_key"
  '';
in
{
  imports = [
    ../base/sops.nix
  ];
  sops.secrets."rtmp_key" = { };
  systemd = {
    tmpfiles.rules = [
      "d /run/mistserver-recordings 0755 root root -"
      "d /run/mistserver 0755 root root -"
      "L /run/mistserver/README.txt - - - - /var/mistserver/README.txt"
      "L /run/mistserver/robots.txt - - - - /var/mistserver/robots.txt"
    ];
    services.ffmpeg-stream = {
      after = [
        "network.target"
        "mistserver.service"
        "ffmpeg-create-dirs.service"
      ];
      description = "Stream /dev/video0 to Mistserver, MKV and PNG";
      wantedBy = [ "multi-user.target" ];
      path = [ pkgs.ffmpeg ];
      serviceConfig = {
        Type = "simple";
        Restart = "always";
        RestartSec = 2;
        TasksMax = "infinity";
        TimeoutStopSec = 8;
        ExecStart = ffmpeg-sh;
        User = "root";
        Group = "root";
      };
    };
  };
}
