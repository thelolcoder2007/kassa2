{ config, pkgs, ... }:

let
  ffmpeg-sh = pkgs.writeShellScript "ffmpeg.sh" ''
    rtmp_key=$(${pkgs.coreutils-full}/bin/cat ${config.sops.secrets."rtmp_key".path})

    # The first branch might be a bad idea, since this is quite a lot of data.
    # Let's hope that Mistserver compresses it into tiny little pieces so end devices don't consume 100mbit/s just viewing this livestream (it did...)
    ffmpeg -f v4l2 -video_size 3840x2160 -framerate 60 \
    -thread_queue_size 1024 -i /dev/video0 \
    -map 0:v -c:v libx265 -g 120 -b:v 100M -maxrate 100M -bufsize 200M -f rtsp "rtsp://127.0.0.1:5554/$rtmp_key" \
    -map 0:v -vf fps=1 -f image2 -y -strftime 1 "/run/mistserver/%S.png" \
    -map 0:v -c:v libx264 -preset ultrafast -qp 0 -threads 0 -f matroska "/run/mistserver-recordings/sntpings-$(date +%Y-%m-%d_%H-%M-%S).mkv"
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
