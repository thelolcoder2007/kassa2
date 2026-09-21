{
  config,
  lib,
  pkgs,
  ...
}:

let
  ffmpeg-sh = pkgs.writeShellScript "ffmpeg.sh" ''
    rtmp_key=$(${lib.getExe' pkgs.coreutils-full "cat"} ${config.sops.secrets."rtmp_key".path})

     	# -f rawvideo -pix_fmt nv12 -video_size 3840x2160 -framerate 60 -i /dev/urandom \
    ${lib.getExe pkgs.ffmpeg} -init_hw_device vaapi=va:/dev/dri/renderD128 -filter_hw_device va \
    	-f v4l2 -input_format nv12 -video_size 3840x2160 -framerate 60 -i /dev/video0 \
    	-map 0:v -vf 'hwupload' -c:v hevc_vaapi \
    	-rc_mode CBR -b:v 30M -maxrate 30M -bufsize 6M -g 120 \
    	-f hls -hls_time 2 -hls_list_size 5 -hls_flags delete_segments /run/mistserver/livestream.m3u8 \
      -map 0:v -vf 'hwupload' -c:v hevc_vaapi \
      /var/lib/ffmpeg/sntpings-recording-$(date +%Y-%m-%d_%H-%M-%S).mkv
  '';
  ffmpeg-remove = pkgs.writeShellScript "remove-hls.sh" ''
    rm /run/mistserver/livestream*.ts
    rm /run/mistserver/livestream.m3u8
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

  sops.secrets."rtmp_key" = { };
  systemd = {
    tmpfiles.rules = [
      "d /run/mistserver 0755 root root -"
      "d /var/lib/ffmpeg 0755 root root -"
      "L /run/mistserver/README.txt - - - - /var/mistserver/README.txt"
      "L /run/mistserver/robots.txt - - - - /var/mistserver/robots.txt"
    ];
    services = {
      # ffmpeg-mkv = {
      #   after = [
      #     "network.target"
      #     "ffmpeg-create-dirs.service"
      #     "ffmpeg-stream.service"
      #   ];
      #   description = "Stream /dev/video0 to MKV";
      #   wantedBy = [ "multi-user.target" ];
      #   path = [ pkgs.ffmpeg ];
      #   serviceConfig = {
      #     ReadWritePaths = "/var/lib/ffmpeg/recordings";
      #     ReadOnlyPaths = "/run/mistserver";
      #     Type = "simple";
      #     Restart = "always";
      #     RestartSec = 2;
      #     TasksMax = "infinity";
      #     TimeoutStopSec = 8;
      #     ExecStart = ffmpeg-mkv;
      #     User = "root";
      #     Group = "root";
      #   };
      # };
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
          ReadWritePaths = "/run/mistserver";
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
  };
}
