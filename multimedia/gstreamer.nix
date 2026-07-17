{
  config,
  lib,
  pkgs,
  ...
}:
let
  gstreamer-sh = pkgs.writeShellScript "gstreamer.sh" ''
    sleep 5
    ${pkgs.gst_all_1.gstreamer}/bin/gst-launch-1.0 -v v4l2src device=/dev/video0 \
        ! video/x-raw,format=Y444,width=3840,height=2160,frames=60/1 ! tee name=t \
        t. ! queue ! x264enc quantizer=0 speed-preset=ultrafast \
        tune=zerolatency ! h264parse \
        ! srtsink uri="srt://127.0.0.1:8889?streamid=$(${pkgs.coreutils-full}/bin/cat ${
          config.sops.secrets."rtmp_key".path
        })" \
        t. ! queue ! avenc_ffv1 ! matroskamux ! filesink location=/var/mistserver/recording/sntpings-$(date +%Y-%m-%d_%H-%M-%S).mkv \
        t. ! queue ! videorate ! video/x-raw,framerate=1/1 ! videoconvert ! pngenc ! multifilesink location=sntpings-$(date +%Y-%m-%d_%H-%M-%S)-frame%06d.png

  '';
in
{
  imports = [
    ../base/sops.nix
  ];
  environment.systemPackages = config.systemd.services."gstreamer-stream".path;

  sops.secrets.rtmp_key = {
    restartUnits = [ "gstreamer-stream.service" ];
  };

  systemd.services."gstreamer-stream" = {
    after = [
      "network.target"
      "mistserver.service"
    ];
    description = "Stream /dev/video0 to RTMP";
    wantedBy = [ "multi-user.target" ];
    serviceConfig = {
      Environment = "GST_PLUGIN_SYSTEM_PATH_1_0=\"${pkgs.gst_all_1.gstreamer.out}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-base}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-good}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-bad}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-ugly}/lib/gstreamer-1.0\"";
      Type = "simple";
      Restart = "always";
      RestartSec = 2;
      TasksMax = "infinity";
      TimeoutStopSec = 8;
      ExecStart = "${lib.getExe pkgs.bash} ${gstreamer-sh}";
      User = "root";
      Group = "root";

    };
    path =
      with pkgs.gst_all_1;
      [
        # keep-sorted start
        gst-libav
        gst-plugins-bad
        gst-plugins-base
        gst-plugins-good
        gst-plugins-ugly
        gstreamer
        # keep-sorted end
      ]
      ++ [
        pkgs.v4l2-relayd
      ];
  };
}
