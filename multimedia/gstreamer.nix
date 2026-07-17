{
  config,
  lib,
  pkgs,
  ...
}:
let
  gstreamer-sh = pkgs.writeShellScriptBin "gstreamer.sh" ''
    ${pkgs.gst_all_1.gstreamer}/bin/gst-launch-1.0 -v v4l2src device=/dev/video0 \
    ! videoconvert ! x264enc quantizer=0 speed-preset=ultrafast tune=zerolatency \
    ! h264parse ! mpegtsmux \
    ! srtsink uri="srt://127.0.0.1:8889?streamid=$(${pkgs.coreutils-full}/bin/cat ${
      config.sops.secrets."rtmp_key".path
    })"
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
      ExecStart = "${lib.getExe pkgs.bash} ${lib.getExe gstreamer-sh}";
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
