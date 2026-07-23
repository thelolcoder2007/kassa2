{ config, pkgs, ... }:

let
  mkdir-sh = pkgs.writeShellScript "mkdir.sh" ''
    BASE_DIR="/var/mistserver/screenshots"

    # Current hour folder, e.g. 2026-07-22_22
    current_dir="$(date +%Y-%m-%d_%H)"

    # Next hour folder — computed via `date -d`, handles day/month/year rollover correctly
    next_dir="$(date -d '+1 hour' +%Y-%m-%d_%H)"

    mkdir -p "$BASE_DIR/$current_dir" "$BASE_DIR/$next_dir"
  '';

  ffmpeg-sh = pkgs.writeShellScript "ffmpeg.sh" ''
    rtmp_key=$(${pkgs.coreutils-full}/bin/cat ${config.sops.secrets."rtmp_key".path})

    ffmpeg -f v4l2 -video_size 3840x2160 -framerate 60 -i /dev/video0 \
      -map 0:v -c:v libx264 -preset ultrafast -tune zerolatency -b:v 100000k -g 60 -x264opts repeat_headers=1 -f rtsp "rtsp://127.0.0.1:5554/$rtmp_key" \
      -map 0:v -c:v libx264 -preset ultrafast -tune zerolatency -qp 0 -f matroska "/var/mistserver/recordings/sntpings-$(date +%Y-%m-%d_%H-%M-%S).mkv" \
      -map 0:v -vf fps=1 -f image2 -strftime 1 "/var/mistserver/screenshots/%Y-%m-%d_%H/%M_%S.png"
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
        OnCalendar = "hourly";
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
        ];
        description = "Stream /dev/video0 to Mistserver";
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
