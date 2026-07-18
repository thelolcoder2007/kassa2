{
  config,
  lib,
  pkgs,
  ...
}:

let
  gstreamer-py = pkgs.writers.writePython3 "gstreamer.py" {
    libraries = with pkgs.python3Packages; [
      gst-python
      pygobject3
    ];
    doCheck = false;
  } (builtins.readFile ./gstreamer.py);
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
    environment = {
      GI_TYPELIB_PATH = lib.makeSearchPath "lib/girepository-1.0" (
        with pkgs;
        [
          gobject-introspection
          gst_all_1.gstreamer
          gst_all_1.gstreamer.out
          gst_all_1.gst-plugins-base
          gst_all_1.gst-plugins-good
          gst_all_1.gst-plugins-bad
          gst_all_1.gst-plugins-ugly
          gst_all_1.gst-rtsp-server
        ]
      );
      GST_PLUGIN_SYSTEM_PATH_1_0 = lib.makeSearchPath "lib/gstreamer-1.0" (
        with pkgs.gst_all_1;
        [
          gstreamer
          gstreamer.out
          gst-plugins-base
          gst-plugins-good
          gst-plugins-bad
          gst-plugins-ugly
          gst-rtsp-server
        ]
      );
      GST_DEBUG = "3";
    };
    serviceConfig = {

      Type = "simple";
      Restart = "always";
      RestartSec = 2;
      TasksMax = "infinity";
      TimeoutStopSec = 8;
      ExecStart = gstreamer-py;
      User = "root";
      Group = "root";
    };

    path = with pkgs; [
      # keep-sorted start
      gst_all_1.gst-libav
      gst_all_1.gst-plugins-bad
      gst_all_1.gst-plugins-base
      gst_all_1.gst-plugins-good
      gst_all_1.gst-plugins-ugly
      gst_all_1.gst-rtsp-server
      gst_all_1.gstreamer
      gst_all_1.gstreamer.out
      v4l2-relayd
      # keep-sorted end
    ];
  };
}
