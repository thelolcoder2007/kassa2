{ config, pkgs, ... }:

let
  mkdir-sh = pkgs.writeShellScript "mkdir.sh" ''
    BASE_DIR="/var/mistserver/screenshots"

    current_dir="$(date +%Y-%m-%d_%H)"

    next_dir="$(date -d '+1 hour' +%Y-%m-%d_%H)"

    mkdir -p "$BASE_DIR/$current_dir" "$BASE_DIR/$next_dir"
  '';

  ffmpeg-sh = pkgs.writeShellScript "ffmpeg.sh" ''
    rtmp_key=$(${pkgs.coreutils-full}/bin/cat ${config.sops.secrets."rtmp_key".path})

    # The first branch might be a bad idea, since this is quite a lot of data.
    # Let's hope that Mistserver compresses it into tiny little pieces so end devices don't consume 100mbit/s just viewing this livestream
    ffmpeg -f v4l2 -video_size 3840x2160 -framerate 60 \
    -thread_queue_size 1024 -i /dev/video0 \
    -map 0:v -c:v libx264 -preset veryfast -tune zerolatency -g 120 -f rtsp "rtsp://127.0.0.1:5554/$rtmp_key" \
    -map 0:v -vf fps=1 -f image2 -strftime 1 "/var/mistserver/screenshots/%Y-%m-%d_%H/%M_%S.png" \
    -map 0:v -c:v libx264 -preset ultrafast -qp 0 -threads 0 -f matroska "/var/mistserver/recordings/sntpings-$(date +%Y-%m-%d_%H-%M-%S).mkv"
  '';
in
{
  imports = [
    ../base/sops.nix
  ];
  sops.secrets."rtmp_key" = { };
  systemd = {
    timers."ffmpeg-create-dirs" = {
      description = "Run ensure-snapshot-dirs every hour";
      timerConfig = {
        OnCalendar = "*-*-* *:30:00"; # Every hour at xx:30
        Unit = "ffmpeg-create-dirs.service";
        Persistent = true;
      };
      wantedBy = [ "timers.target" ];
    };
    services = {
      ffmpeg-create-dirs = {
        description = "Ensure current and next hour screenshot directories exist";
        serviceConfig = {
          ExecStart = mkdir-sh;
          Type = "oneshot";
        };
      };
      ffmpeg-stream = {
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
  };
}
