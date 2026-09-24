{ lib, pkgs, ... }:

let
  ffmpeg-sh = pkgs.writeShellScript "ffmpeg.sh" ''

    # ${lib.getExe pkgs.ffmpeg} -init_hw_device vaapi=va:/dev/dri/renderD128 -filter_hw_device va \
    #  	-f rawvideo -pix_fmt yuyv422 -video_size 3840x2160 -framerate 60 -i /dev/urandom \
    #  	-vf 'hwupload' -c:v hevc_vaapi -low_power 1 \
    #    -rc_mode CBR -b:v 30M -maxrate 30M -bufsize 30M -g 120 \
    #    -f hls -hls_time 2 -hls_list_size 5 -hls_flags delete_segments /run/mistserver/livestream.m3u8
    #    -f v4l2 -input_format nv12 -video_size 3840x2160 -framerate 60 -i /dev/video0 \
    ${lib.getExe pkgs.ffmpeg} -init_hw_device vaapi=va:/dev/dri/renderD128 -filter_hw_device va \
      -f rawvideo -pix_fmt yuyv422 -video_size 3840x2160 -framerate 60 -i /dev/urandom \
      -filter_complex "[0:v]split=2[a][b];[a]hwupload,split=2[live];[b]fps=1[pics]" \
      -map "[live]" -c:v av1_vaapi \
        -rc_mode CBR -b:v 30M -maxrate 30M -bufsize 30M -g 120 \
        -f hls -hls_time 2 -hls_list_size 5 -hls_flags delete_segments /run/mistserver/livestream.m3u8 \
      -map "[pics]" -f image2 -strftime 1 "/run/mistserver/%S.png" \
      # -map "[rec]" -c:v av1_vaapi \
      #   -rc_mode CQP -g 120 \
      #   -f matroska "/var/lib/ffmpeg/sntpings-recording-$(date +%Y-%m-%d_%H-%M-%S).mkv"
  '';
  ffmpeg-remove = pkgs.writeShellScript "remove-hls.sh" ''
    rm -f /run/mistserver/livestream*.ts
    rm -f /run/mistserver/livestream.m3u8
  '';
  # ffmpeg-mkv = pkgs.writeShellScript "ffmpeg-mkv.sh" ''
  #   ffmpeg -init_hw_device vaapi=va:/dev/dri/renderD128 -filter_hw_device va \
  #   -f rawvideo -f v4l2 -input_format nv12 -video_size 3840x2160 -framerate 60 -i /dev/video0 \
  #   -vf 'hwupload' -c:v hevc_vaapi /var/lib/ffmpeg/sntpings-recording-$(date +%Y-%m-%d_%H-%M-%S).mkv
  # '';
in
{
  imports = [
    ../base/sops.nix
  ];

  hardware.graphics.extraPackages = with pkgs; [
    amdvlk
    libvdpau-va-gl
  ];

  systemd = {
    tmpfiles.rules = [
      "d /run/mistserver 0755 root root -"
      "d /var/lib/ffmpeg 0755 root root -"
      "L /run/mistserver/README.txt - - - - /var/mistserver/README.txt"
      "L /run/mistserver/robots.txt - - - - /var/mistserver/robots.txt"
    ];
    services.ffmpeg-stream = {
      after = [
        "network.target"
        "mistserver.service"
      ];
      description = "Stream /dev/video0 to Mistserver, MKV and PNG";
      wantedBy = [ "multi-user.target" ];
      path = [ pkgs.ffmpeg ];
      serviceConfig = {
        ReadWritePaths = [
          "/run/mistserver"
          "/var/lib/ffmpeg"
        ];
        Type = "simple";
        Restart = "always";
        RestartSec = 2;
        TasksMax = "infinity";
        TimeoutStopSec = 8;
        ExecStart = ffmpeg-sh;
        User = "root";
        Group = "root";
        ExecStopPost = ffmpeg-remove;
      };
    };
  };
}
