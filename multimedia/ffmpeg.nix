{
  config,
  lib,
  pkgs,
  ...
}:

let
  ffmpeg-sh = pkgs.writeShellScript "ffmpeg.sh" ''
        rtmp_key=$(${lib.getExe' pkgs.coreutils-full "cat"} ${config.sops.secrets."rtmp_key".path})

        # ${lib.getExe pkgs.ffmpeg} -init_hw_device vaapi=va:/dev/dri/renderD128 -filter_hw_device va \
        #     -f v4l2 -input_format nv12 -video_size 3840x2160 -framerate 60 -i /dev/video0 \
        #     -vf 'hwupload' \
        #     -c:v hevc_vaapi -profile:v main -quality 4 -bf 2 \
        #     -rc_mode VBR -b:v 30M -maxrate 33M -bufsize 6M -g 30 \
        #     -f hls -hls_time 2 -hls_list_size 5 -hls_flags delete_segments /run/mistserver/livestream.m3u8
        # ${lib.getExe pkgs.ffmpeg} -init_hw_device vaapi=va:/dev/dri/renderD128 -filter_hw_device va \
        # 	-f rawvideo -pix_fmt nv12 -video_size 3840x2160 -framerate 60 -i /dev/urandom \
    	   #  -vf 'hwupload' -c:v hevc_vaapi -profile:v main -quality 4 -bf 2 \
    	   #  -rc_mode CBR -b:v 30M -maxrate 30M -bufsize 6M -g 120 \
    	   #  -f hls -hls_time 2 -hls_list_size 5 -hls_flags delete_segments /run/mistserver/livestream.m3u8
    		${lib.getExe pkgs.ffmpeg} -init_hw_device vaapi=va:/dev/dri/renderD128 -filter_hw_device va \
    			-f rawvideo -pix_fmt nv12 -video_size 3840x2160 -framerate 60 -i /dev/urandom \
    			-vf 'hwupload' -c:v hevc_vaapi \
    			-rc_mode CBR -b:v 30M -maxrate 30M -bufsize 6M -g 120 \
    			-f hls -hls_time 2 -hls_list_size 5 -hls_flags delete_segments /run/mistserver/livestream.m3u8
  '';
  ffmpeg-remove = pkgs.writeShellScript "remove-hls.sh" ''
    		rm /run/mistserver/livestream*.ts
    		rm /run/mistserver/livestream.m3u8
  '';
  ffmpeg-mkv = pkgs.writeShellScript "ffmpeg-mkv.sh" ''
    		sleep 10
    		ffmpeg -hwaccel vaapi -hwaccel_output_format vaapi -i /run/mistserver/livestream.m3u8 \
    		"/var/lib/ffmpeg/recordings/livestream-$(date +%Y-%m-%d_%H-%M-%S).mkv"
  '';
in
{
  # ffmpeg -f rawvideo -pix_fmt yuv420p -video_size 3840x2160 -framerate 60 -i /dev/urandom
  # -map 0:v -vf fps=1 -f image2 -y -strftime 1 "/run/mistserver/%S.png"
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
      "d /run/mistserver-recordings 0755 root root -"
      "d /run/mistserver 0755 root root -"
      "d /var/lib/ffmpeg/recordings 0755 root root -"
      "L /run/mistserver/README.txt - - - - /var/mistserver/README.txt"
      "L /run/mistserver/robots.txt - - - - /var/mistserver/robots.txt"
    ];
    services = {
      ffmpeg-mkv = {
        after = [
          "network.target"
          "ffmpeg-create-dirs.service"
          "ffmpeg-stream.service"
        ];
        description = "Stream /dev/video0 to MKV";
        wantedBy = [ "multi-user.target" ];
        path = [ pkgs.ffmpeg ];
        serviceConfig = {
          ReadWritePaths = "/var/lib/ffmpeg/recordings";
          ReadOnlyPaths = "/run/mistserver";
          Type = "simple";
          Restart = "always";
          RestartSec = 2;
          TasksMax = "infinity";
          TimeoutStopSec = 8;
          ExecStart = ffmpeg-mkv;
          User = "root";
          Group = "root";
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
