{
  config,
  lib,
  pkgs,
  ...
}:
let
  gstreamer-sh = pkgs.writeShellScript "gstreamer.sh" ''
    sleep 5
    ${pkgs.gst_all_1.gstreamer}/bin/gst-launch-1.0 -v v4l2src device=/dev/video0 do-timestamp=true \
        ! video/x-raw,width=3840,height=2160,framerate=60/1 ! tee name=t \
        t. ! queue max-size-bytes=0 max-size-buffers=600 max-size-time=0 leaky=downstream \
           ! videoconvert ! x264enc bitrate=100000 speed-preset=ultrafast tune=zerolatency key-int-max=60 \
           ! h264parse ! rtspclientsink protocols=tcp location="rtsp://127.0.0.1:5554/$(${pkgs.coreutils-full}/bin/cat ${
             config.sops.secrets."rtmp_key".path
           })" \
        t. ! queue max-size-bytes=0 max-size-buffers=10 max-size-time=0 leaky=downstream \
           ! videorate ! video/x-raw,framerate=1/1 ! videoconvert ! pngenc \
           ! multifilesink location=/var/mistserver/screenshots/sntpings-$(date +%Y-%m-%d_%H-%M-%S)+%06d.png async=false \
        t. ! queue max-size-bytes=0 max-size-buffers=600 max-size-time=0 leaky=downstream \
           ! videoconvert ! x264enc pass=quant quantizer=0 speed-preset=ultrafast \
           ! matroskamux ! filesink location=/var/mistserver/recordings/sntpings-$(date +%Y-%m-%d_%H-%M-%S).mkv \
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
      Environment = "GST_PLUGIN_SYSTEM_PATH_1_0=\"${pkgs.gst_all_1.gstreamer.out}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-base}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-good}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-bad}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-plugins-ugly}/lib/gstreamer-1.0:${pkgs.gst_all_1.gst-rtsp-server}/lib/gstreamer-1.0\"";
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
        gst-rtsp-server
        gstreamer
        gstreamer.out
        # keep-sorted end
      ]
      ++ [
        pkgs.v4l2-relayd
      ];
  };
}
